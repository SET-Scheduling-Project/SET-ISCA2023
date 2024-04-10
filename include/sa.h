#ifndef SA_H
#define SA_H

#include "cluster.h"
#include "ltreenode.h"
#include "schnode.h"

#include <cstdint>		// std::uint32_t, std::uint64_t
#include <iostream>		// std::ostream
#include <random>		// std::mt19937
#include <sstream>		// std::ostringstream


struct WholeSch{
	LTreeNode* tree;
	SchNode* sch;
	WholeSch();
	WholeSch(LTreeNode* _tree, SchNode* _sch);
	operator bool() const;
	WholeSch copy() const;
	void del();
	void min(WholeSch& w_sch);
	// ~WholeSch(){ del();}
};

class SAEngine{
public:
	static int nrounds;
private:
	static constexpr int NUM_OP = 7;
	int cur_round;
	std::uint64_t num_tries;
	std::uint64_t cur_tries;
	std::mt19937 generator;
	std::ostringstream strStream;
	std::ostream& out;
	// Generate int between [0, to)
	int randInt(int to);
	bool withProb(double prob);

	static void halv_bat(LTreeNode* node);
	static void flat_bat(LTreeNode* node, len_t n_batch = 1);
public:
	SAEngine(std::uint32_t seed, bool directCout = false);

	void SA_search(WholeSch& w_sch, const Cluster& c, lid_t max_depth=0, int sa_type=0);

	// sa_type: 0 -> arbitrary. 1 -> only s under top t. 2 -> only t under top t.
	LTreeNode* sa_change(LTreeNode* root, bool* valid_op, lid_t max_depth=0, int sa_type=0, int* op_type=nullptr);

	bool sa_accept(cost_t cur_cost, cost_t new_cost, int round);

	void flushBuf();

	// void ping_func(volatile bool& stop) const;
};

void LP_search(lid_t num_layer, len_t tot_batch, Cluster& c, WholeSch& w_sch, bool has_S, bool has_T);

#endif // SA_H
