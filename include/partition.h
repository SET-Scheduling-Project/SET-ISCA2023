/* This file contains
 *	PartSch:    Records a partition scheme.
 *  PartEngine: Generates valid partition schemes.
 *  PartIter:   Used to iterate through all valid partition schemes.
 */

#ifndef PARTITION_H
#define PARTITION_H

#include <iostream>
#include <vector>

#include "util.h"

class Node;
//#include "network.h"


struct PartSch{
	typedef len_t partlen_t;
	len_t K, B, H, W;

	explicit PartSch()=default;
	PartSch(len_t _K, len_t _B, len_t _H, len_t _W);

	// (*this)[0/1/2/3] = K/B/H/W
	len_t& operator[](std::uint8_t i);
	const len_t& operator[](std::uint8_t i) const;

	// Total size of partition: K * B * H * W
	vol_t size() const;

	friend std::ostream& operator<<(std::ostream& os, const PartSch& sch);
};

class PartIter;

class PartEngine{
	friend PartIter;

	/* ########## Static cached constants & helper funcs ########## */

	typedef std::uint16_t factor_t;
	typedef std::vector<PartSch> fvec;
	struct num_pair{
		factor_t x,y;
	};

	static constexpr auto MAX_BUF = MAX_CHIPS;
	// factors[i]: All partitions of i cores.
	static fvec factors[MAX_BUF+1];

	// utils[i][j] = i / (ceil(i/j) * j), util of running i identical jobs on j cores.
	static double utils[MAX_BUF+1][MAX_BUF+1];

	// Returns all pair (a, b) which mults to n.
	static std::vector<num_pair> factor_num(factor_t n);

	// Initialize factors and utils.
	static void init_all();

	/* ########## Class Members ########## */

	// Minimal acceptable utilization
	double min_util;

public:
	PartEngine(double _min_util=0.75);

	// Returns the iterator for partition schemes of *cluster_size*, partition will be iterated in *sch*.
	PartIter init(cidx_t cluster_size, len_t batch_num, const Node& layer, PartSch& sch, len_t min_cuts);
}extern partEngine; // Global partEngine to use.

class PartIter{
	friend PartEngine;

	typedef PartEngine::fvec fvec;
	typedef fvec::const_iterator citer;

	PartSch& curSch;
	citer nextPos, endPos;
	len_t maxB, maxK, maxH, maxW, min_ncut;
	double min_util;
	bool finished;

	PartIter(PartSch& partSch, double _min_util=0.75);

	// Calculates the utilization of *nextSch*
	bool calcUtil(const PartSch& nextSch) const;

	// Returns PartEngine::utils[real][part], see PartEngine::utils for details.
	static double calc_util(len_t real, len_t part);

	// Get the best partition scheme (if no scheme is found)
	bool getBestPart(cost_t cost = cost_inf);

	// Similar to calcUtil, but also updates maximal util.
	bool calc_util_best(const PartSch& cur_sch);

public:
	// Next function for iterator, returns whether iterator is valid.
	bool nextPart(cost_t cost = cost_inf);

	/**
	 * @brief operator bool: same as "eof" in an IO stream.
	 * Returns whether iterator is valid.
	 */
	operator bool() const;
};

#endif // PARTITION_H
