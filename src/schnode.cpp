#include "schnode.h"

#include <cassert>
#include <iostream>

#include "layerengine.h"
#include "noc.h"
#include "placement.h"
#include "util.h"
#include "json/json.h"

LayerEngine* SchNode::layerMapper=nullptr;
len_t SchNode::tot_batch=0;

SchNode::sn_ptr SchNode::newNode(LTreeNode* _node, const Cluster& _c, Cut* parent){
	switch (_node->get_type()) {
		case NodeType::L:
			return new LNode(_node, _c, parent);
		case NodeType::S:
			return new SCut(_node, _c, parent);
		case NodeType::T:
			return new TCut(_node, _c, parent);
	}
	assert(false);
	return nullptr;
}

SchNode::SchNode(NodeType t, const Cluster& _c, cut_ptr _parent, len_t nbatch)
	:valid(true), type(t), num_batch(nbatch), cluster(_c), parent(_parent),
	 lnodeList(parent != nullptr ? parent->lnodeList : new nodeList_t){
	assert(nbatch == 0 || _parent == nullptr || _parent->num_batch % nbatch == 0);
	if(_parent != nullptr) _parent->add(this);
}

SchNode::~SchNode(){
	if(parent == nullptr) delete lnodeList;
}

void SchNode::setParent(Cut* newParent){
	const_cast<Cut*&>(parent) = newParent;
	if(newParent == nullptr){
		const_cast<nodeList_t*&>(lnodeList) = new nodeList_t;
	}else{
		const_cast<nodeList_t*&>(lnodeList) = newParent->lnodeList;
		newParent->add(this);
	}
}

bool SchNode::is_valid() const{
	return valid;
}

bool SchNode::is_DRAM_cut() const{
	return parent == nullptr && type == NodeType::T;
}

SchNode::NodeType SchNode::get_type() const{
	return type;
}

const Cluster& SchNode::get_cluster() const{
	return cluster;
}

SchNode::SchCost SchNode::get_cost() const{
	return cost;
}

const NoC& SchNode::get_noc() const{
	return noc;
}

const BufferUsage& SchNode::get_buf_usage() const{
	return buf_usage;
}

const BufferUsage& SchNode::get_ifm_usage() const{
	return ifm_usage;
}

const BufferUsage& SchNode::get_wgt_usage() const{
	return wgt_usage;
}

energy_t SchNode::get_ubuf_energy() const{
	return ubuf_energy;
}

energy_t SchNode::get_buf_energy() const{
	return buf_energy;
}

energy_t SchNode::get_bus_energy() const{
	return bus_energy;
}

energy_t SchNode::get_mac_energy() const{
	return mac_energy;
}

void SchNode::print_res(std::ostream& os) const{
	os << cost << ", Ubuf/Buf/Bus/Mac/NoC/DRAM:" << ubuf_energy << '/' << buf_energy << '/' << bus_energy << '/' << mac_energy;
	os << '/' << noc.get_hop_cost() << '/' << noc.get_cost() - noc.get_hop_cost();
	energy_t e = cost.energy;
	e -= ubuf_energy + buf_energy + bus_energy + mac_energy + noc.get_cost();
	if(e == 0 && cost.energy == 0) return;
	if(cost.energy != 0) e /= cost.energy;
	if(e>1e-8 || e<-1e-8){
		os << std::endl << "[Error]: cost mismatch! error: " << e;
	}
}

std::ostream& operator<<(std::ostream& os, const SchNode& sch){
	sch.print_res(os);
	return os;
}

std::ostream& operator<<(std::ostream& os, const SchNode* sch){
	sch->print_res(os);
	return os;
}

bool LNode::search(){
	auto res = layerMapper->search(this);
	if(!res.isValid()) return false;
	noc = std::move(res.noc);
	ubuf_energy = res.extUbufEnergy;
	place_sch = std::move(res.place);
	tileSch = res.tileSch;
	cost = res.totCost;
	return true;
}

LNode::LNode(LTreeNode *_node, const Cluster& _c, SchNode::cut_ptr _parent)
	:SchNode(NodeType::L, _c, _parent, _node->get_tot_batch()), layerid(_node->layers().first()),
	  layert(network->getNode(layerid)), /*place_sch(cluster, layert, _node->get_tot_batch()),*/
	dirp_set(_node->get_dirp_set()), to_dram(_node->get_to_dram()){
	searchLayer();
}

