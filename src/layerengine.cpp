#include "layerengine.h"

#include <cassert>

#include "network.h"
#include "partition.h"


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

	// Info extracted from curNode
	const Cluster& cluster = curNode->cluster;
	const Node& layerT = curNode->layert;
	const Layer& layer = layerT.layer();
	const fmap_shape& ofmShape = layer.ofmap_shape();
	len_t totBatch = LNode::tot_batch;
	len_t B = curNode->num_batch;
	bool wgt_B = layerT.hasWgtPrevs();
	/*
	len_t K = ofmShape.c;
	len_t H = ofmShape.h;
	len_t W = ofmShape.w;
	*/
	cidx_t numCores = cluster.num_cores();
	const Core::Buffer& ubuf = mapper->core().ubuf();
	vol_t totUbufSize = ubuf.Size * numCores;

	// Current scheme
	SchNode::SchCost curCost;
	PlaceSch placeSch;
	NoC noc(false);
	pos_t* permOrder = new pos_t[numCores];
	placeSch.permuteOrder.reset(permOrder);
	placeSch.ifmLayout = std::make_unique<StdDataLayout>(numCores, permOrder);
	if(layer.weight_size() > 0)
		placeSch.wgtLayout = std::make_unique<StdDataLayout>(numCores, permOrder);
	else
		placeSch.wgtLayout = std::make_unique<StdDataLayout>(0, nullptr);
	placeSch.ofmLayout = std::make_unique<StdULayout>(numCores, permOrder);
	// TODO: change interleaving into 1-2 remote mems.
	/*
	if(curNode->to_dram){
		// TODO: change interleaving into 1-2 remote mems.
		fmap_range ofmRange(ofmShape, totBatch);
		placeSch.memLayout = std::make_unique<MemULayout>(ofmRange, NoC::dram_list.data(), NoC::dram_list.size());
		layerSch.place.memLayout = std::make_unique<MemULayout>(ofmRange, NoC::dram_list.data(), NoC::dram_list.size());
	}
	*/

	PartSch& partSch = placeSch.part;
	CoreMapper::CoreMapping tileSch;

	// For ubuf energy
	energy_t ubufOfm = ofmShape.tot_size(B) * ubuf.RCost;
	energy_t ubufTotal;

	len_t minCuts = 0;
	if(REF_IS_INSTANCE(layer, ConvLayer) && !REF_IS_INSTANCE(layer, GroupConvLayer))
		minCuts = static_cast<len_t>(layer.real_ifmap_shape().tot_size(B) / (totUbufSize*0.8) + 1);
	auto partIter = partEngine.init(numCores, B, layerT, partSch, minCuts);
	if(!partIter){
		return layerSch;
	}
	do{
		assert(partSch.size() == static_cast<unsigned>(numCores));

		// Update placeSch
		initLayouts(placeSch, layerT, ofmShape, B);

		// Estimate buffer usage
		vol_t estimatedBuf = mapper->buffer_size(placeSch.ofmLayout->maxRange());
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
		if(!wgt_B) ubufWgt /= (totBatch / B);
		ubufTotal = ubufWgt + ubufOfm;
		ubufTotal += placeSch.ifmLayout->totalSize() * ubuf.WCost;
		curCost.energy += ubufTotal;

		// Search for placement.
		auto placeIter = placeEngine.init(placeSch);
		// if(!placeIter) continue;
		do{
			placeSch.initPlacement(cluster);
			static_cast<StdDataLayout&>(placeSch.getIfmL()).setCPosArr();
			static_cast<StdDataLayout&>(placeSch.getWgtL()).setCPosArr();
			calcNoC(noc, placeSch, layerSch.iMemLayouts, layerSch.oMemLayout, curNode);
			SchNode::SchCost curCostAll = curCost;
			curCostAll.energy += noc.get_cost();
			cycle_t nocTime = noc.get_time();
			curCostAll.time = MAX(curCostAll.time, nocTime);
			if(curCostAll.cost() < layerSch.totCost.cost()){
				layerSch.totCost = curCostAll;
				layerSch.extUbufEnergy = ubufTotal;
				layerSch.tileSch = tileSch;
				layerSch.place.update(std::move(placeSch));
				// layerSch.oMemLayout = oMemLayout;
			}
		// TODO: add cur_cost.cost back.
		}while(placeIter.nextPlace(/*cur_cost.cost()*/));
	}while(partIter.nextPart(/*cur_cost.cost()*/));
	if(layerSch.isValid()){
		layerSch.place.ifmLayout = std::move(placeSch.ifmLayout);
		layerSch.place.wgtLayout = std::move(placeSch.wgtLayout);
		layerSch.place.ofmLayout = std::move(placeSch.ofmLayout);
		layerSch.place.permuteOrder = std::move(placeSch.permuteOrder);
		initLayouts(layerSch.place, layerT, ofmShape, B);
		layerSch.place.initPlacement(cluster);
		static_cast<StdDataLayout&>(layerSch.place.getIfmL()).setCPosArr();
		static_cast<StdDataLayout&>(layerSch.place.getWgtL()).setCPosArr();
		layerSch.place.finalize();
		calcNoC(layerSch.noc, layerSch.place, layerSch.iMemLayouts, layerSch.oMemLayout, curNode);
	}
	return layerSch;
}

