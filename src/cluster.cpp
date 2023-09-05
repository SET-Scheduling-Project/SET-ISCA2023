#include "cluster.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

mlen_t Cluster::xlen,Cluster::ylen,Cluster::stride;
double Cluster::min_util;
//cidx_t Cluster::tmp_nlist[MAX_CHIPS];

Cluster::Cluster(cidx_t _first, cidx_t _last){
	//use_range = true;
	range.first = _first;
	range.last = _last;
	//printf("%d\t%d\n",first, last);
	assert(range.last > range.first);
}

bool Cluster::operator==(const Cluster& other) const{
	return range.first == other.range.first && range.last == other.range.last;
}

bool Cluster::operator!=(const Cluster& other) const{
	return !operator==(other);
}

cidx_t Cluster::num_cores() const{
	return range.last - range.first;
}

Cluster::allocRes_t Cluster::try_alloc(utime_t* ops, cidx_t childNum, utime_t totOps) const{
	cidx_t totalNodes = num_cores();
	if(childNum > totalNodes) return nullptr;
	if(totOps <= 0){
		totOps = 0;
		for(cidx_t i=0; i<childNum; ++i){
			totOps += ops[i];
		}
	}

	// TODO: (low priority) check whether changing this to stack saves time.
	auto* ratioList = new std::pair<double, cidx_t>[childNum];
	auto* isPlaced  = new bool[childNum]();
	Cluster::allocRes_t allocRes = std::make_unique<cidx_t[]>(childNum+1);
	// memset(isPlaced, 0, sizeof(bool)*static_cast<size_t>(childNum));
	// memset(tmp_nlist, 0, sizeof(cidx_t)*static_cast<size_t>(childNum));

	cidx_t remainNodes = totalNodes;
	utime_t remainOps = totOps;

	double max_time = 0;
	while(remainOps > 0){
		assert(remainNodes > 0);
		/*
		if (remainNodes == 0){
			delete[] isPlaced;
			delete[] ratioList;
			delete[] allocRes;
			return nullptr;
		}
		*/
		cidx_t j = 0;
		cidx_t addNodes = remainNodes;
		for (cidx_t i=0; i<childNum; ++i) {
			if(isPlaced[i]) continue;
			double idealNodes = (static_cast<double>(ops[i])/remainOps) * remainNodes;
			allocRes[i] = static_cast<cidx_t>(idealNodes); // Floor by default.
			addNodes -= allocRes[i];
			ratioList[j++] = std::make_pair(allocRes[i]/idealNodes, i);
		}
		if(addNodes == 0){
			for (cidx_t i=0; i<childNum; ++i) {
				if(isPlaced[i]) continue;
				assert(allocRes[i] > 0);
				double cur_mtime = ops[i]/static_cast<double>(allocRes[i]);
				max_time = MAX(max_time, cur_mtime);
			}
			break;
		}
		assert(addNodes > 0);
		std::sort(ratioList, ratioList+j);
		j = 0;
		do{
			cidx_t cur_idx = ratioList[j++].second;
			remainNodes -= ++allocRes[cur_idx];
			remainOps -= ops[cur_idx];
			isPlaced[cur_idx] = true;
			double cur_mtime = ops[cur_idx]/static_cast<double>(allocRes[cur_idx]);
			max_time = MAX(max_time, cur_mtime);
		}while (--addNodes > 0);
	}

	delete[] isPlaced;
	delete[] ratioList;

	double utilization = totOps / (totalNodes * max_time);
	assert(utilization < 1 + 1e-6);
	if(utilization < min_util){
		allocRes.reset();
		return allocRes;
	}

	cidx_t curTNum = allocRes[0], nextTNum;
	allocRes[0] = range.first;
	for (cidx_t i=0; i<childNum; ++i) {
		nextTNum = allocRes[i+1];
		allocRes[i+1] = allocRes[i] + curTNum;
		curTNum = nextTNum;
	}
	assert(allocRes[childNum] == range.last);
	return allocRes;
}

Cluster Cluster::sub_cluster(cidx_t childIdx, const allocRes_t& allocRes) const{
	return Cluster(allocRes[childIdx], allocRes[childIdx+1]);
}

Cluster Cluster::sub_cluster(cidx_t from, cidx_t num) const{
	from += range.first;
	assert(from + num <= range.last);
	return Cluster(from, from+num);
}

pos_t Cluster::operator[](cidx_t num_chip) const{
	assert(num_chip < num_cores());
	//if(use_range){
		return get_pos(range.first + num_chip);
	//}
	//return core_list[static_cast<size_t>(num_chip)];
}

memidx_t Cluster::nearest_dram() const{
	// TODO:
	assert(false);
	return -1;
}

/*
Cluster::hop_t Cluster::unicast(pos_t src, pos_t dst){
	return static_cast<hop_t>(abs(src.x-dst.x)+abs(src.y-dst.y));
}

static inline Cluster::hop_t calc_intd(mlen_t s, mlen_t c, mlen_t b){
	return static_cast<Cluster::hop_t>(MAX(c,b) - MIN(s,c));
}

// TODO: better realization.
Cluster::hop_t Cluster::multicast(pos_t src, pos_t* dst, cidx_t len){
	mlen_t cur_x = dst[0].x;
	mlen_t min_y = dst[0].y;
	hop_t h = calc_intd(dst[0].x, src.x, dst[len-1].x);
	for(cidx_t i=1; i<len; ++i){
		if(dst[i].x != cur_x){
			cur_x = dst[i].x;
			h+=calc_intd(min_y, src.y, dst[i-1].y);
			min_y = dst[i].y;
		}
	}
	h+=calc_intd(min_y, src.y, dst[len-1].y);
	return h;
}

Cluster::hop_t Cluster::unicast_dram(pos_t dst, vol_t size){
	return (static_cast<hop_t>((xlen+1)*ylen+(ylen-1)*(ylen-2*dst.y)+2*dst.y*dst.y)*size)/static_cast<hop_t>(2*ylen);
}

Cluster::hop_t Cluster::unicast_to_dram(pos_t dst, vol_t size){
	return unicast_dram(dst, size);
}

// TODO: better multicast.
// TODO: multicast can also do unicast.
Cluster::hop_t Cluster::multicast_dram(pos_t* dst, cidx_t len, vol_t size){
	hop_t tot_hop=0;
	for(cidx_t i=0; i<len; ++i){
		tot_hop += unicast_dram(dst[i], size);
	}
	return tot_hop;
}
*/
pos_t Cluster::get_pos(cidx_t core_idx){
	assert(core_idx >= 0 && core_idx <xlen*ylen);
	cidx_t str_id = (core_idx / (stride*ylen));
	bool up_down = str_id%2==1;
	mlen_t x = str_id*stride+core_idx%stride;
	mlen_t y = (core_idx / stride) % ylen;
	y = up_down?((ylen-1)-y):y;
	return {x, y};
}

Cluster::xyid_t Cluster::get_xyid(pos_t& chip){
	return chip.y*(xlen+2)+chip.x+1;
}