LNode::~LNode(){
	(*lnodeList)[layerid] = nullptr;
}

void LNode::searchLayer(){
	valid = search();
	if(!valid){
		return;
	}
	if(!place_sch.getIfmL().update(ifm_usage)){
		valid = false;
		return;
	}
	if(!place_sch.getWgtL().update(layert.hasWgtPrevs() ? ifm_usage : wgt_usage)){
		valid = false;
		return;
	}

	buf_usage = ifm_usage;
	if(!buf_usage.all_add(ofm_ubuf_vol)){
		valid = false;
		return;
	}
	if(!(buf_usage + wgt_usage)){
		valid = false;
		return;
	}
	(*lnodeList)[layerid] = this;
	ubuf_energy += tileSch.ubuf * cluster.num_cores();
	buf_energy = tileSch.buffer * cluster.num_cores();
	bus_energy = tileSch.noc * cluster.num_cores();
	mac_energy = tileSch.mac * cluster.num_cores();
	bool is_seg = (parent == nullptr) || parent->is_DRAM_cut();
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(cost.time, noc_time);
	}
}

void LNode::searchInc(LTreeNode* node){
	(void) node;
	// if(!node->isModified()) return;
	// We'll never use this function.
	// Since LNode can't be child-modified.
	assert(false);
	return;
}

SchNode* LNode::copy(Cut* newParent) const{
	LNode* node = new LNode(*this);
	node->setParent(newParent);
	(*node->lnodeList)[layerid] = node;
	return node;
}

bool LNode::contains(lid_t _layerid) const{
	return _layerid == layerid;
}

const Node& LNode::getLayer() const{
	return layert;
}

const PlaceSch& LNode::get_place_sch() const{
	return place_sch;
}

const Bitset& LNode::get_dirp_set() const{
	return dirp_set;
}

bool LNode::get_to_dram() const{
	return to_dram;
}

const CoreMapper::CoreMapping& LNode::get_tileSch() const {
	return tileSch;
}

void LNode::print_struct(std::string pad, std::ostream& os) const{
	os << pad << layert.name() << ' ' << num_batch << ' ' << place_sch;
	os << " util:" << tileSch.util*100 << '/' << tileSch.tot_util*100;
	os << ' ' << cost << " Ubuf/Buf/Bus/Mac:" << ubuf_energy << '/' << buf_energy << '/' << bus_energy << '/' << mac_energy;
	os << ' ' << noc << ' ' << buf_usage << ' ' << wgt_usage << ' ' << ifm_usage;
	os << ' ' << layert.layer().real_ifmap_shape().tot_size(num_batch);
	os << '/' << layert.layer().weight_size();
	os << '/' << layert.layer().ofmap_shape().tot_size(num_batch);
	os << " Max NoC: " << noc.get_max_link();
	os << ' ' << tileSch.tile_part;
	os << std::endl;
}

void LNode::print_tree(std::string pad, std::ostream& os) const{
	os << pad << layert.name() << ' ' << num_batch;
	// os << ' ' << place_sch;
	os << std::endl;
}

Cut::Cut(SchNode::NodeType t, LTreeNode* node, const Cluster& _c, SchNode::cut_ptr _parent)
	:SchNode(t, _c, _parent, node->get_tot_batch()), curNode(nullptr),
	  layers(node->layers()), num_bgrp(node->get_bgrp_num()){
}

Cut::~Cut(){
	for(auto child: children){
		delete child;
	}
}

/* Constructs a new child corresponding to "_node".
 * During incremental search, will reuse old SchNode children if possible.
 * (When the LTreeNode is not marked with "new", then it's reusable)
 */
