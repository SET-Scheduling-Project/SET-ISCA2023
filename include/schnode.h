/* This file contains
 *	SchNode: [base class] Node of a "full" RA Tree scheme. (Contains both
 *      structural information and scheduling scheme & costs)
 *  LNode:   [derived from SchNode] Node of type L
 *  Cut:     [derived from SchNode] Node of type S/T
 *  TCut:    [derived from Cut] Node of type T
 *  SCut:    [derived from Cut] Node of type S
 *
 *  All inter-layer cost calculations are in schnode.cpp
 */


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

	// Cost, including energy and latency(time)
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
	bool valid;              // whether scheme is valid
	const NodeType type;     // type of node
	const len_t num_batch;   // batch num (b_i in SET paper)
	const Cluster cluster;   // core cluster (TG_i in SET paper)
	const Cut* const parent; // parent of current node
	SchCost cost;            // total cost of current node
	NoC noc;                 // noc information

	// Occupied buffer size, see doc for more documentation.
	BufferUsage buf_usage, ifm_usage, wgt_usage;
	// Total energy of ubuf/buffer/bus/mac (in intra-core)
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

	// Incremental search (reuse old results if possible)
	virtual void searchInc(LTreeNode* node) =0;

	// Copy and return a new SchNode from this.
	virtual SchNode* copy(Cut* newParent = nullptr) const =0;
	// Checks whether this SchNode contains a layer or not.
	virtual bool contains(lid_t layerid) const =0;

	bool is_valid() const;
	// A "root T cut" is a DRAM cut.
	bool is_DRAM_cut() const;

	// Getter functions

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

#ifndef NOT_GEN_IR
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
#endif
};

class LNode : public SchNode{
	friend class StdLayerEngine;

private:
	lid_t layerid;                   // Index of represented layer
	const Node& layert;              // Reference to represented layer
	PlaceSch place_sch;              // Placement (and partition) scheme
	const Bitset dirp_set;           // Direct prev layers (for shortcut)
	const bool to_dram;              // whether writes results to DRAM
	CoreMapper::CoreMapping tileSch; // scheduling scheme of this layer (tiling, etc.)

	// Search for intra-layer scheme
	bool search();

public:
	LNode(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~LNode() override;

	// Search for intra-layer scheme, then update buffer usage and cost.
	void searchLayer();

	virtual void searchInc(LTreeNode* node) override;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;
	virtual bool contains(lid_t _layerid) const override;

	// Getter functions

	const Node& getLayer() const;
	const PlaceSch& get_place_sch() const;
	const Bitset& get_dirp_set() const;
	bool get_to_dram() const;

	virtual void print_struct(std::string pad = "", std::ostream& os = std::cout) const override;
	virtual void print_tree(std::string pad = "", std::ostream& os = std::cout) const override;

#ifndef NOT_GEN_IR
	// **************** Code for IR generation ****************
	static const Cut* get_lca(const LNode* node1, const LNode* node2);
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
	virtual const LNode* get_lnode_by_id(lid_t id) const override;
#endif
};

class Cut : public SchNode{
private:
	// Used for incremental search
	sn_vec oldChildren;
	LTreeNode* curNode;

protected:
	const Bitset layers; // All layers in this node (L_i in SET paper)
	sn_vec children;     // Childrens of this node (C_i in SET paper)
	len_t num_bgrp;      // Number of batch groups (sb_i in SET paper)

	// Iteratively construct all childs while updating *this
	virtual void construct(LTreeNode* node) =0;

public:
	Cut(NodeType t, LTreeNode* node, const Cluster& _c, cut_ptr _parent);
	virtual ~Cut() override;

	// Constructs a new child corresponding to "_node".
	// See definition for more details.
	sn_ptr newNode(LTreeNode *_node, const Cluster& _c);

	// Adds new child to end of *children*.
	void add(SchNode* child);

	virtual void searchInc(LTreeNode* node) override;

	virtual SchNode* copy(Cut* newParent = nullptr) const override =0;
	virtual bool contains(lid_t layerid) const override;

	// Getter functions

	const sn_vec& getChildren() const;
	len_t get_num_bgrp() const;

	virtual void print_struct(std::string pad = "", std::ostream& os = std::cout) const override;
	virtual void print_tree(std::string pad = "", std::ostream& os = std::cout) const override;

#ifndef NOT_GEN_IR
	// **************** Code for IR generation ****************
	virtual const LNode* get_lnode_by_id(lid_t id) const override;
	friend const Cut* LNode::get_lca(const LNode* node1, const LNode* node2);
#endif
};

class TCut : public Cut{
private:
	virtual void construct(LTreeNode* node) override;

public:
	TCut(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~TCut() override = default;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;

#ifndef NOT_GEN_IR
	// **************** Code for IR generation ****************
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
#endif
};

class SCut : public Cut{
private:
	const std::vector<lid_t> stage; // Pipeline stage of each child
	const lid_t num_stage;          // Number of pipeline stages (minus 1)

	virtual void construct(LTreeNode* node) override;

public:
	SCut(LTreeNode* _node, const Cluster& _c, cut_ptr _parent);
	virtual ~SCut() override = default;

	virtual SchNode* copy(Cut* newParent = nullptr) const override;

#ifndef NOT_GEN_IR
	// **************** Code for IR generation ****************
	virtual void add_workload_and_dfs(len_t batch_offset, len_t segment, std::vector<Json::Value>& workload_list) const override;
#endif
};

#endif // SCHNODE_H
