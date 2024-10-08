#include <cassert>
#include "ir_util.h"
#include "json/json.h"
#include "network.h"
#include "schnode.h"

SchNode::csn_ptr SchNode::root;
wlid_t SchNode::workload_cnt;
tfid_t SchNode::transferid_cnt;
// std::vector<std::vector<std::vector<SchNode::jsonindex_t> > > SchNode::wlid; // core layer batch -> workload_list[core][wlid]
// core layer bchw_lowerbound
std::vector<std::vector<std::map<BCHW_coor,jsonindex_t> > > SchNode::wlid; // core layer bchw -> workload_list[core][wlid]
std::vector<bool> SchNode::from_core, SchNode::weight_from_core, SchNode::to_dram;
std::vector<std::map<fmap_range, jsonindex_t> > SchNode::ofmapid;
std::vector<std::set<Json::Value> > SchNode::curr_ifmap, SchNode::curr_weight;
std::map<std::string,lid_t> SchNode::name_to_id;

const Cut* LNode::get_lca(const LNode* node1, const LNode* node2){
	const Cut* lca = node2->parent;
	while (!lca->layers.contains(node1->layerid)) {
		lca = lca->parent;
	}
	return lca;
}

Json::Value SchNode::IR_gen() const{
	std::vector<Json::Value> workload_list;
	cidx_t num_cores = cluster.ylen * (cluster.xlen+2);
	workload_list.resize(num_cores);
	workload_cnt = 0;
	transferid_cnt = 0;
	root = this;
	wlid.resize(num_cores);
	for(size_t i=0;i<wlid.size();++i){
		wlid[i].resize(network->len());
	}
	ofmapid.resize(0);
	from_core.resize(0);
	weight_from_core.resize(0);
	to_dram.resize(0);
	curr_ifmap.resize(num_cores);
	curr_weight.resize(num_cores);
	name_to_id.clear();
	DRAM_list.clear();
	DRAM_list.resize(NoC::get_il_group_num());
	for(int i=0; i<network->len(); ++i){
		name_to_id[network->getNode(i).name()] = i;
	}
	add_workload_and_dfs(0, 0, workload_list);

	Json::Value ret;
	for(cidx_t i=0;i<num_cores;++i){
		for(const auto& ifmap:curr_ifmap[i]){
			Json::StyledWriter swriter;
			std::string IR_str = swriter.write(ifmap);
			std::cout << IR_str;
		}
		for(const auto& weight:curr_weight[i]){
			Json::StyledWriter swriter;
			std::string IR_str = swriter.write(weight);
			std::cout << IR_str;
		}
		assert(curr_ifmap[i].empty());
		assert(curr_weight[i].empty());

		Json::Value* last_wl = nullptr;
		for(Json::Value& wl: workload_list[i]){
			for(Json::Value& buffer: wl["buffer"]){
				if(buffer["type"] == "ifmap"){
					buffer["workload_id"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][{buffer["lower"][0u].asUInt(),buffer["lower"][1u].asUInt(),buffer["lower"][2u].asUInt(),buffer["lower"][3u].asUInt()}]]["workload_id"];
					buffer["source"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][{buffer["lower"][0u].asUInt(),buffer["lower"][1u].asUInt(),buffer["lower"][2u].asUInt(),buffer["lower"][3u].asUInt()}]]["ifmap_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][(Json::Value::UInt) 0].asUInt())]["source"];
					for(Json::Value &source: buffer["source"]){
						buffer["transfer_id"].append(source["transfer_id"]);
					}
				}
				if(buffer["type"] == "weight"){
					if(buffer.isMember("from_core")){
						if(!buffer.isMember("workload_id")){
							buffer["workload_id"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][{buffer["lower"][0u].asUInt(),buffer["lower"][1u].asUInt(),buffer["lower"][2u].asUInt(),buffer["lower"][3u].asUInt()}]]["workload_id"];
						}
						buffer.removeMember("from_core");
					}
					if(!buffer.isMember("source")){
						buffer["source"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][{buffer["lower"][0u].asUInt(),buffer["lower"][1u].asUInt(),buffer["lower"][2u].asUInt(),buffer["lower"][3u].asUInt()}]]["weight_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][(Json::Value::UInt) 0].asUInt())]["source"];
					}
					if(!buffer.isMember("transfer_id")){
						for(Json::Value &source: buffer["source"]){
							buffer["transfer_id"].append(source["transfer_id"]);
						}
					}
				}
			}
			if(wl.isMember("ifmap")){
				if(!from_core[wl["workload_id"].asUInt()]){
					Json::Value buffer;
					buffer["type"] = "ifmap";
					buffer["layer"] = wl["layer_name"];
					buffer["lower"] = wl["ifmap"]["lower"];
					buffer["upper"] = wl["ifmap"]["upper"];
					buffer["workload_id"] = wl["workload_id"];
					buffer["block"] = ((wl["ifmap"]["upper"][0u].asUInt() - wl["ifmap"]["lower"][0u].asUInt() + 1) * (wl["ifmap"]["upper"][1].asUInt() - wl["ifmap"]["lower"][1].asUInt() + 1) * (wl["ifmap"]["upper"][2].asUInt() - wl["ifmap"]["lower"][2].asUInt() + 1) * (wl["ifmap"]["upper"][3].asUInt() - wl["ifmap"]["lower"][3].asUInt() + 1) + 1023) >> 10;
					buffer["source"] = wl["ifmap_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][0u].asUInt())]["source"];
					for(Json::Value &source: buffer["source"]){
						buffer["transfer_id"].append(source["transfer_id"]);
					}
					//buffer["DRAMIFMAP"] = true;
					wl["buffer"].append(buffer);
					if(last_wl && (*last_wl)["workload_id"] >= wl["ifmap"]["max_workload_id"].asUInt()){
						(*last_wl)["buffer"].append(buffer);
					}
				}
			}
			if(wl.isMember("weight") && wl["weight"].isMember("from_ofmap")){
				if(!weight_from_core[wl["workload_id"].asUInt()]){
					Json::Value buffer;
					buffer["type"] = "weight";
					buffer["layer"] = wl["layer_name"];
					buffer["lower"] = wl["weight"]["lower"];
					buffer["upper"] = wl["weight"]["upper"];
					buffer["workload_id"] = wl["workload_id"];
					buffer["block"] = (wl["weight"]["size"].asUInt() / 8 + 1023) >> 10;
					buffer["source"] = wl["weight_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][0u].asUInt())]["source"];
					for(Json::Value &source: buffer["source"]){
						buffer["transfer_id"].append(source["transfer_id"]);
					}
					wl["buffer"].append(buffer);
					if(last_wl && (*last_wl)["workload_id"] >= wl["weight"]["max_workload_id"].asUInt()){
						(*last_wl)["buffer"].append(buffer);
					}
				}
			}
			if(wl.isMember("ifmap_temp")){
				wl.removeMember("ifmap_temp");
			}
			if(wl.isMember("weight_temp")){
				wl.removeMember("weight_temp");
			}
			last_wl = &wl;
		}
		if(workload_list[i].type() != Json::nullValue){
			ret[std::to_string(i)]=workload_list[i];
		}
	}
	ret["top_batch_cut"] = root->type != SchNode::NodeType::L ? dynamic_cast<const Cut*>(root)->get_num_bgrp() : 1;
	for(didx_t ilgroupid=0; ilgroupid<DRAM_list.size(); ++ilgroupid){
		auto &dram = DRAM_list[ilgroupid].get_IR();
		if(dram.isMember("related_ofmap_map")){
			dram.removeMember("related_ofmap_map");
		}
		ret["DRAM"][std::to_string(ilgroupid)]=dram;
	}
	ret["xlen"] = cluster.xlen;
	ret["ylen"] = cluster.ylen;
	return ret;
}