void StdLayerEngine::initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const{
	using plen_t = PartSch::partlen_t;

	const PartSch& part = place.part;
	const Layer& layer = layerT.layer();
	bool fmap_K = layer.fmap_channel_rel();
	bool hasWgt = layer.weight_size() > 0;
	bool wgt_B = layerT.hasWgtPrevs();

	len_t* arrs[4];
	arrs[0] = part_intv(ofmShape.c, part.K);
	arrs[1] = part_intv(B, part.B);
	arrs[2] = part_intv(ofmShape.h, part.H);
	arrs[3] = part_intv(ofmShape.w, part.W);
	fmap_range::dim_range kRange, bRange, hRange, wRange;
	fmap_range emptyRange({0, 0}, {0, 0}, {0, 0}, {0, 0});

	auto& ofmLayout = static_cast<StdULayout&>(place.getOfmL());
	auto& ifmLayout = static_cast<StdDataLayout&>(place.getIfmL());
	auto& wgtLayout = static_cast<StdDataLayout&>(place.getWgtL());
	ofmLayout.setDims(part.K, part.B, part.H, part.W);
	if(fmap_K)
		ifmLayout.setBcast(1, 1);
	else
		ifmLayout.setBcast(part.K, part.B * part.H * part.W);

	if(hasWgt){
		if(wgt_B)
			wgtLayout.setBcast(part.H * part.W, 1);
		else
			wgtLayout.setBcast(part.B * part.H * part.W, 1);
	}
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
					*ofmArr = {kRange, bRange, hRange, wRange};
					vol_t s = (*ofmArr).size();
					ofmLayout.update(*ofmArr);
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

void StdLayerEngine::calcNoC(NoC& noc,
							 const PlaceSch& place,
							 std::vector<MemLayout>& iMemLayouts,
							 MemLayout& oMemLayout,
							 LNode* curNode) const
{
	noc.clear();
	const Node& layerT = curNode->layert;
	len_t B = curNode->num_batch;
	bool wgt_B = layerT.hasWgtPrevs();
	len_t curC;
	iMemLayouts.clear();

	// Fetch weight first.
	if(wgt_B){
		// Fetch each prev layer from its ofmap/mem layout
		curC = 0;
		const auto& prevs = layerT.getWgtPrevs();
		FOR_BITSET(it, prevs){
			lid_t prev = it;
			len_t prevC = network->getNode(prev).layer().ofmap_shape().c;
			LNode* fromNode = (*(curNode->lnodeList))[prev];
			if(curNode->get_dirp_set().contains(prev)){
				const auto& fromLayout = fromNode->get_place_sch().getOfmL();
				noc.betweenLayout(fromLayout, place.getWgtL(), curC, fromNode->num_batch, B);
			}else{
				const auto& iMemLayout = fromNode->get_oMemLayout();
				assert(!iMemLayout.empty());
				iMemLayouts.push_back(iMemLayout);
				noc.fromRemoteMem(iMemLayout, place.getWgtL(), curC, curC + prevC);
			}
			curC += prevC;
		}
		assert(curC == layerT.layer().weight_shape().c);
	}else{
		noc.fromRemoteMem(place.getWgtL());
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
			// TODO: here we have asserted input_C < elt_K for eltwise layer, so its fine.
			assert(curC <= elt_K);
		}
	}

	// Fetch each prev layer from its ofmap/mem layout
	const auto& prevs = layerT.getIfmPrevs();
	FOR_BITSET(it, prevs){
		lid_t prev = it;
		len_t prevC = network->getNode(prev).layer().ofmap_shape().c;
		LNode* fromNode = (*(curNode->lnodeList))[prev];
		if(curNode->get_dirp_set().contains(prev)){
			const auto& fromLayout = fromNode->get_place_sch().getOfmL();
			noc.betweenLayout(fromLayout, place.getIfmL(), curC, fromNode->num_batch, B);
		}else{
			const auto& iMemLayout = fromNode->get_oMemLayout();
			assert(!iMemLayout.empty());
			iMemLayouts.push_back(iMemLayout);
			noc.fromRemoteMem(iMemLayout, place.getIfmL(), curC, curC + prevC);
		}
		curC += prevC;
		if(elt_K > 0){
			if(curC == elt_K){
				curC = 0;
				++cur_N;
			}else{
				// TODO: here we have asserted input_C < elt_K for eltwise layer, so its fine.
				assert(curC <= elt_K);
			}
		}
	}
	if(elt_K > 0) curC = elt_K * cur_N;
	assert(curC == layerT.layer().real_ifmap_shape().c);

	// Save to remote mem if necessary
	if(curNode->to_dram){
		noc.toRemoteMem(place.getOfmL(), oMemLayout);
	}else{
		oMemLayout.clear();
	}
}