SchNode::sn_ptr Cut::newNode(LTreeNode* _node, const Cluster& _c){
	if(curNode == nullptr) return SchNode::newNode(_node, _c, this);

	if(_node->isNew()) return SchNode::newNode(_node, _c, this);

	const Bitset& layers = _node->layers();
	bool found = false, reSearch = false;

	while(!oldChildren.empty()){
		SchNode* node = oldChildren.front();
		oldChildren.pop_front();

		switch(node->get_type()){
		case NodeType::L:
			if(layers.count() == 1 && node->contains(layers.first()))
				found = true;
			break;
		case NodeType::S:
			reSearch = (_c != node->get_cluster());
		[[clang::fallthrough]];
		case NodeType::T:
			Cut* cut = static_cast<Cut*>(node);
			if(cut->layers == layers) found = !reSearch;
			break;
		}

		if(found){
			children.push_back(node);
			if(_node->isModified())
				node->searchInc(_node);
			return node;
		}

		delete node;
		if(reSearch) return SchNode::newNode(_node, _c, this);
	}

	std::cerr << "[Warning] Cannot find old child in oldChildren." << std::endl;
	return SchNode::newNode(_node, _c, this);
}

void Cut::add(SchNode* child){
	children.push_back(child);
}

void Cut::searchInc(LTreeNode* node){
	assert(node->layers() == layers);
	curNode = node;
	oldChildren = std::move(children);
	children.clear();
	noc = NoC();
	ifm_usage = BufferUsage();
	wgt_usage = BufferUsage();
	buf_usage = BufferUsage();
	construct(node);
	while(!oldChildren.empty()){
		delete oldChildren.front();
		oldChildren.pop_front();
	}
	curNode = nullptr;
}

bool Cut::contains(lid_t layerid) const{
	return layers.contains(layerid);
}

const SchNode::sn_vec& Cut::getChildren() const{
	return children;
}

len_t Cut::get_num_bgrp() const{
	return num_bgrp;
}

void Cut::print_struct(std::string pad, std::ostream& os) const{
	os << pad << ((type == NodeType::S)?'S':'T');
	os << ' ' << num_batch << '/' << num_bgrp;
	os << ' ' << cost << " Ubuf/Buf/Bus/Mac:" << ubuf_energy << '/' << buf_energy << '/' << bus_energy << '/' << mac_energy;
	os << ' ' << noc << ' ' << buf_usage << ' ' << wgt_usage << ' ' << ifm_usage;
	os << " Max NoC: " << noc.get_max_link();
	os << std::endl;
	pad += '\t';
	for(auto child: children){
		child->print_struct(pad, os);
	}
}

void Cut::print_tree(std::string pad, std::ostream& os) const{
	os << pad << ((type == NodeType::S)?'S':'T');
	os << ' ' << num_batch << '/' << num_bgrp;
	// os << ' ' << cost;
	os << std::endl;
	pad += '\t';
	for(auto child: children){
		child->print_tree(pad, os);
	}
}

void TCut::construct(LTreeNode* node){
	bool is_top = (parent == nullptr);
	bool is_seg = (!is_top) && parent->is_DRAM_cut();
	bool wgt_shift = is_seg && (num_bgrp == 1);

	// Recursively construct (and search) each child.
	sn_ptr last_p = nullptr;
	cost.energy = 0;
	cost.time = 0;
	ubuf_energy = buf_energy = bus_energy = mac_energy = 0;
	for(auto child: node->get_children()){
		sn_ptr p = newNode(child, cluster);
		if(!p->is_valid()){
			valid = false;
			return;
		}
		if(!is_top){
			/*if(!p->get_ifm_usage()){
				valid = false;
				return;
			}*/
			if(!(is_seg || (ifm_usage += p->get_ifm_usage()))){
				valid = false;
				return;
			}
			if(last_p == nullptr){
				//buf_usage = p->get_buf_usage();
			}else{
				if(wgt_shift){
					buf_usage.max_with(p->get_ifm_usage() + last_p->get_buf_usage() + last_p->get_wgt_usage() + p->get_wgt_usage());
				}else{
					buf_usage.max_with(p->get_ifm_usage() + last_p->get_buf_usage());
				}
				if(!buf_usage){
					valid = false;
					return;
				}
				//buf_usage.max(p->get_buf_usage());
			}
			if(!wgt_shift && !(wgt_usage += p->get_wgt_usage())){
				valid = false;
				return;
			}
		}
		cost.time += p->get_cost().time;
		cost.energy += p->get_cost().energy;
		noc += p->get_noc();
		ubuf_energy += p->get_ubuf_energy();
		buf_energy += p->get_buf_energy();
		bus_energy += p->get_bus_energy();
		mac_energy += p->get_mac_energy();
		last_p = p;
	}

	// Update and check buffer usage.
	if(!is_top){
		if(num_bgrp == 1){
			if(wgt_shift){
				buf_usage.max_with(last_p->get_buf_usage() + last_p->get_wgt_usage());
			}else{
				buf_usage.max_with(last_p->get_buf_usage());
			}
		}else{
			if(!(is_seg || ifm_usage.multiple(num_bgrp))){
				valid = false;
				return;
			}
			if(wgt_shift){
				buf_usage.max_with(children.front()->get_ifm_usage() + children.front()->get_wgt_usage() + last_p->get_buf_usage() + last_p->get_wgt_usage());
			}else{
				buf_usage.max_with(children.front()->get_ifm_usage() + last_p->get_buf_usage());
			}
		}
		if(!buf_usage){
			valid = false;
			return;
		}
		if(!wgt_shift && !(buf_usage + wgt_usage)){
			valid = false;
			return;
		}
	}

	cost *= num_bgrp;
	noc *= num_bgrp;
	ubuf_energy *= num_bgrp;
	buf_energy *= num_bgrp;
	bus_energy *= num_bgrp;
	mac_energy *= num_bgrp;
	// Needs to bound total time with DRAM access.
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(cost.time, noc_time);
	}
}

