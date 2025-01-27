/* This file contains
 *	WholeSch: Records an RA Tree (LTreeNode + SchNode)
 *  SAEngine: Performs the SA algorithm
 */

#ifndef SA_H
#define SA_H

#include <cstdint>		// std::uint32_t, std::uint64_t
#include <iostream>		// std::ostream
#include <random>		// std::mt19937
#include <sstream>		// std::ostringstream

#include "util.h"

class Cluster;
class LTreeNode;
class SchNode;
//#include "cluster.h"
//#include "ltreenode.h"
//#include "schnode.h"


struct WholeSch{
	/*
	 * One RA Tree (two representations)
	 * tree: contains only structural info of the RA Tree
	 * sch:  contains all scheduling schemes of the RA Tree
	 */
	LTreeNode* tree;
	SchNode* sch;

	WholeSch();
	WholeSch(LTreeNode* _tree, SchNode* _sch);
	// Now delete is handled manually.
	// ~WholeSch(){ del();}

	operator bool() const;

	WholeSch copy() const;
	void del();
	// Takes minimal with another tree.
	// Will delete the other tree if it exists.
	void min(WholeSch& w_sch);
};

class SAEngine{
public:
	// Total #rounds of SA.
	static int nrounds;

private:
	// SA has 7 OPs (OP1 in SET paper is divided into two OPs <- this can be optimized)
	static constexpr int NUM_OP = 7;

	// Halves all batch sizes under node.
	static void halv_bat(LTreeNode* node);
	// Reduce all batch sizes under node to n_batch, do not change if less.
	static void flat_bat(LTreeNode* node, len_t n_batch = 1);

	// Current round
	int cur_round;

	// Statistic variables
	std::uint64_t num_tries;
	std::uint64_t cur_tries;

	// Random generator
	std::mt19937 generator;

	// Output buffer used in multithreading
	// (sync flush to avoid concurrent cout)
	std::ostringstream strStream;
	// Points to either cout or *strStream*.
	std::ostream& out;

	// Uniform int between [0, to)
	int randInt(int to);

	// Bernoulli variable with probability "prob".
	bool withProb(double prob);

	// Used for printing status each minute (in a separate ping thread).
	// void ping_func(volatile bool& stop) const;

public:
	SAEngine(std::uint32_t seed, bool directCout = false);

	// Prints buffered messages (in strStream) to cout
	void flushBuf();

	/*
	 * Main search function for SA
	 *
	 * w_sch:     inputs the initial RA Tree, outputs the final RA Tree
	 * c:         the total cluster, including all cores on hardware
	 * max_depth: controls the maximal allowed depth of the RA Tree (0 for no constraint)
	 * sa_type:   0 -> arbitrary. 1 -> LP(only s under top t). 2 -> LS(only t under top t).
	 */
	void SA_search(WholeSch& w_sch, const Cluster& c, lid_t max_depth=0, int sa_type=0);

	// Change current tree (according to the OPs in SA)
	LTreeNode* sa_change(LTreeNode* root, bool* valid_op, lid_t max_depth=0, int sa_type=0, int* op_type=nullptr);

	// Determines whether SA accepts new scheme.
	bool sa_accept(cost_t cur_cost, cost_t new_cost, int round);
};

/*
 * [Deprecated]
 * Search LP by Dynamic Programming (DP).
 * Deprecated since in general it's hard to perform DP on general RA Trees.
 */
void LP_search(lid_t num_layer, len_t tot_batch, Cluster& c, WholeSch& w_sch, bool has_S, bool has_T);

#endif // SA_H
