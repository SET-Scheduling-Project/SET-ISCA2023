#include "cluster.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

mlen_t Cluster::xlen,Cluster::ylen,Cluster::stride;
double Cluster::min_util;

Cluster::Cluster(cidx_t _first, cidx_t _last){
	//use_range = true;
	range.first = _first;
	range.last = _last;
	if(range.last <= range.first){
		throw std::invalid_argument("Cluster: last <= first!");
	}
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
	if(childNum <= 0){
		throw std::invalid_argument("Cluster::try_alloc : childNum must be positive.");
	}
	cidx_t totalNodes = num_cores();
	if(childNum > totalNodes) return nullptr;
	if(totOps <= 0){
		totOps = 0;
		for(cidx_t i=0; i<childNum; ++i){
			totOps += ops[i];
		}
	}

	auto* ratioList = new std::pair<double, cidx_t>[childNum];
	auto* isPlaced  = new bool[childNum]();
	Cluster::allocRes_t allocRes = std::make_unique<cidx_t[]>(childNum+1);

	cidx_t remainNodes = totalNodes;
	utime_t remainOps = totOps;

	double max_time = 0;
	while(remainOps > 0){
		assert(remainNodes > 0);
		cidx_t rListLen = 0;
		cidx_t addNodes = remainNodes;
		for (cidx_t i=0; i<childNum; ++i) {
			if(isPlaced[i]) continue;
			double idealNodes = (static_cast<double>(ops[i])/remainOps) * remainNodes;
			allocRes[i] = static_cast<cidx_t>(idealNodes); // Floor by default.
			addNodes -= allocRes[i];
			ratioList[rListLen++] = std::make_pair(allocRes[i]/idealNodes, i);
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
		assert(addNodes > 0 && addNodes <= rListLen);
		std::sort(ratioList, ratioList+rListLen);
		cidx_t j = 0;
		do{
			cidx_t cur_idx = ratioList[j++].second;
			remainNodes -= ++allocRes[cur_idx];
			remainOps -= ops[cur_idx];
			isPlaced[cur_idx] = true;
			double cur_mtime = ops[cur_idx] / static_cast<double>(allocRes[cur_idx]);
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

	cidx_t curCoreNum = allocRes[0], nextCoreNum;
	allocRes[0] = range.first;
	for (cidx_t i=0; i<childNum; ++i) {
		nextCoreNum = allocRes[i+1];
		allocRes[i+1] = allocRes[i] + curCoreNum;
		curCoreNum = nextCoreNum;
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
	if(num_chip >= num_cores()){
		std::string msg = "Cluster::operator[] : num_chip >= num_cores() (";
		msg += std::to_string(num_chip);
		msg += " >= ";
		msg += std::to_string(num_cores());
		msg += ")";
		throw std::out_of_range(msg);
	}
	/*
	if(!use_range){
		return core_list[static_cast<size_t>(num_chip)];
	}
	*/
	return get_pos(range.first + num_chip);
}

pos_t Cluster::get_pos(cidx_t core_idx){
	if(core_idx < 0 || core_idx >= xlen * ylen){
		std::string msg = "Cluster::get_pos : core_idx (";
		msg += std::to_string(core_idx);
		msg += ") out of range [0, ";
		msg += std::to_string(xlen * ylen);
		msg += ")";
		throw std::out_of_range(msg);
	}
	cidx_t str_id = (core_idx / (stride * ylen));
	mlen_t x = str_id * stride + (core_idx % stride);
	mlen_t y = (core_idx / stride) % ylen;
	bool up_down = (str_id % 2) == 1;
	y = up_down ? ((ylen-1)-y) : y;
	return {x, y};
}

Cluster::xyid_t Cluster::get_xyid(pos_t& chip){
	return chip.y * (xlen+2) + chip.x + 1;
}