TCut::TCut(LTreeNode *_node, const Cluster& _c, SchNode::cut_ptr _parent)
	:Cut(NodeType::T, _node, _c, _parent){
	TCut::construct(_node);
}

SchNode* TCut::copy(Cut* newParent) const{
	TCut* cut = new TCut(*this);
	cut->setParent(newParent);
	cut->children.clear();
	for(auto child : children){
		child->copy(cut);
	}
	return cut;
}

void SCut::construct(LTreeNode* node){
	bool is_seg = (parent == nullptr) || parent->is_DRAM_cut();
	const auto& cnodes = node->get_children();
	cidx_t cnum = static_cast<cidx_t>(cnodes.size());
	assert(cnum > 0);

	// Initialize utime list.
	utime_t* tlist = new utime_t[cnum];
	utime_t* cur_item = tlist;
	for(auto child: cnodes){
		*(cur_item++) = child->get_utime();
	}

	// Try to allocate subclusters.
	auto allocRes = cluster.try_alloc(tlist, cnum);
	delete[] tlist;
	if(!allocRes){
		valid = false;
		return;
	}

	// Recursively construct (and search) each child.
	cidx_t i=0;
	cycle_t max_time = 0;
	cost.energy = 0;
	ubuf_energy = buf_energy = bus_energy = mac_energy = 0;
	for(auto child: cnodes){
		auto p = newNode(child, cluster.sub_cluster(i++, allocRes));
		if(!p->is_valid()){
			valid = false;
			return;
		}
		if(num_bgrp > 1 && !(buf_usage += p->get_ifm_usage())){
			valid = false;
			return;
		}
		if(!(buf_usage += p->get_buf_usage())){
			valid = false;
			return;
		}
		if(!(wgt_usage += p->get_wgt_usage())){
			valid = false;
			return;

		}
		if(!(is_seg || (ifm_usage += p->get_ifm_usage()))){
			valid = false;
			return;
		}
		//cost.time += p->get_cost().time;
		cost.energy += p->get_cost().energy;
		max_time = MAX(p->get_cost().time, max_time);
		noc += p->get_noc();
		ubuf_energy += p->get_ubuf_energy();
		buf_energy += p->get_buf_energy();
		bus_energy += p->get_bus_energy();
		mac_energy += p->get_mac_energy();
	}

	// Update and check buffer usage.
	if(!(buf_usage + wgt_usage)){
		valid = false;
		return;
	}

	ifm_usage.multiple(num_bgrp);
	if(!(is_seg || ifm_usage)){
		valid = false;
		return;
	}

	cost.time = max_time * (num_stage + num_bgrp);
	cost.energy *= num_bgrp;
	noc *= num_bgrp;
	ubuf_energy *= num_bgrp;
	buf_energy *= num_bgrp;
	bus_energy *= num_bgrp;
	mac_energy *= num_bgrp;
	// Needs to bound total time with DRAM access.
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(cost.time, noc_time);
	}
}

