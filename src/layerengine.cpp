#include "layerengine.h"

#include <cassert>

#include "network.h"
#include "partition.h"


bool LayerScheme::isValid() const{
	return totCost.isValid();
}

StdLayerEngine::StdLayerEngine(CoreMapper* _mapper):mapper(_mapper){}

vol_t StdLayerEngine::get_ubuf_size() const{
	return mapper->get_ubuf_size();
}

/**
 * @brief StdLayerEngine::search.
 * Searches partition and placement of each layer.
 * The procedure is as follows
 *  for each partition:
 *      estimate max ubuf usage
 *      search for intra-tile dataflow
 *      calculate ubuf energy (outside tile)
 *      for each placement:
 *		    calculate NoC
 *          update best scheme
 *
 * @return LayerScheme.
 */
LayerScheme StdLayerEngine::search(LNode* curNode) const{
	// The final scheme
	LayerScheme layerSch;

	/* ########## Constant infos ########## */

	// Info extracted from curNode
	const Cluster& cluster = curNode->cluster;
	const Node& layerT = curNode->layert;
	const Layer& layer = layerT.layer();
	const fmap_shape& ofmShape = layer.ofmap_shape();
	const len_t totBatch = LNode::tot_batch;
	const len_t B = curNode->num_batch;
	const bool wgt_B = layerT.hasWgtPrevs();

	const cidx_t numCores = cluster.num_cores();
	const Core::Buffer& ubuf = mapper->core().ubuf();
	const vol_t totUbufSize = ubuf.Size * numCores;

	/* ########## Current scheme ########## */

	SchNode::SchCost curCost;
	CoreMapper::CoreMapping tileSch;
	// Bandwidth is only calculated in the final scheme (Sec. Update optimal scheme)
	NoC noc(false);

	PlaceSch placeSch;
	{
		pos_t* permOrder = new pos_t[numCores];
		placeSch.permuteOrder.reset(permOrder);
		placeSch.ifmLayout = std::make_unique<StdDataLayout>(numCores, permOrder);
		if(layer.weight_size() > 0)
			placeSch.wgtLayout = std::make_unique<StdDataLayout>(numCores, permOrder);
		else
			placeSch.wgtLayout = std::make_unique<StdDataLayout>(0, nullptr);
		placeSch.ofmLayout = std::make_unique<StdULayout>(numCores, permOrder);
		// permOrder = nullptr; // Handled to placeSch.permuteOrder
	}

	PartSch& partSch = placeSch.part;

	/* ########## Search iterations ########## */

	// For ubuf energy
	const energy_t ubufOfm = ofmShape.tot_size(B) * ubuf.RCost;
	energy_t ubufTotal;

	// Minimal cuts on ifmap. Ifmap tile should not use too much ubuf.
	len_t minCuts = 0;
	if(REF_IS_INSTANCE(layer, ConvLayer) && !REF_IS_INSTANCE(layer, GroupConvLayer))
		minCuts = static_cast<len_t>(layer.real_ifmap_shape().tot_size(B) / (totUbufSize*0.8) + 1);

	// Iterator over all valid partitions.
	auto partIter = partEngine.init(numCores, B, layerT, partSch, minCuts);
	if(!partIter){
		// No partition found!
		return layerSch;
	}

	// Iter all partitions.
	do{
		assert(partSch.size() == static_cast<unsigned>(numCores));

		// Init partition
		initLayouts(placeSch, layerT, ofmShape, B);

		// Estimate buffer usage
		vol_t estimatedBuf = ofm_ubuf_vol;
		estimatedBuf += placeSch.ifmLayout->maxRange();
		estimatedBuf += placeSch.wgtLayout->maxRange();
		if(estimatedBuf > ubuf.Size) continue;

		// Search for intra-tile dataflow
		tileSch = mapper->genLayerMap(layer, partSch, B, wgt_B);
		if(!tileSch.cost.is_valid()) continue;
		curCost.energy = tileSch.cost.energy * numCores;
		curCost.time = tileSch.cost.time;

		// Calc ubuf energy
		// TODO: default to not pinning weights.
		energy_t ubufWgt = placeSch.wgtLayout->totalSize() * ubuf.WCost;
		// weight ubuf energy should only count once,
		// thus divided by different batches.
		if(!wgt_B) ubufWgt /= (totBatch / B);

		ubufTotal = ubufWgt + ubufOfm;
		ubufTotal += placeSch.ifmLayout->totalSize() * ubuf.WCost;
		curCost.energy += ubufTotal;

		// Iterate over all placements.
		auto placeIter = placeEngine.init(placeSch);
		// Placement must yield at least one valid scheme
		assert(placeIter);
		do{
			// Init placement
			placeSch.initPlacement(cluster);

			calcNoC(noc, placeSch, curNode);

			cycle_t nocTime = noc.get_time();

			SchNode::SchCost curCostAll = curCost;
			curCostAll.energy += noc.get_cost();
			curCostAll.time = MAX(curCostAll.time, nocTime);

			// Update optimal cost
			if(curCostAll.cost() < layerSch.totCost.cost()){
				layerSch.totCost = curCostAll;
				layerSch.extUbufEnergy = ubufTotal;
				layerSch.tileSch = tileSch;
				layerSch.place.update(std::move(placeSch));
			}
		}while(placeIter.nextPlace(/*curCost.cost()*/));
	}while(partIter.nextPart(/*curCost.cost()*/));

	/* ########## Update optimal scheme ########## */

	if(layerSch.isValid()){
		// Reuse allocated array
		layerSch.place.ifmLayout = std::move(placeSch.ifmLayout);
		layerSch.place.wgtLayout = std::move(placeSch.wgtLayout);
		layerSch.place.ofmLayout = std::move(placeSch.ofmLayout);
		layerSch.place.permuteOrder = std::move(placeSch.permuteOrder);

		/* ##### Re-calculate placement scheme & NoC ##### */

		// Init partition
		initLayouts(layerSch.place, layerT, ofmShape, B);

		// Init placement
		layerSch.place.initPlacement(cluster);

		// Finalize layerSch.place
		layerSch.place.finalize();

		// Update NoC
		calcNoC(layerSch.noc, layerSch.place, curNode);
	}

	return layerSch;
}