const LNode* LNode::get_lnode_by_id(lid_t id) const{
	assert(contains(id));
	return this;
}

const LNode* Cut::get_lnode_by_id(lid_t id) const{
	for(auto child : children){
		if(child->contains(id))
			return child->get_lnode_by_id(id);
	}
	assert(false);
	return nullptr;
}

void LNode::add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const{
	//printf("layer: %s, batch: %d\n", layert.name().c_str(), batch_offset);
	Json::Value empty_list;
	empty_list.append(1);
	empty_list.resize(0);
	const auto& ofm_parts = place_sch.getOfmL();
	for(auto part: ofm_parts){
		for(len_t b_coor=0; b_coor<tileSch.tile_part.B; ++b_coor){
			for(len_t c_coor=0; c_coor<tileSch.tile_part.K; ++c_coor){
				for(len_t h_coor=0; h_coor<tileSch.tile_part.H; ++h_coor){
					for(len_t w_coor=0; w_coor<tileSch.tile_part.W; ++w_coor){
						auto fraction = [](len_t n, len_t d, len_t coor){
							return n*coor/d;
						};
						fmap_range range = part.first, range_size = range;
						range.b.from=range_size.b.from+fraction(num_batch,tileSch.tile_part.B,b_coor);
						range.b.to=range_size.b.from+fraction(num_batch,tileSch.tile_part.B,b_coor+1);
						range.c.from=range_size.c.from+fraction(range_size.c.size(),tileSch.tile_part.K,c_coor);
						range.c.to=range_size.c.from+fraction(range_size.c.size(),tileSch.tile_part.K,c_coor+1);
						range.h.from=range_size.h.from+fraction(range_size.h.size(),tileSch.tile_part.H,h_coor);
						range.h.to=range_size.h.from+fraction(range_size.h.size(),tileSch.tile_part.H,h_coor+1);
						range.w.from=range_size.w.from+fraction(range_size.w.size(),tileSch.tile_part.W,w_coor);
						range.w.to=range_size.w.from+fraction(range_size.w.size(),tileSch.tile_part.W,w_coor+1);
						if(range.is_empty()) continue;

						range.b += batch_offset;
						pos_t core = part.second;
						Cluster::xyid_t core_id = Cluster::get_xyid(core);
						Json::Value workload;
						workload["workload_id"] = workload_cnt++;
						workload["layer_name"] = layert.name();
						if(REF_IS_INSTANCE(layert.layer(), FCLayer)){
							workload["layer_type"] = "fc";
						}
						else if(REF_IS_INSTANCE(layert.layer(), ConvLayer)){
							workload["layer_type"] = "conv2d";
						}
						else if(REF_IS_INSTANCE(layert.layer(), PoolingLayer)){
							workload["layer_type"] = "pool";
						}
						else if(REF_IS_INSTANCE(layert.layer(), EltwiseLayer)){
							workload["layer_type"] = "element_wise";
						}
						else if(REF_IS_INSTANCE(layert.layer(), PTPLayer)){
							workload["layer_type"] = "point_to_point";
						}
						Json::Value oblock_lower, oblock_upper;
						oblock_lower.append(range.b.from);
						oblock_lower.append(range.c.from);
						oblock_lower.append(range.h.from);
						oblock_lower.append(range.w.from);

						oblock_upper.append(range.b.to - 1);
						oblock_upper.append(range.c.to - 1);
						oblock_upper.append(range.h.to - 1);
						oblock_upper.append(range.w.to - 1);

						workload["workload"].append(oblock_lower);
						workload["workload"].append(oblock_upper);
						workload["ofmap_size"] = range.size() * 8;
						workload["time"] = (int)tileSch.cost.time;

						if(REF_IS_INSTANCE(layert.layer(), ConvLayer) && !layert.hasWgtPrevs()){
							Json::Value weight;
							weight["lower"] = range.c.from;
							weight["upper"] = range.c.to - 1;
							Json::Value key;
							key["segment"] = segment;
							key["layer_name"] = layert.name();
							key["lower"] = weight["lower"];
							key["upper"] = weight["upper"];
							Json::Value destination;
							destination["type"] = "core";
							destination["id"] = core_id;
							destination["workload_id"] = workload["workload_id"];
							bool recorded = DRAM_weight_tfid.count(key) > 0;
							tfid_t transfer_id = recorded ? DRAM_weight_tfid[key] : transferid_cnt++;
							if(!recorded){
								DRAM_weight_tfid[key]=transfer_id;
							}
							for(auto ilgroupid: memLayouts.wMemLayout.get_layouts()){
								auto& dram=DRAM_list[ilgroupid];
								if(recorded){
									len_t batch_size = 0;
									if(root->get_type() != NodeType::L){
										batch_size = tot_batch/dynamic_cast<const Cut*>(root)->get_num_bgrp();
									}
									else{
										batch_size = tot_batch;
									}
									if(batch_offset % batch_size == 0){
										dram.append_weight_destination(key,destination);
									}
								}
								else{
									ConvLayer::Workload wl = static_cast<const ConvLayer&>(layert.layer()).get_workload();
									Json::Value dram_weight;
									dram_weight["destination"].append(destination);
									dram_weight["layer_name"] = layert.name();
									dram_weight["lower"] = weight["lower"];
									dram_weight["upper"] = weight["upper"];
									dram_weight["related_ifmap"] = empty_list;
									dram_weight["transfer_id"] = transfer_id;
									dram_weight["size"] = wl.R * wl.S * wl.C * range.c.size() * 8;
									dram_weight["type"] = "weight";
									dram.append_weight(key,dram_weight);
								}
							}
							weight["transfer_id"].append(transfer_id);
							workload["weight"] = weight;
						}

						fmap_range ofmap_range = range;
						fmap_range weight_range = range;
						layert.layer().ofm_to_ifm(ofmap_range);
						layert.layer().ofm_to_wgt(weight_range);

						if(REF_IS_INSTANCE(layert.layer(), ConvLayer) && layert.hasWgtPrevs()){
							Json::Value weight;
							append_range_bch1(weight,weight_range);
							weight["from_ofmap"] = true;
							weight["size"] = weight_range.size() * 8;
							workload["weight"] = weight;
						}

						append_range_bchw(workload["ifmap"],ofmap_range);

						Bitset prev = layert.getPrevs();
						Bitset next = layert.get_nexts();
						wlid_t max_from_workload_id = 0;
						wlid_t weight_max_from_workload_id = 0;
						//std::cerr << prev;
						bool from_other_core = false, weight_from_other_core = false;
						len_t prev_channel_offset = 0;
						FOR_BITSET(layerno, prev){
							const Node& node = network->getNode(layerno);
							const LNode* lnode = root->get_lnode_by_id(layerno);
							assert(layert.getIfmPrevs().contains(layerno)^layert.getWgtPrevs().contains(layerno));
							const auto input_range = layert.getIfmPrevs().contains(layerno) ? ofmap_range : weight_range;
							const auto real_prev_channel_offset = layert.getIfmPrevs().contains(layerno) ? prev_channel_offset : 0;
							if(dirp_set.contains(layerno)){
								for(auto prev_part: lnode->get_place_sch().getOfmL()){
									for(len_t prev_batch_offset=0; prev_batch_offset<tot_batch; prev_batch_offset += lnode->num_batch){
										for(len_t prev_b_coor=0; prev_b_coor<lnode->tileSch.tile_part.B; ++prev_b_coor){
											for(len_t prev_c_coor=0; prev_c_coor<lnode->tileSch.tile_part.K; ++prev_c_coor){
												for(len_t prev_h_coor=0; prev_h_coor<lnode->tileSch.tile_part.H; ++prev_h_coor){
													for(len_t prev_w_coor=0; prev_w_coor<lnode->tileSch.tile_part.W; ++prev_w_coor){	
														auto fraction = [](len_t n, len_t d, len_t coor){
															return n*coor/d;
														};
														fmap_range prev_range = prev_part.first, range_size = prev_range;
														prev_range.b.from=range_size.b.from+fraction(lnode->num_batch,lnode->tileSch.tile_part.B,prev_b_coor);
														prev_range.b.to=range_size.b.from+fraction(lnode->num_batch,lnode->tileSch.tile_part.B,prev_b_coor+1);
														prev_range.c.from=range_size.c.from+fraction(range_size.c.size(),lnode->tileSch.tile_part.K,prev_c_coor);
														prev_range.c.to=range_size.c.from+fraction(range_size.c.size(),lnode->tileSch.tile_part.K,prev_c_coor+1);
														prev_range.h.from=range_size.h.from+fraction(range_size.h.size(),lnode->tileSch.tile_part.H,prev_h_coor);
														prev_range.h.to=range_size.h.from+fraction(range_size.h.size(),lnode->tileSch.tile_part.H,prev_h_coor+1);
														prev_range.w.from=range_size.w.from+fraction(range_size.w.size(),lnode->tileSch.tile_part.W,prev_w_coor);
														prev_range.w.to=range_size.w.from+fraction(range_size.w.size(),lnode->tileSch.tile_part.W,prev_w_coor+1);
														Cluster::xyid_t from_id = Cluster::get_xyid(prev_part.second);
														prev_range.b += prev_batch_offset;
														prev_range.c += real_prev_channel_offset;
														/*if(layert.name() == "encoder1_QK" && !layert.getIfmPrevs().contains(layerno))
														{
															printf("prev_range_from = %d %d %d %d\n",prev_range.b.from,prev_range.c.from,prev_range.h.from,prev_range.w.from);
															printf("prev_range_to = %d %d %d %d\n",prev_range.b.to,prev_range.c.to,prev_range.h.to,prev_range.w.to);
															printf("input_range_from = %d %d %d %d\n",input_range.b.from,input_range.c.from,input_range.h.from,input_range.w.from);
															printf("input_range_to = %d %d %d %d\n",input_range.b.to,input_range.c.to,input_range.h.to,input_range.w.to);
															printf("batch_offset = %d\n\n",batch_offset);
														}*/
														fmap_range intersect = input_range.intersect(prev_range);
														if(intersect.is_empty())
															continue;
														prev_range.c -= real_prev_channel_offset;
														intersect.c -= real_prev_channel_offset;
														if(layert.getIfmPrevs().contains(layerno)){
															from_other_core = 1;
														}
														else{
															weight_from_other_core = 1;
														}
														Json::Value ifmap;
														append_range_bchw(ifmap,intersect);

														ifmap["channel"].append(real_prev_channel_offset + intersect.c.from);
														ifmap["channel"].append(real_prev_channel_offset + intersect.c.to-1);
														ifmap["size"] = intersect.size()*8;

														ifmap["type"] = "core";
														ifmap["id"] = from_id;
														ifmap["layer_name"] = node.name();

														jsonindex_t prev_wlid = wlid[from_id][layerno][{prev_range.b.from,prev_range.c.from,prev_range.h.from,prev_range.w.from}];
														wlid_t prev_workload_id = workload_list[from_id][prev_wlid]["workload_id"].asUInt();
														if(layert.getIfmPrevs().contains(layerno)){
															max_from_workload_id = std::max(max_from_workload_id, prev_workload_id);
														}
														else{
															weight_max_from_workload_id = std::max(weight_max_from_workload_id, prev_workload_id);
														}
														if(ofmapid[prev_workload_id].count(intersect)){
															ifmap["transfer_id"] = workload_list[from_id][prev_wlid]["ofmap"][ofmapid[prev_workload_id][intersect]]["transfer_id"];
														}
														else{
															ifmap["transfer_id"] = transferid_cnt++;
														}

														if(layert.getIfmPrevs().contains(layerno)){
															workload["ifmap"]["transfer_id"].append(ifmap["transfer_id"]);
															workload["ifmap_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
														}
														else{
															workload["weight"]["transfer_id"].append(ifmap["transfer_id"]);
															workload["weight_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
														}


														Json::Value destination;
														destination["type"] = "core";
														destination["id"] = core_id;
														destination["workload_id"] = workload["workload_id"];
														destination["layer_name"] = layert.name();

														if(ofmapid[prev_workload_id].count(intersect)){
															workload_list[from_id][prev_wlid]["ofmap"][ofmapid[prev_workload_id][intersect]]["destination"].append(destination);
														}
														else{
															Json::Value ofmap;
															append_range_bchw(ofmap,intersect);

															ofmap["transfer_id"] = ifmap["transfer_id"];
															ofmap["size"] = intersect.size()*8;

															ofmap["destination"].append(destination);
															ofmapid[prev_workload_id][intersect] = workload_list[from_id][prev_wlid]["ofmap"].size();
															workload_list[from_id][prev_wlid]["ofmap"].append(ofmap);
														}
													}
												}
											}
										}
									}
								}
							}
							else{
								int lower_c, upper_c;
								lower_c = std::max(0, (int)input_range.c.from - (int)real_prev_channel_offset);
								upper_c = std::min((int)node.layer().ofmap_shape().c, (int)input_range.c.to - (int)real_prev_channel_offset);
								if(lower_c < upper_c){
									Json::Value related_ifmap;
									Json::Value ifmap;
									ifmap["lower"].append(input_range.b.from);
									ifmap["lower"].append(lower_c);
									ifmap["lower"].append(input_range.h.from);
									ifmap["lower"].append(input_range.w.from);

									ifmap["upper"].append(input_range.b.to-1);
									ifmap["upper"].append(upper_c-1);
									ifmap["upper"].append(input_range.h.to-1);
									ifmap["upper"].append(input_range.w.to-1);

									ifmap["channel"].append(real_prev_channel_offset + lower_c);
									ifmap["channel"].append(real_prev_channel_offset + upper_c-1);

									vol_t ifmap_size = 1;
									for(int i=0; i<4; ++i){
										ifmap_size *= ifmap["upper"][i].asUInt() - ifmap["lower"][i].asUInt() + 1;
									}
									ifmap["size"] = ifmap_size * 8;

									ifmap["type"] = "DRAM";
									ifmap["id"] = 0;
									ifmap["layer_name"] = node.name();

									Json::Value key;
									key["lower"] = ifmap["lower"];
									key["upper"] = ifmap["upper"];
									key["source_layer_name"] = node.name();
									key["destination_layer_name"] = layert.name();
									key["type"] = layert.getIfmPrevs().contains(layerno) ? "ifmap" : "weight";

									bool recorded = DRAM_ofmap_tfid.count(key) > 0;
									tfid_t ofmap_transfer_id = recorded ? DRAM_ofmap_tfid[key] : transferid_cnt++;
									if(!recorded){
										DRAM_ofmap_tfid[key]=ofmap_transfer_id;
									}

									for(auto prev_part: lnode->get_place_sch().getOfmL()){
										for(len_t prev_batch_offset=0; prev_batch_offset<tot_batch; prev_batch_offset += lnode->num_batch){
											for(len_t prev_b_coor=0; prev_b_coor<lnode->tileSch.tile_part.B; ++prev_b_coor){
												for(len_t prev_c_coor=0; prev_c_coor<lnode->tileSch.tile_part.K; ++prev_c_coor){
													for(len_t prev_h_coor=0; prev_h_coor<lnode->tileSch.tile_part.H; ++prev_h_coor){
														for(len_t prev_w_coor=0; prev_w_coor<lnode->tileSch.tile_part.W; ++prev_w_coor){	
															auto fraction = [](len_t n, len_t d, len_t coor){
																return n*coor/d;
															};
															fmap_range prev_range = prev_part.first, range_size = prev_range;
															prev_range.b.from=fraction(lnode->num_batch,lnode->tileSch.tile_part.B,prev_b_coor);
															prev_range.b.to=fraction(lnode->num_batch,lnode->tileSch.tile_part.B,prev_b_coor+1);
															prev_range.c.from=fraction(range_size.c.size(),lnode->tileSch.tile_part.K,prev_c_coor);
															prev_range.c.to=fraction(range_size.c.size(),lnode->tileSch.tile_part.K,prev_c_coor+1);
															prev_range.h.from=fraction(range_size.h.size(),lnode->tileSch.tile_part.H,prev_h_coor);
															prev_range.h.to=fraction(range_size.h.size(),lnode->tileSch.tile_part.H,prev_h_coor+1);
															prev_range.w.from=fraction(range_size.w.size(),lnode->tileSch.tile_part.W,prev_w_coor);
															prev_range.w.to=fraction(range_size.w.size(),lnode->tileSch.tile_part.W,prev_w_coor+1);
															Cluster::xyid_t from_id = Cluster::get_xyid(prev_part.second);
															prev_range.b += prev_batch_offset;
															prev_range.c += real_prev_channel_offset;
															
															fmap_range intersect = input_range.intersect(prev_range);
															if(intersect.is_empty())
																continue;
															prev_range.c -= real_prev_channel_offset;
															intersect.c -= real_prev_channel_offset;
															tfid_t transfer_id;

															jsonindex_t prev_wlid = wlid[from_id][layerno][{prev_range.b.from,prev_range.c.from,prev_range.h.from,prev_range.w.from}];
															wlid_t prev_workload_id = workload_list[from_id][prev_wlid]["workload_id"].asUInt();
															if(layert.getIfmPrevs().contains(layerno)){
																max_from_workload_id = std::max(max_from_workload_id, prev_workload_id);
															}
															else{
																weight_max_from_workload_id = std::max(weight_max_from_workload_id, prev_workload_id);
															}
															Json::Value destination;
															destination["type"] = "DRAM";
															destination["id"] = 0;
															destination["layer_name"] = layert.name();

															Json::Value source;
															source["lower"].append(prev_range.b.from);
															source["lower"].append(prev_range.c.from);
															source["lower"].append(prev_range.h.from);
															source["lower"].append(prev_range.w.from);

															source["upper"].append(prev_range.b.to-1);
															source["upper"].append(prev_range.c.to-1);
															source["upper"].append(prev_range.h.to-1);
															source["upper"].append(prev_range.w.to-1);
															source["core_id"] = from_id;
															source["workload_id"] = prev_workload_id;

															if(ofmapid[prev_workload_id].count(prev_range)){
																transfer_id = workload_list[from_id][prev_wlid]["ofmap"][ofmapid[prev_workload_id][prev_range]]["transfer_id"].asUInt();
																source["transfer_id"] = transfer_id;
																if(!SchNode::to_dram[prev_workload_id]){
																	workload_list[from_id][prev_wlid]["ofmap"][ofmapid[prev_workload_id][prev_range]]["destination"].append(destination);
																	SchNode::to_dram[prev_workload_id] = true;
																	DRAM_ifmap_pos[transfer_id] = DRAM["in"].size();
																	DRAM["in"].append(source);
																}

															}
															else{
																transfer_id = transferid_cnt++;
																source["transfer_id"] = transfer_id;
																Json::Value ofmap;
																ofmap["lower"] = source["lower"];
																ofmap["upper"] = source["upper"];
																ofmap["destination"].append(destination);
																ofmap["transfer_id"] = transfer_id;
																ofmap["size"] = prev_range.size() * 8;
																SchNode::to_dram[workload_list[from_id][prev_wlid]["workload_id"].asUInt()] = true;
																ofmapid[prev_workload_id][prev_range] = workload_list[from_id][prev_wlid]["ofmap"].size();
																workload_list[from_id][prev_wlid]["ofmap"].append(ofmap);
																DRAM_ifmap_pos[transfer_id] = DRAM["in"].size();
																DRAM["in"].append(source);
															}
															if(!DRAM["in"][DRAM_ifmap_pos[transfer_id]]["related_ofmap_map"].isMember(std::to_string(ofmap_transfer_id))){
																DRAM["in"][DRAM_ifmap_pos[transfer_id]]["related_ofmap"].append(ofmap_transfer_id);
																DRAM["in"][DRAM_ifmap_pos[transfer_id]]["related_ofmap_map"][std::to_string(ofmap_transfer_id)] = true;
															}
															related_ifmap.append(transfer_id);
														}
													}
												}
											}
										}
									}
									Json::Value destination;
									destination["id"] = core_id;
									destination["type"] = "core";
									destination["workload_id"] = workload["workload_id"];
									for(auto ilgroupid: lnode->get_oMemLayout().get_layouts()){
										auto &dram=DRAM_list[ilgroupid];
										if(recorded){
											dram.append_ofmap_destination(key,destination);
										}
										else{
											Json::Value ofmap;
											ofmap["lower"] = key["lower"];
											ofmap["upper"] = key["upper"];
											ofmap["layer_name"] = layert.name();
											ofmap["size"] = ifmap["size"];
											ofmap["transfer_id"] = ofmap_transfer_id;
											ofmap["destination"].append(destination);
											ofmap["related_ifmap"] = related_ifmap;
											ofmap["type"] = "fmap";
											dram.append_ofmap(key,ofmap);
										}
									}
									ifmap["transfer_id"] = ofmap_transfer_id;
									if(layert.getIfmPrevs().contains(layerno)){
										workload["ifmap"]["transfer_id"].append(ofmap_transfer_id);
										workload["ifmap_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
									}
									else{
										workload["weight"]["transfer_id"].append(ofmap_transfer_id);
										workload["weight_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
									}

								}
							}
							if(layert.getIfmPrevs().contains(layerno)){
								prev_channel_offset += network->getNode(layerno).layer().ofmap_shape().c;
							}
							//fprintf(stderr,"layerno = %d, add = %d\n", layerno, network->getNode(layerno).layer().ofmap_shape().c);
							if(REF_IS_INSTANCE(layert.layer(), EltwiseLayer)){
								auto eltlayer = dynamic_cast<const EltwiseLayer*>(&(layert.layer()));
								prev_channel_offset %= eltlayer->get_workload().K;

							}
						}

						workload["ifmap"]["max_workload_id"] = max_from_workload_id;
						if(layert.hasWgtPrevs()){
							workload["weight"]["max_workload_id"] = weight_max_from_workload_id;
						}

						if(prev.count() == 0){
							Json::Value ifmap;
							ifmap["lower"].append(ofmap_range.b.from);
							ifmap["lower"].append(ofmap_range.c.from);
							ifmap["lower"].append(ofmap_range.h.from);
							ifmap["lower"].append(ofmap_range.w.from);

							ifmap["upper"].append(ofmap_range.b.to-1);
							ifmap["upper"].append(ofmap_range.c.to-1);
							ifmap["upper"].append(ofmap_range.h.to-1);
							ifmap["upper"].append(ofmap_range.w.to-1);

							ifmap["channel"].append(ofmap_range.c.from);
							ifmap["channel"].append(ofmap_range.c.to-1);
							ifmap["size"] = ofmap_range.size() * 8;

							ifmap["layer_name"] = "input";
							ifmap["id"] = 0;
							ifmap["type"] = "DRAM";

							Json::Value key;
							key["source_layer_name"] = "input";
							key["destination_layer_name"] = layert.name();
							key["lower"] = ifmap["lower"];
							key["upper"] = ifmap["upper"];

							bool recorded = DRAM_ofmap_tfid.count(key) > 0;
							tfid_t transfer_id = recorded ? DRAM_ofmap_tfid[key] : transferid_cnt++;
							if(!recorded){
								DRAM_ofmap_tfid[key]=transfer_id;
							}
							ifmap["transfer_id"] = transfer_id;
							workload["ifmap_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
							Json::Value destination;
							destination["id"] = core_id;
							destination["type"] = "core";
							destination["workload_id"] = workload["workload_id"];

							for(auto ilgroupid: ){
								auto &dram=;
								if(recorded){
									DRAM["out"][DRAM_ofmap_pos[key]]["destination"].append(destination);
								}
								else{
									DRAM_ofmap_pos[key] = DRAM["out"].size();
									Json::Value input;
									input["transfer_id"] = transfer_id;
									input["layer_name"] = layert.name();
									input["related_ifmap"] = empty_list;
									input["destination"].append(destination);
									input["size"] = ifmap["size"];
									input["lower"] = ifmap["lower"];
									input["upper"] = ifmap["upper"];
									input["type"] = "fmap";
									DRAM["out"].append(input);
								}
							}
							workload["ifmap"]["transfer_id"].append(transfer_id);
						}
						if(next.count() == 0){
							Json::Value ofmap;
							ofmap["lower"].append(range.b.from);
							ofmap["lower"].append(range.c.from);
							ofmap["lower"].append(range.h.from);
							ofmap["lower"].append(range.w.from);

							ofmap["upper"].append(range.b.to-1);
							ofmap["upper"].append(range.c.to-1);
							ofmap["upper"].append(range.h.to-1);
							ofmap["upper"].append(range.w.to-1);

							ofmap["size"] = range.size() * 8;
							ofmap["transfer_id"] = transferid_cnt++;

							Json::Value destination;
							destination["type"] = "DRAM";
							destination["id"] = 0;
							destination["layer_name"] = "output";
							ofmap["destination"].append(destination);
							workload["ofmap"].append(ofmap);

							Json::Value output;
							output["lower"] = ofmap["lower"];
							output["upper"] = ofmap["upper"];
							output["core_id"] = core_id;
							output["related_ofmap"] = empty_list;
							output["workload_id"] = workload["workload_id"];
							output["transfer_id"] = ofmap["transfer_id"];

							DRAM["in"].append(output);
						}

						bool to_other_core = false;

						FOR_BITSET(layerno, next){
							const Node& node = network->getNode(layerno);
							const LNode* lnode = root->get_lnode_by_id(layerno);
							len_t next_channel_offset = 0;
							const Bitset& prev_set = node.getIfmPrevs();
							if(lnode->layert.getIfmPrevs().contains(layerid)){
								FOR_BITSET(lid, prev_set){
									if(lid != layerid){
										next_channel_offset += network->getNode(lid).layer().ofmap_shape().c;
									}
									else{
										break;
									}
								}
							}
							if(REF_IS_INSTANCE(node.layer(), EltwiseLayer)){
								auto eltlayer = dynamic_cast<const EltwiseLayer*>(&(node.layer()));
								next_channel_offset %= eltlayer->get_workload().K;
							}
							if(lnode->get_dirp_set().contains(layerid)){
								for(auto next_part: lnode->get_place_sch().getOfmL()){
									for(len_t next_batch_offset=0; next_batch_offset<tot_batch; next_batch_offset += lnode->num_batch){
										for(len_t next_b_coor=0; next_b_coor<lnode->tileSch.tile_part.B; ++next_b_coor){
											for(len_t next_c_coor=0; next_c_coor<lnode->tileSch.tile_part.K; ++next_c_coor){
												for(len_t next_h_coor=0; next_h_coor<lnode->tileSch.tile_part.H; ++next_h_coor){
													for(len_t next_w_coor=0; next_w_coor<lnode->tileSch.tile_part.W; ++next_w_coor){	
														auto fraction = [](len_t n, len_t d, len_t coor){
															return n*coor/d;
														};
														fmap_range next_range = next_part.first, range_size = next_range;
														next_range.b.from=range_size.b.from+fraction(lnode->num_batch,lnode->tileSch.tile_part.B,next_b_coor);
														next_range.b.to=range_size.b.from+fraction(lnode->num_batch,lnode->tileSch.tile_part.B,next_b_coor+1);
														next_range.c.from=range_size.c.from+fraction(range_size.c.size(),lnode->tileSch.tile_part.K,next_c_coor);
														next_range.c.to=range_size.c.from+fraction(range_size.c.size(),lnode->tileSch.tile_part.K,next_c_coor+1);
														next_range.h.from=range_size.h.from+fraction(range_size.h.size(),lnode->tileSch.tile_part.H,next_h_coor);
														next_range.h.to=range_size.h.from+fraction(range_size.h.size(),lnode->tileSch.tile_part.H,next_h_coor+1);
														next_range.w.from=range_size.w.from+fraction(range_size.w.size(),lnode->tileSch.tile_part.W,next_w_coor);
														next_range.w.to=range_size.w.from+fraction(range_size.w.size(),lnode->tileSch.tile_part.W,next_w_coor+1);
														Cluster::xyid_t to_id = Cluster::get_xyid(next_part.second);
														if(next_range.is_empty()) continue;
														next_range.b += next_batch_offset;
														if(lnode->getLayer().getIfmPrevs().contains(layerid)){
															node.layer().ofm_to_ifm(next_range);
														}
														else{
															node.layer().ofm_to_wgt(next_range);
														}
														range.c += next_channel_offset;
														fmap_range intersect = range.intersect(next_range);
														range.c -= next_channel_offset;
														if(intersect.is_empty())
															continue;
														if(to_id != core_id){
															to_other_core = true;
														}
														Json::Value buffer;
														buffer["type"] = lnode->getLayer().getIfmPrevs().contains(layerid) ? "ifmap" : "weight";
														if(!lnode->getLayer().getIfmPrevs().contains(layerid))
															buffer["from_core"] = true;
														buffer["layer"] = node.layer().get_name();
														buffer["lower"].append(next_range.b.from);
														buffer["lower"].append(next_range.c.from);
														buffer["lower"].append(next_range.h.from);
														buffer["lower"].append(next_range.w.from);
														buffer["upper"].append(next_range.b.to-1);
														buffer["upper"].append(next_range.c.to-1);
														buffer["upper"].append(next_range.h.to-1);
														buffer["upper"].append(next_range.w.to-1);
														buffer["block"] = ((next_range.size() + 1023) >> 10);
														curr_ifmap[to_id].insert(buffer);
													}
												}
											}
										}
									}
								}
							}
						}

						len_t batch_size = 0;
						Json::Value weight;
						if(REF_IS_INSTANCE(layert.layer(), ConvLayer) && !layert.hasWgtPrevs()){
							if(root->get_type() != NodeType::L){
								batch_size = tot_batch/dynamic_cast<const Cut*>(root)->get_num_bgrp();
							}
							else{
								batch_size = tot_batch;
							}
							weight["type"] = "weight";
							weight["layer"] = layert.name();
							weight["lower"] = range.c.from;
							weight["upper"] = range.c.to - 1;
							ConvLayer::Workload wl = static_cast<const ConvLayer&>(layert.layer()).get_workload();
							Json::Value source;
							source["size"] = wl.R * wl.S * wl.C * range.c.size() * 8;
							weight["block"] = (source["size"].asUInt() / 8 + 1023) >> 10;
							source["id"] = 0;
							source["type"] = "DRAM";
							source["lower"] = weight["lower"];
							source["upper"] = weight["upper"];
							source["transfer_id"] = workload["weight"]["transfer_id"][0u];
							weight["source"].append(source);
							weight["transfer_id"].append(workload["weight"]["transfer_id"]);
							if(batch_offset % batch_size == 0){
								curr_weight[core_id].insert(weight);
								if(workload_list[core_id].size() && get_lca(this, root->get_lnode_by_id(name_to_id[workload_list[core_id][workload_list[core_id].size()-1]["layer_name"].asString()])) != root){
									if(workload_list[core_id][workload_list[core_id].size()-1]["layer_name"] != layert.name()){
										workload_list[core_id][workload_list[core_id].size()-1]["buffer"].append(weight);
									}
								}
							}
						}

						for(const Json::Value& datablock : curr_ifmap[core_id]){
							workload["buffer"].append(datablock);
						}
						for(const Json::Value& weight : curr_weight[core_id]){
							workload["buffer"].append(weight);
						}
						if(to_other_core || to_dram){
							Json::Value ofmap;
							ofmap["type"] = "ofmap";
							ofmap["layer"] = layert.name();
							ofmap["lower"].append(range.b.from);
							ofmap["lower"].append(range.c.from);
							ofmap["lower"].append(range.h.from);
							ofmap["lower"].append(range.w.from);
							ofmap["upper"].append(range.b.to-1);
							ofmap["upper"].append(range.c.to-1);
							ofmap["upper"].append(range.h.to-1);
							ofmap["upper"].append(range.w.to-1);
							ofmap["block"] = (range.size() + 1023) / 1024;
							//ofmap["block"] = 10;
							ofmap["size"] = range.size() * 8;
							workload["buffer"].append(ofmap);
						}

						wlid[core_id][layerid][(BCHW_coor){range.b.from,range.c.from,range.h.from,range.w.from}] = workload_list[core_id].size();

						workload_list[core_id].append(workload);

						std::vector<Json::Value> this_workload_ifmap;
						for(const Json::Value& ifmap: curr_ifmap[core_id]){
							if(ifmap["layer"] == layert.name() && ifmap["lower"][0u] == range.b.from){
								this_workload_ifmap.push_back(ifmap);
							}
						}
						for(const Json::Value& ifmap: this_workload_ifmap){
							curr_ifmap[core_id].erase(ifmap);
						}
						if(REF_IS_INSTANCE(layert.layer(), ConvLayer) && !layert.hasWgtPrevs()){
							if((batch_offset + num_batch) % batch_size == 0){
								for(auto weight : curr_weight[core_id]){
									if(weight["layer"] == layert.name()){
										curr_weight[core_id].erase(weight);
										break;
									}
								}
							}
						}
						ofmapid.resize(workload_cnt);
						from_core.push_back(from_other_core);
						weight_from_core.push_back(weight_from_other_core);
						SchNode::to_dram.push_back(false);
					}
				}
			}
		}
	}
}

void TCut::add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const{
	for(len_t i=0;i<num_batch;i+=num_batch/num_bgrp){
		for(auto& child : children){
			child->add_workload_and_dfs(batch_offset + i, segment, workload_list);
			if(this == root){
				segment++;
			}
		}
	}
}

void SCut::add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const{
	const len_t stage_size = num_batch/num_bgrp;
	for(len_t stage_id=0; stage_id < num_bgrp+num_stage; ++stage_id){
		size_t i=0;
		for(auto child : children){
			if(stage_id >= stage[i] && stage_id < stage[i]+num_bgrp){
				len_t stage_offset = (stage_id - stage[i]) * stage_size + batch_offset;
				child->add_workload_and_dfs(stage_offset, segment, workload_list);
			}
			++i;
		}
	}
}
