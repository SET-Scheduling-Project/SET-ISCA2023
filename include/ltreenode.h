/* This file contains
 *	LTreeNode: Node of a "structural" RA Tree. (Contains only
 *      structural information like parent/child, type, num_batch,
 *      but no scheduling-scheme-related informations like
 *      core cluster, tiling, noc, ...)
 */

#ifndef LTREENODE_H
#define LTREENODE_H

#include <vector>

#include "bitset.h"
#include "util.h"

class SAEngine;
//#include "sa.h"


class LTreeNode{
	friend class SAEngine;

public:
	enum class NodeType : std::uint8_t{
		S, // SCut
		T, // TCut
		L  // LNode
	};

	typedef std::vector<LTreeNode*> node_vec;

private:
	// Type of node.
	NodeType t;

	// "new" and "modified" tags in incremental search.
	bool isNewNode, modified;

	// Tree properties.
	len_t height;
	LTreeNode* parent;
	node_vec children;

	// Node properties.
	Bitset layer_set;
	utime_t unit_time;
	len_t num_bgrp, num_batch;

	// Only for SCut.
	std::vector<lid_t> stage;
	// Number of pipeline stages (minus 1)
	lid_t num_stage;

	// Only for LNode.
	bool to_dram;
	// Direct prevs.
	Bitset dirp_set;

	// Adds a child to the tail of children.
	void add(LTreeNode* child);

	// Checks whether "from -> to" is shortcut.
	// Also sets "to_dram" of "from" accordingly.
	static bool is_shortcut(lid_t from_id, const LTreeNode& to);

	// traverse_pass1/traverse_pass2: Used in init_root()
	// Since "layer_set" is set after traverse_pass1(), we need two passes to init.

	// traverse_pass1: sets "t", "stage", "num_stage", "modified" and "layer_set".
	//     *calc_type*: if set, auto deduce type "t".
	void traverse_pass1(bool calc_type = false);

	// traverse_pass2: sets "num_bgrp", "unit_time", "height", "to_dram" and "dirp_set".
	void traverse_pass2();

public:
	LTreeNode(const Bitset& _layer_set, len_t _num_batch, LTreeNode* _parent=nullptr, NodeType _t=NodeType::L);
	LTreeNode(lid_t _layer, len_t _num_batch, LTreeNode* _parent=nullptr);
	LTreeNode(const LTreeNode& node)=default;
	~LTreeNode();

	// Initialize the whole tree from the root.
	void init_root();

	// Used in incremental search.
	bool isModified() const;
	bool isNew() const;
	// Confirm the tree, reset "new" and "modified" tags.
	void confirm();

	// Copy a new tree.
	LTreeNode* copy() const;

	// Reset layer_set (for re-calculation)
	void reset_lset();

	// Getter functions.
	NodeType get_type();
	const node_vec& get_children();
	const Bitset& layers();
	utime_t get_utime() const;
	len_t get_bgrp_num() const;
	len_t get_bgrp_size() const;
	len_t get_tot_batch() const;
	const std::vector<lid_t>& get_stages() const;
	lid_t get_num_stage() const;
	bool get_to_dram() const;
	const Bitset& get_dirp_set() const;
};

#endif // LTREENODE_H