SCut::SCut(LTreeNode *_node, const Cluster& _c, SchNode::cut_ptr _parent)
	:Cut(NodeType::S, _node, _c, _parent),
	 stage(_node->get_stages()), num_stage(_node->get_num_stage()){
	SCut::construct(_node);
}

SchNode* SCut::copy(Cut* newParent) const{
	SCut* cut = new SCut(*this);
	cut->setParent(newParent);
	cut->children.clear();
	for(auto child : children){
		child->copy(cut);
	}
	return cut;
}

SchNode::SchCost::SchCost(energy_t _energy, cycle_t _time)
	:energy(_energy),time(_time){}

SchNode::SchCost&SchNode::SchCost::operator+=(const SchNode::SchCost& other){
	if(!(isValid() && other.isValid())){
		energy = energy_inf;
		return *this;
	}
	energy += other.energy;
	time += other.time;
	return *this;
}

SchNode::SchCost& SchNode::SchCost::operator*=(len_t other){
	if(isValid()){
		energy *= other;
		time *= other;
	}
	return *this;
}

bool SchNode::SchCost::operator!=(const SchNode::SchCost& other) const{
	if(isValid() && other.isValid())
		return energy != other.energy || time != other.time;
	return isValid() == other.isValid();
}

bool SchNode::SchCost::isValid() const{
	return energy < energy_inf;
}

cost_t SchNode::SchCost::cost(len_t nbatch) const{
	return calc_cost(energy, time*nbatch);
}

std::ostream& operator<<(std::ostream& os, const SchNode::SchCost& cost){
	return os << "E:" << cost.energy << ", T:" << cost.time << ", Cost:" << cost.cost();
}


// **************** Code for IR generation ****************