bool StdLayerEngine::updateNoC(LNode* curNode, NoC& old_noc) const {
	auto& noc = curNode->noc;
	const auto& place = curNode->place_sch;
	auto& iMemLayouts = curNode->iMemLayouts;
	auto mem_it  = iMemLayouts.begin();
	auto mem_end = iMemLayouts.end();
	const Node& layerT = curNode->layert;
	bool wgt_B = layerT.hasWgtPrevs();
	len_t curC;
	bool has_update = false;

	const auto update = [&](lid_t prev, const DataLayout& to) -> void {
		len_t prevC = network->getNode(prev).layer().ofmap_shape().c;
		LNode* fromNode = (*(curNode->lnodeList))[prev];
		if(!curNode->get_dirp_set().contains(prev)){
			const auto& m = fromNode->get_oMemLayout();
			assert(!m.empty());

			assert(mem_it != mem_end);
			if(*mem_it != m){
				if(!has_update){
					has_update = true;
					old_noc = noc;
				}
				noc.fromRemoteMem_upd(*mem_it, m, to, curC, curC + prevC);
				*mem_it = m;
			}
			++mem_it;
		}
		curC += prevC;
	};

	// Fetch weight first.
	if(wgt_B){
		// Fetch each prev layer from its ofmap/mem layout
		curC = 0;
		const auto& prevs = layerT.getWgtPrevs();
		FOR_BITSET(it, prevs){
			lid_t prev = it;
			update(prev, place.getWgtL());
		}
		assert(curC == layerT.layer().weight_shape().c);
	}

	// Identify eltwise first
	len_t elt_K = 0, cur_N = 0;
	if(REF_IS_INSTANCE(layerT.layer(), EltwiseLayer)){
		elt_K = layerT.layer().ofmap_shape().c;
	}

	// Fetch external data from remote MEM
	curC = layerT.get_external_C();
	if(elt_K > 0){
		if(curC == elt_K){
			curC = 0;
			++cur_N;
		}else{
			// TODO: here we have asserted input_C < elt_K for eltwise layer, so its fine.
			assert(curC <= elt_K);
		}
	}

	// Fetch each prev layer from its ofmap/mem layout
	const auto& prevs = layerT.getIfmPrevs();
	FOR_BITSET(it, prevs){
		lid_t prev = it;
		update(prev, place.getIfmL());
		if(elt_K > 0){
			if(curC == elt_K){
				curC = 0;
				++cur_N;
			}else{
				// TODO: here we have asserted input_C < elt_K for eltwise layer, so its fine.
				assert(curC <= elt_K);
			}
		}
	}
	if(elt_K > 0) curC = elt_K * cur_N;
	assert(curC == layerT.layer().real_ifmap_shape().c);

	return has_update;
}

bool LayerScheme::isValid() const{
	return totCost.isValid();
}
