#include "schnode.h"

#include <cassert>

#include "layerengine.h"
#include "network.h"
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

void SchNode::inplace_search(LTreeNode* node){
	searchInc(node);
	if(!valid) return;
	NoC tmp;
	updateNoc(tmp);
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
	memLayouts = std::move(res.memLayouts);
	cost = res.totCost;
	core_cost = res.coreCost;
	return true;
}

LNode::LNode(LTreeNode *_node, const Cluster& _c, SchNode::cut_ptr _parent)
	:SchNode(NodeType::L, _c, _parent, _node->get_tot_batch()), layerid(_node->layers().first()),
	  layert(network->getNode(layerid)), /*place_sch(cluster, layert, _node->get_tot_batch()),*/
	dirp_set(_node->get_dirp_set()), to_dram(_node->get_to_dram()){
	searchLayer();
}

LNode::~LNode(){
	auto& ptr = (*lnodeList)[layerid];
	if(ptr == this) ptr = nullptr;
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
	if(!place_sch.getOfmL().update_ofm(buf_usage, tileSch.tile_part)){
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

bool LNode::updateNoc(NoC& old_noc){
	energy_t old_noc_cost = noc.get_cost();
	if(!layerMapper->updateNoC(this, old_noc)) return false;

	cost.energy -= old_noc_cost;
	cost.energy += noc.get_cost();

	bool is_seg = (parent == nullptr) || parent->is_DRAM_cut();
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(core_cost.time, noc_time);
	}else{
		cycle_t noc_time = noc.get_dram_time();
		cost.time = MAX(core_cost.time, noc_time);
	}
	return true;
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

const MemLayouts& LNode::get_MemLayouts() const {
	return memLayouts;
}

const std::vector<MemLayout>& LNode::get_iMemLayouts() const {
	return memLayouts.iMemLayouts;
}

const MemLayout& LNode::get_wMemLayout() const {
	return memLayouts.wMemLayout;
}

const MemLayout& LNode::get_oMemLayout() const {
	return memLayouts.oMemLayout;
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
	os << " (";
	for(const auto& it : memLayouts.oMemLayout.get_layouts()){
		os << it << ',';
	}
	os << ")";
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
	noc.clear();
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

bool Cut::updateNoc(NoC& old_noc){
	std::vector<lid_t> updated_index;
	std::vector<NoC> old_nocs;
	lid_t child_idx = 0;
	for(auto child: children){
		if(child->updateNoc(old_noc)){
			updated_index.push_back(child_idx);
			old_nocs.push_back(std::move(old_noc));
		}
		++child_idx;
	}

	if(updated_index.empty())
		return false;

	if(3 * updated_index.size() >= children.size()){
		// More than 1/3 is changed, just re-add.
		old_noc = std::move(noc);
		noc.clear();
		for(auto child: children){
			noc += child->get_noc();
		}
		noc *= num_bgrp;
	}else{
		// Less than 1/3 is changed, just modify on original.
		old_noc = noc;
		noc /= num_bgrp;
		for(lid_t i = 0; i<updated_index.size(); ++i){
			auto cur_idx = updated_index[i];
			noc -= old_nocs[i];
			noc += children[cur_idx]->get_noc();
		}
		noc *= num_bgrp;
	}
	// Update cost in SCut/TCut::updateNoC
	return true;
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
		cost += p->get_cost();
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

bool TCut::updateNoc(NoC& old_noc){
	if(!Cut::updateNoc(old_noc)) return false;

	bool is_top = (parent == nullptr);
	bool is_seg = (!is_top) && parent->is_DRAM_cut();

	cost.time = 0;
	cost.energy = 0;
	for(auto child: children){
		cost += child->get_cost();
	}
	cost *= num_bgrp;
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(cost.time, noc_time);
	}
	return true;
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

bool SCut::updateNoc(NoC& old_noc){
	if(!Cut::updateNoc(old_noc)) return false;

	bool is_seg = (parent == nullptr) || parent->is_DRAM_cut();
	cycle_t max_time = 0;

	cost.energy = 0;
	for(auto child: children){
		cost.energy += child->get_cost().energy;
		max_time = MAX(child->get_cost().time, max_time);
	}
	cost.time = max_time * (num_stage + num_bgrp);
	cost.energy *= num_bgrp;
	if(is_seg){
		cycle_t noc_time = noc.get_time();
		cost.time = MAX(cost.time, noc_time);
	}
	return true;
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