SchNode::csn_ptr SchNode::root;
SchNode::wlid_t SchNode::workload_cnt;
SchNode::tfid_t SchNode::transferid_cnt;
std::vector<std::vector<std::vector<SchNode::jsonindex_t> > > SchNode::wlid;
std::vector<bool> SchNode::from_core, SchNode::weight_from_core, SchNode::to_dram;
std::vector<std::map<fmap_range, SchNode::jsonindex_t> > SchNode::ofmapid;
std::vector<std::set<Json::Value> > SchNode::curr_ifmap, SchNode::curr_weight;
std::map<std::string,lid_t> SchNode::name_to_id;
Json::Value SchNode::DRAM;
std::map<Json::Value,SchNode::jsonindex_t> SchNode::DRAM_ofmap_pos;
std::map<Json::Value,SchNode::jsonindex_t> SchNode::DRAM_weight_pos;
std::map<SchNode::tfid_t,SchNode::jsonindex_t> SchNode::DRAM_ifmap_pos;

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
		for(size_t j=0;j<wlid[i].size();++j){
			wlid[i][j].resize(tot_batch);
		}
	}
	ofmapid.resize(0);
	from_core.resize(0);
	weight_from_core.resize(0);
	to_dram.resize(0);
	curr_ifmap.resize(num_cores);
	curr_weight.resize(num_cores);
	name_to_id.clear();
	DRAM.clear();
	DRAM_ifmap_pos.clear();
	DRAM_weight_pos.clear();
	DRAM_ofmap_pos.clear();
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
					buffer["workload_id"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][buffer["lower"][0u].asUInt()]]["workload_id"];
					buffer["source"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][buffer["lower"][(Json::Value::UInt) 0].asUInt()]]["ifmap_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][(Json::Value::UInt) 0].asUInt())]["source"];
					for(Json::Value &source: buffer["source"]){
						buffer["transfer_id"].append(source["transfer_id"]);
					}
				}
				if(buffer["type"] == "weight"){
					if(buffer.isMember("from_core")){
						if(!buffer.isMember("workload_id")){
							buffer["workload_id"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][buffer["lower"][0u].asUInt()]]["workload_id"];
						}
						buffer.removeMember("from_core");
					}
					if(!buffer.isMember("source")){
						buffer["source"] = workload_list[i][wlid[i][name_to_id[buffer["layer"].asString()]][buffer["lower"][(Json::Value::UInt) 0].asUInt()]]["weight_temp"][buffer["layer"].asString()+"_"+std::to_string(buffer["lower"][(Json::Value::UInt) 0].asUInt())]["source"];
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
	for(auto &in: DRAM["in"]){
		if(in.isMember("related_ofmap_map")){
			in.removeMember("related_ofmap_map");
		}
	}
	ret["top_batch_cut"] = root->type != SchNode::NodeType::L ? dynamic_cast<const Cut*>(root)->get_num_bgrp() : 1;
	ret["-1"] = DRAM;
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
		fmap_range range = part.first;
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
			tfid_t transfer_id = 0;
			if(DRAM_weight_pos.count(key)){
				transfer_id = DRAM["out"][DRAM_weight_pos[key]]["transfer_id"].asUInt();
				len_t batch_size = 0;
				if(root->get_type() != NodeType::L){
					batch_size = tot_batch/dynamic_cast<const Cut*>(root)->get_num_bgrp();
				}
				else{
					batch_size = tot_batch;
				}
				if(batch_offset % batch_size == 0){
					DRAM["out"][DRAM_weight_pos[key]]["destination"].append(destination);
				}
			}
			else{
				DRAM_weight_pos[key] = DRAM["out"].size();
				transfer_id = transferid_cnt++;
				Json::Value dram_weight;
				dram_weight["destination"].append(destination);
				dram_weight["layer_name"] = layert.name();
				dram_weight["lower"] = weight["lower"];
				dram_weight["upper"] = weight["upper"];
				dram_weight["related_ifmap"] = empty_list;
				dram_weight["transfer_id"] = transfer_id;
				ConvLayer::Workload wl = static_cast<const ConvLayer&>(layert.layer()).get_workload();
				dram_weight["size"] = wl.R * wl.S * wl.C * range.c.size() * 8;
				dram_weight["type"] = "weight";
				DRAM["out"].append(dram_weight);
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
			weight["lower"].append(weight_range.b.from);
			weight["lower"].append(weight_range.c.from);
			weight["lower"].append(weight_range.h.from);
			weight["upper"].append(weight_range.b.to-1);
			weight["upper"].append(weight_range.c.to-1);
			weight["upper"].append(weight_range.h.to-1);
			weight["from_ofmap"] = true;
			weight["size"] = weight_range.size() * 8;
			workload["weight"] = weight;
		}

		workload["ifmap"]["lower"].append(ofmap_range.b.from);
		workload["ifmap"]["lower"].append(ofmap_range.c.from);
		workload["ifmap"]["lower"].append(ofmap_range.h.from);
		workload["ifmap"]["lower"].append(ofmap_range.w.from);
		workload["ifmap"]["upper"].append(ofmap_range.b.to-1);
		workload["ifmap"]["upper"].append(ofmap_range.c.to-1);
		workload["ifmap"]["upper"].append(ofmap_range.h.to-1);
		workload["ifmap"]["upper"].append(ofmap_range.w.to-1);

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
						Cluster::xyid_t from_id = Cluster::get_xyid(prev_part.second);
						fmap_range prev_range = prev_part.first;
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
						ifmap["lower"].append(intersect.b.from);
						ifmap["lower"].append(intersect.c.from);
						ifmap["lower"].append(intersect.h.from);
						ifmap["lower"].append(intersect.w.from);

						ifmap["upper"].append(intersect.b.to-1);
						ifmap["upper"].append(intersect.c.to-1);
						ifmap["upper"].append(intersect.h.to-1);
						ifmap["upper"].append(intersect.w.to-1);

						ifmap["channel"].append(real_prev_channel_offset + intersect.c.from);
						ifmap["channel"].append(real_prev_channel_offset + intersect.c.to-1);

						/*vol_t ifmap_size = 1;
						for(int i=0; i<4; ++i){
							ifmap_size *= ifmap["source"]["upper"][i].asUInt() - ifmap["source"]["lower"][i].asUInt();
						}
						ifmap["source"]["size"] = ifmap_size * 8;*/

						ifmap["size"] = intersect.size()*8;

						ifmap["type"] = "core";
						ifmap["id"] = from_id;
						//ifmap["workload_id"] = workload_list[from_id][wlid[from_id][layerno][intersect.b.from]]["workload_id"];
						ifmap["layer_name"] = node.name();

						jsonindex_t prev_wlid = wlid[from_id][layerno][intersect.b.from];
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
							ofmap["lower"].append(intersect.b.from);
							ofmap["lower"].append(intersect.c.from);
							ofmap["lower"].append(intersect.h.from);
							ofmap["lower"].append(intersect.w.from);

							ofmap["upper"].append(intersect.b.to-1);
							ofmap["upper"].append(intersect.c.to-1);
							ofmap["upper"].append(intersect.h.to-1);
							ofmap["upper"].append(intersect.w.to-1);

							ofmap["transfer_id"] = ifmap["transfer_id"];
							ofmap["size"] = intersect.size()*8;

							ofmap["destination"].append(destination);
							ofmapid[prev_workload_id][intersect] = workload_list[from_id][prev_wlid]["ofmap"].size();
							workload_list[from_id][prev_wlid]["ofmap"].append(ofmap);
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

					tfid_t ofmap_transfer_id;

					if(DRAM_ofmap_pos.count(key)){
						ofmap_transfer_id = DRAM["out"][DRAM_ofmap_pos[key]]["transfer_id"].asUInt();
					}
					else{
						ofmap_transfer_id = transferid_cnt++;
					}

					for(auto prev_part: lnode->get_place_sch().getOfmL()){
						for(len_t prev_batch_offset=0; prev_batch_offset<tot_batch; prev_batch_offset += lnode->num_batch){
							Cluster::xyid_t from_id = Cluster::get_xyid(prev_part.second);
							fmap_range prev_range = prev_part.first;
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
							tfid_t transfer_id;

							jsonindex_t prev_wlid = wlid[from_id][layerno][intersect.b.from];
							wlid_t prev_workload_id = workload_list[from_id][prev_wlid]["workload_id"].asUInt();
							if(layert.getIfmPrevs().contains(layerno)){
								max_from_workload_id = std::max(max_from_workload_id, prev_workload_id);
							}
							else{
								weight_max_from_workload_id = std::max(weight_max_from_workload_id, prev_workload_id);
							}
							//max_prev_wlid = std::max(max_prev_wlid, prev_workload_id);
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
					Json::Value destination;
					destination["id"] = core_id;
					destination["type"] = "core";
					destination["workload_id"] = workload["workload_id"];

					if(DRAM_ofmap_pos.count(key)){
						DRAM["out"][DRAM_ofmap_pos[key]]["destination"].append(destination);
					}
					else{
						DRAM_ofmap_pos[key] = DRAM["out"].size();
						Json::Value ofmap;
						ofmap["lower"] = key["lower"];
						ofmap["upper"] = key["upper"];
						ofmap["layer_name"] = layert.name();
						ofmap["size"] = ifmap["size"];
						ofmap["transfer_id"] = ofmap_transfer_id;
						ofmap["destination"].append(destination);
						ofmap["related_ifmap"] = related_ifmap;
						ofmap["type"] = "fmap";
						DRAM["out"].append(ofmap);
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

			tfid_t transfer_id;
			if(DRAM_ofmap_pos.count(key)){
				transfer_id = DRAM["out"][DRAM_ofmap_pos[key]]["transfer_id"].asUInt();
			}
			else{
				transfer_id = transferid_cnt++;
			}
			ifmap["transfer_id"] = transfer_id;
			workload["ifmap_temp"][layert.name()+"_"+std::to_string(range.b.from)]["source"].append(ifmap);
			Json::Value destination;
			destination["id"] = core_id;
			destination["type"] = "core";
			destination["workload_id"] = workload["workload_id"];

			if(DRAM_ofmap_pos.count(key)){
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
						Cluster::xyid_t to_id = Cluster::get_xyid(next_part.second);
						fmap_range next_range = next_part.first;
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
			//ofmap["block"] = (range.size() + 10239) / 10240;
			ofmap["block"] = 10;
			ofmap["size"] = range.size() * 8;
			workload["buffer"].append(ofmap);
		}

		for(len_t batch=range.b.from; batch<range.b.to; ++batch)
			wlid[core_id][layerid][batch] = workload_list[core_id].size();

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
