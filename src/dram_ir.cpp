#include "dram_ir.h"

const Json::Value& DRAM::get_IR() const{
	return IR;
}

void DRAM::append_weight(const Json::Value& key, const Json::Value& weight){
	weight_pos[key]=IR["out"].size();
	IR["out"].append(weight);
	IR["out"][weight_pos[key]]["priority"]=std::min(IR["out"][weight_pos[key]]["priority"].asUInt(),weight["destination"][0u]["workload_id"].asUInt());
}

void DRAM::append_weight_destination(const Json::Value& key, const Json::Value& destination){
	IR["out"][weight_pos[key]]["destination"].append(destination);
	IR["out"][weight_pos[key]]["priority"]=std::min(IR["out"][weight_pos[key]]["priority"].asUInt(),destination["workload_id"].asUInt());
}

void DRAM::append_ofmap(const Json::Value& key, const Json::Value& ofmap){
	Json::Value empty_list;
	empty_list.append(1);
	empty_list.resize(0);
	std::vector<tfid_t> merged;
	for(const auto &transfer_id: ofmap["related_ifmap"]){
		merged.push_back(transfer_id.asUInt());
	}
	std::sort(merged.begin(),merged.end());
	auto newend=std::unique(merged.begin(),merged.end());
	Json::Value new_related_ifmap = empty_list;
	for(auto iter=merged.begin();iter!=newend;++iter){
		new_related_ifmap.append(*iter);
	}
	ofmap_pos[key]=IR["out"].size();
	IR["out"].append(ofmap);
	IR["out"][ofmap_pos[key]]["priority"]=ofmap["destination"][0u]["workload_id"];
	IR["out"][ofmap_pos[key]]["related_ifmap"]=new_related_ifmap;
}

void DRAM::append_ofmap_destination(const Json::Value& key, const Json::Value& destination){
	IR["out"][ofmap_pos[key]]["destination"].append(destination);
	IR["out"][ofmap_pos[key]]["priority"]=std::min(IR["out"][ofmap_pos[key]]["priority"].asUInt(),destination["workload_id"].asUInt());
}

void DRAM::append_ifmap(tfid_t transfer_id, const Json::Value& ifmap){
	ifmap_pos[transfer_id]=IR["in"].size();
	IR["in"].append(ifmap);
}

void DRAM::append_related_ofmap(tfid_t transfer_id, tfid_t ofmap_transfer_id){
	if(related_ofmap.size()<=ifmap_pos[transfer_id]){
		related_ofmap.resize(ifmap_pos[transfer_id]+1);
	}
	if(!related_ofmap[ifmap_pos[transfer_id]].count(ofmap_transfer_id)){
		related_ofmap[ifmap_pos[transfer_id]][ofmap_transfer_id]=true;
		IR["in"][ifmap_pos[transfer_id]]["related_ofmap"].append(ofmap_transfer_id);
	}
}

void DRAM::merge_related_ifmap(const Json::Value& key, const Json::Value& related_ifmap){
	Json::Value empty_list;
	empty_list.append(1);
	empty_list.resize(0);
	std::vector<tfid_t> merged;
	for(const auto &transfer_id: IR["out"][ofmap_pos[key]]["related_ifmap"]){
		merged.push_back(transfer_id.asUInt());
	}
	for(const auto &transfer_id: related_ifmap){
		merged.push_back(transfer_id.asUInt());
	}
	std::sort(merged.begin(),merged.end());
	auto newend=std::unique(merged.begin(),merged.end());
	Json::Value new_related_ifmap=empty_list;
	for(auto iter=merged.begin();iter!=newend;++iter){
		new_related_ifmap.append(*iter);
	}
	IR["out"][ofmap_pos[key]]["related_ifmap"]=new_related_ifmap;
}