void StdLayerEngine::initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const{
	using plen_t = PartSch::partlen_t;

	const PartSch& part = place.part;
	const Layer& layer = layerT.layer();
	const bool fmap_K = layer.fmap_channel_rel();
	const bool hasWgt = layer.weight_size() > 0;
	const bool wgt_B = layerT.hasWgtPrevs();

	// Intervals on each dimension.
	const len_t* arrs[4];
	arrs[0] = part_intv(ofmShape.c, part.K);
	arrs[1] = part_intv(B, part.B);
	arrs[2] = part_intv(ofmShape.h, part.H);
	arrs[3] = part_intv(ofmShape.w, part.W);

	/* ########## Set fmap layouts ########## */

	assert(REF_IS_INSTANCE(place.getOfmL(), StdULayout));
	assert(REF_IS_INSTANCE(place.getIfmL(), StdDataLayout));
	assert(REF_IS_INSTANCE(place.getWgtL(), StdDataLayout));
	auto& ofmLayout = static_cast<StdULayout&>(place.getOfmL());
	auto& ifmLayout = static_cast<StdDataLayout&>(place.getIfmL());
	auto& wgtLayout = static_cast<StdDataLayout&>(place.getWgtL());

	ofmLayout.setDims(part.K, part.B, part.H, part.W);

	if(fmap_K){
		ifmLayout.setBcast(1, 1);
	}else{
		ifmLayout.setBcast(part.K, part.B * part.H * part.W);
	}

	if(hasWgt){
		if(wgt_B){
			wgtLayout.setBcast(part.H * part.W, 1);
		}else{
			wgtLayout.setBcast(part.B * part.H * part.W, 1);
		}
	}

	/* ########## Set rangeArr ########## */

	fmap_range::dim_range kRange, bRange, hRange, wRange;
	const fmap_range emptyRange({0, 0}, {0, 0}, {0, 0}, {0, 0});

	auto* ofmArr = ofmLayout.rangeArr.get();
	auto* ifmArr = ifmLayout.rangeArr.get();
	auto* wgtArr = wgtLayout.rangeArr.get();

	for(plen_t k = 0; k < part.K; ++k){
		kRange = {arrs[0][k], arrs[0][k+1]};
		for(plen_t b = 0; b < part.B; ++b){
			bRange = {arrs[1][b], arrs[1][b+1]};
			for(plen_t h = 0; h < part.H; ++h){
				hRange = {arrs[2][h], arrs[2][h+1]};
				for(plen_t w = 0; w < part.W; ++w){
					wRange = {arrs[3][w], arrs[3][w+1]};

					// Update ofmap rangeArr
					*ofmArr = {kRange, bRange, hRange, wRange};
					vol_t s = (*ofmArr).size();
					ofmLayout.update(*ofmArr);

					// Update ifmap rangeArr
					if(fmap_K || k == 0){
						if(s == 0){
							*ifmArr = emptyRange;
						}else{
							*ifmArr = *ofmArr;
							layer.ofm_to_ifm(*ifmArr);
							ifmLayout.update(*ifmArr);

						}
						++ifmArr;
					}

					// Update weight rangeArr
					if(hasWgt && (wgt_B || b == 0) && h == 0 && w == 0){
						if(s == 0){
							*wgtArr = emptyRange;
						}else{
							*wgtArr = *ofmArr;
							layer.ofm_to_wgt(*wgtArr);
							if(!wgt_B) wgtArr->b = {0, 1};
							wgtLayout.update(*wgtArr);
						}
						++wgtArr;
					}

					++ofmArr;
				}
			}
		}
	}

	assert(ifmArr == ifmLayout.rangeArr.get() + ifmLayout.rangeLength());
	assert(wgtArr == wgtLayout.rangeArr.get() + wgtLayout.rangeLength());
	assert(ofmArr == ofmLayout.rangeArr.get() + ofmLayout.rangeLength());

	// Multiply size of ifmLayout for eltwise
	auto* eltLayer = dynamic_cast<const EltwiseLayer*>(&layerT.layer());
	if(eltLayer != nullptr){
		ifmLayout.sizeMult(eltLayer->get_workload().N);
	}

	delete[] arrs[0];
	delete[] arrs[1];
	delete[] arrs[2];
	delete[] arrs[3];
}

