#ifndef SCHNODE_H
#define SCHNODE_H

#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>

#include "bitset.h"
#include "bufferusage.h"
#include "cluster.h"
#include "coremapping.h"
#include "ltreenode.h"
#include "noc.h"
#include "placement.h"
#include "util.h"

class LayerEngine;
class StdLayerEngine;
namespace Json{
	class Value;
};
//#include "layerengine.h"
//#include "json/json.h"


class LNode;
class Cut;

class SchNode{
public:
	typedef SchNode* sn_ptr;
	typedef const SchNode* csn_ptr;
	typedef SchNode& sn_ref;
	typedef std::deque<sn_ptr> sn_vec;

	typedef Cut* cut_ptr;

	typedef std::unordered_map<lid_t, LNode*> nodeList_t;
	typedef LTreeNode::NodeType NodeType;

	struct SchCost{
		energy_t energy;
		cycle_t time;

		SchCost(energy_t _energy=energy_inf, cycle_t _time=0);

		SchCost& operator+=(const SchCost& other);
		SchCost& operator*=(len_t other);
		bool operator!=(const SchCost& other) const;

		bool isValid() const;
		cost_t cost(len_t nbatch=1) const;

		friend std::ostream& operator<<(std::ostream& os, const SchCost& cost);
	};

	// The global layer engine.
	static LayerEngine* layerMapper;
	// The total batch size.
	static len_t tot_batch;

protected:
	bool valid;
	const NodeType type;
	const len_t num_batch;
	const Cluster cluster;
	const Cut* const parent;
	SchCost cost;
	NoC noc;
	BufferUsage buf_usage, ifm_usage, wgt_usage;
	energy_t ubuf_energy, buf_energy, bus_energy, mac_energy;
	// lnodeList points to a list of all SchNodes on the tree.
	// All SchNodes on the tree shares one lnodeList, managed by the root.
	nodeList_t* const lnodeList;

	SchNode(const SchNode& node) = default;

public:
	// Factory function.
	// Constructs a SchNode corresponding to the LTreeNode object "_node".
	// The derived class type and initilization will perform accordingly.
	static sn_ptr newNode(LTreeNode *_node, const Cluster& _c, Cut* parent);

	SchNode(NodeType t, const Cluster& _c, cut_ptr _parent, len_t nbatch);
	virtual ~SchNode() =0;
	// Used to set a new parent for this. See the implementation of copy().
	void setParent(Cut* newParent);

	// The main search function.
	virtual void searchInc(LTreeNode* node) =0;

	// Copy and return a new SchNode from this.
	virtual SchNode* copy(Cut* newParent = nullptr) const =0;
	// Checks whether this SchNode contains a layer or not.
	virtual bool contains(lid_t layerid) const =0;

	bool is_valid() const;
	bool is_DRAM_cut() const;

	// Getter functions.
	NodeType get_type() const;
	const Cluster& get_cluster() const;
	SchCost get_cost() const;
	const NoC& get_noc() const;
	const BufferUsage& get_buf_usage() const;
	const BufferUsage& get_ifm_usage() const;
	const BufferUsage& get_wgt_usage() const;
	energy_t get_ubuf_energy() const;
	energy_t get_buf_energy() const;
	energy_t get_bus_energy() const;
	energy_t get_mac_energy() const;

	// Print functions.
	virtual void print_struct(std::string pad = "", std::ostream& os = std::cout) const =0;
	virtual void print_tree(std::string pad = "", std::ostream& os = std::cout) const =0;
	void print_res(std::ostream& os = std::cout) const;
	friend std::ostream& operator<<(std::ostream& os, const SchNode& sch);
	friend std::ostream& operator<<(std::ostream& os, const SchNode* sch);

// **************** Code for IR generation ****************
protected:
	typedef std::uint32_t jsonindex_t;
	typedef std::uint32_t wlid_t;
	typedef std::uint32_t tfid_t;

	static wlid_t workload_cnt;
	static tfid_t transferid_cnt;
	static std::vector<std::vector<std::vector<jsonindex_t> > > wlid;
	static std::vector<bool> from_core, weight_from_core, to_dram;
	static std::vector<std::map<fmap_range, jsonindex_t> > ofmapid;
	static std::vector<std::set<Json::Value> > curr_ifmap;
	static std::vector<std::set<Json::Value> > curr_weight;
	static std::map<std::string,lid_t> name_to_id;
	static Json::Value DRAM;
	static std::map<Json::Value,jsonindex_t> DRAM_ofmap_pos;
	static std::map<Json::Value,jsonindex_t> DRAM_weight_pos;
	static std::map<tfid_t,jsonindex_t> DRAM_ifmap_pos;
	static csn_ptr root;

public:
	Json::Value IR_gen() const;
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const = 0;
	virtual const LNode* get_lnode_by_id(lid_t id) const = 0;
};

class LNode : public SchNode{
	friend class StdLayerEngine;
private:
	lid_t layerid;
	const Node& layert;
	PlaceSch place_sch;
	const Bitset dirp_set;
	const bool to_dram;
	CoreMapper::CoreMapping tileSch;

	bool search();

public:
	LNode(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~LNode() override;

	void searchLayer();
	virtual void searchInc(LTreeNode* node) override;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;
	virtual bool contains(lid_t _layerid) const override;

	const Node& getLayer() const;
	const PlaceSch& get_place_sch() const;
	const Bitset& get_dirp_set() const;
	bool get_to_dram() const;

	virtual void print_struct(std::string pad = "", std::ostream& os = std::cout) const override;
	virtual void print_tree(std::string pad = "", std::ostream& os = std::cout) const override;

	// **************** Code for IR generation ****************
	static const Cut* get_lca(const LNode* node1, const LNode* node2);
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
	virtual const LNode* get_lnode_by_id(lid_t id) const override;
};

class Cut : public SchNode{
private:
	sn_vec oldChildren;
	LTreeNode* curNode;

protected:
	const Bitset layers;
	sn_vec children;
	len_t num_bgrp;

	virtual void construct(LTreeNode* node) =0;

public:

	Cut(NodeType t, LTreeNode* node, const Cluster& _c, cut_ptr _parent);
	virtual ~Cut() override;
	// Constructs a new child corresponding to "_node".
	// See definition for more details.
	sn_ptr newNode(LTreeNode *_node, const Cluster& _c);

	void add(SchNode* child);

	virtual void searchInc(LTreeNode* node) override;

	virtual SchNode* copy(Cut* newParent = nullptr) const override =0;
	virtual bool contains(lid_t layerid) const override;

	const sn_vec& getChildren() const;
	len_t get_num_bgrp() const;

	virtual void print_struct(std::string pad = "", std::ostream& os = std::cout) const override;
	virtual void print_tree(std::string pad = "", std::ostream& os = std::cout) const override;

	// **************** Code for IR generation ****************
	virtual const LNode* get_lnode_by_id(lid_t id) const override;
	friend const Cut* LNode::get_lca(const LNode* node1, const LNode* node2);
};

class TCut : public Cut{
private:
	virtual void construct(LTreeNode* node) override;

public:
	TCut(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~TCut() override = default;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;

	// **************** Code for IR generation ****************
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
};

class SCut : public Cut{
private:
	const std::vector<lid_t> stage;
	const lid_t num_stage;

	virtual void construct(LTreeNode* node) override;

public:
	SCut(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~SCut() override = default;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;

	// **************** Code for IR generation ****************
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
};

#endif // SCHNODE_H
