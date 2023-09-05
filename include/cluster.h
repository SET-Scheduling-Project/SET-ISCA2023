#ifndef CLUSTER_H
#define CLUSTER_H

#include <memory>
//#include <vector>
#include "util.h"

class Cluster{
public:
	/*
	struct core{
		pos_t pos;
		vol_t UBUF_vol;
	};
	*/
	typedef vol_t hop_t;
	typedef cidx_t xyid_t;
	typedef std::unique_ptr<cidx_t[]> allocRes_t;
	static mlen_t xlen,ylen,stride;
	static double min_util;
private:
	//const core* cores;
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
	/*
	static hop_t unicast(pos_t src, pos_t dst);
	// TODO: dst needs to be in inc. order.
	static hop_t multicast(pos_t src, pos_t* dst, cidx_t len);
	// DRAM is at (-1,x) and (n,x)
	static hop_t unicast_dram(pos_t dst, vol_t size);
	static hop_t unicast_to_dram(pos_t dst, vol_t size);
	static hop_t multicast_dram(pos_t* dst, cidx_t len, vol_t size);
	*/
	static pos_t get_pos(cidx_t core_idx);
	static xyid_t get_xyid(pos_t& chip);
	// bool first_chip(pos_t& chip) const;
	// bool next_chip(pos_t& chip) const;
	pos_t operator[](cidx_t num_chip) const;
	memidx_t nearest_dram() const;
};


#endif // CLUSTER_H