void StdLayerEngine::calcNoC(NoC& noc, const PlaceSch& place, LNode* curNode) const{
	noc.clear();

	const Node& layerT = curNode->layert;
	const len_t B = curNode->num_batch;
	const bool wgt_B = layerT.hasWgtPrevs();

	len_t curC;

	// Fetch weight first.
	if(wgt_B){
		// Fetch each prev layer from its ofmap/mem layout
		curC = 0;
		const auto& prevs = layerT.getWgtPrevs();
		FOR_BITSET(it, prevs){
			const lid_t prev = it;
			const len_t prevC = network->getNode(prev).layer().ofmap_shape().c;
			if(curNode->get_dirp_set().contains(prev)){
				const LNode* fromNode = (*(curNode->lnodeList))[prev];
				const auto& fromLayout = fromNode->get_place_sch().getOfmL();
				noc.betweenLayout(fromLayout, place.getWgtL(), curC, fromNode->num_batch, B);
			}else{
				// TODO: Change to last layer's memLayout.
				noc.fromRemoteMem(place.getWgtL(), curC, curC + prevC);
			}
			curC += prevC;
		}
		assert(curC == layerT.layer().weight_shape().c);
	}else{
		noc.fromRemoteMem(place.getWgtL());
		// Weight only need to fetch once. Thus divided by #bgrp.
		noc.div(LNode::tot_batch / B);
	}

	// Identify eltwise first
	len_t elt_K = 0, cur_N = 0;
	if(REF_IS_INSTANCE(layerT.layer(), EltwiseLayer)){
		elt_K = layerT.layer().ofmap_shape().c;
	}

	// Fetch external data from remote MEM
	curC = layerT.get_external_C();
	noc.fromRemoteMem(place.getIfmL(), 0, curC);
	if(elt_K > 0){
		if(curC == elt_K){
			curC = 0;
			++cur_N;
		}else{
			// This is checked when eltwise layer is added to the network.
			assert(curC < elt_K);
		}
	}

	// Fetch each prev layer from its ofmap/mem layout
	const auto& prevs = layerT.getIfmPrevs();
	FOR_BITSET(it, prevs){
		const lid_t prev = it;
		const len_t prevC = network->getNode(prev).layer().ofmap_shape().c;
		if(curNode->get_dirp_set().contains(prev)){
			const LNode* fromNode = (*(curNode->lnodeList))[prev];
			const auto& fromLayout = fromNode->get_place_sch().getOfmL();
			noc.betweenLayout(fromLayout, place.getIfmL(), curC, fromNode->num_batch, B);
		}else{
			// TODO: Change to last layer's memLayout.
			noc.fromRemoteMem(place.getIfmL(), curC, curC + prevC);
		}
		curC += prevC;
		if(elt_K > 0){
			if(curC == elt_K){
				curC = 0;
				++cur_N;
			}else{
				// This is checked when eltwise layer is added to the network.
				assert(curC < elt_K);
			}
		}
	}
	if(elt_K > 0) curC = elt_K * cur_N;
	assert(curC == layerT.layer().real_ifmap_shape().c);

	// Save to remote mem if necessary
	if(curNode->to_dram){
		noc.toRemoteMem(place.getOfmL());
	}

	/*
	if(curNode->to_dram){
		// TODO: change interleaving into 1-2 remote mems.
		fmap_range ofmRange(ofmShape, totBatch);
		placeSch.memLayout = std::make_unique<MemULayout>(ofmRange, NoC::dram_list.data(), NoC::dram_list.size());
		layerSch.place.memLayout = std::make_unique<MemULayout>(ofmRange, NoC::dram_list.data(), NoC::dram_list.size());
	}
	*/
}
