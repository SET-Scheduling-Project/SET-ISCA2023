/* This file contains
 *	Cluster: a class that records chips in a cluster.
 */

#ifndef CLUSTER_H
#define CLUSTER_H

#include <memory>
//#include <vector>
#include "util.h"

class Cluster{
public:
	// Traditional XY index of a chip. (e.g. y * xlen + x)
	typedef cidx_t xyid_t;

	// xlen/ylen: The x and y length of the cluster
	// stride:    Used in the strided allocation algorithm.
	// min_util:  Minimal utilization (in alloc algorithm).
	static mlen_t xlen, ylen, stride;
	static double min_util;

private:
	// Range of the cluster, a cluster has chips with cidx:
	// first, first+1, ..., last-1
	struct{
		cidx_t first, last;
	}range;

	// NOTE: use core_list in the future,
	//       when cores are not allocated in rectangular fashion.
	// bool use_range; // "true": using range; "false": using core_list.
	// std::vector<pos_t> core_list;

public:
	Cluster(cidx_t _first, cidx_t _last);

	bool operator==(const Cluster& other) const;
	bool operator!=(const Cluster& other) const;

	cidx_t num_cores() const;
	pos_t operator[](cidx_t num_chip) const;

	/* Allocation algorithm (see the definition of try_alloc for more details)
	 * SET applies a strided allocation algorithm.
	 * Details can be found in try_alloc.
	 *
	 * try_alloc: try to allocate a subcluster for each of the layers in a list.
	 * [try_alloc Inputs]
	 * ops:      The NPT of the layers.
	 * childNum: Number of layers.
	 * totOps:   Sum of ops (for acceleration).
	 *            When totOps <= 0, will re-initialize totOps to \sum(ops).
	 *
	 * [try_alloc Outputs]
	 * res: Records the allocation result. (Type: allocRes_t)
	 *       When the allocation fails, returns nullptr.
	 *       The ith subcluster can be retrieved by sub_cluster(i, res).
	 */
	typedef std::unique_ptr<cidx_t[]> allocRes_t;
	allocRes_t try_alloc(utime_t* ops, cidx_t childNum, utime_t totOps=0) const;
	Cluster sub_cluster(cidx_t childIdx, const allocRes_t& allocRes) const;
	Cluster sub_cluster(cidx_t from, cidx_t num) const;

	// Global functions for "cidx_t -> pos_t" and "pos_t -> xyid_t" mappings.
	static pos_t get_pos(cidx_t core_idx);
	static xyid_t get_xyid(pos_t& chip);
};

#endif // CLUSTER_H
