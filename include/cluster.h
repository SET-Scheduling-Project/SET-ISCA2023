#ifndef CLUSTER_H
#define CLUSTER_H

#include <memory>
//#include <vector>
#include "util.h"

class Cluster{
public:
	typedef cidx_t xyid_t;
	typedef std::unique_ptr<cidx_t[]> allocRes_t;
	static mlen_t xlen,ylen,stride;
	static double min_util;
private:
	struct{
		cidx_t first, last;
	}range;

	// TODO: use core_list in the future,
	//       when cores are not allocated in rectangular fashion.
	//bool use_range;
	//std::vector<pos_t> core_list;

public:
	Cluster(cidx_t _first, cidx_t _last);
	bool operator==(const Cluster& other) const;
	bool operator!=(const Cluster& other) const;
	cidx_t num_cores() const;
	allocRes_t try_alloc(utime_t* ops, cidx_t childNum, utime_t totOps=0) const;
	Cluster sub_cluster(cidx_t childIdx, const allocRes_t& allocRes) const;
	Cluster sub_cluster(cidx_t from, cidx_t num) const;
	static pos_t get_pos(cidx_t core_idx);
	static xyid_t get_xyid(pos_t& chip);
	pos_t operator[](cidx_t num_chip) const;
};


#endif // CLUSTER_H
