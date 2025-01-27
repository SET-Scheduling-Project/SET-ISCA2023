#include "placement.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "cluster.h"


PlaceEngine placeEngine;

PlaceSch::PlaceSch(const PlaceSch& sch)
	:part(sch.part),
	 ifmLayout(sch.ifmLayout->clone()),
	 wgtLayout(sch.wgtLayout->clone()),
	 ofmLayout(sch.ofmLayout->clone())
{
	memcpy(order, sch.order, sizeof(order[0])*4);
}

DataLayout& PlaceSch::getIfmL(){
	return *ifmLayout.get();
}

const DataLayout& PlaceSch::getIfmL() const{
	return *ifmLayout.get();
}

DataLayout& PlaceSch::getWgtL(){
	return *wgtLayout.get();
}

const DataLayout& PlaceSch::getWgtL() const{
	return *wgtLayout.get();
}

UniqueLayout& PlaceSch::getOfmL(){
	return *ofmLayout.get();
}

const UniqueLayout& PlaceSch::getOfmL() const{
	return *ofmLayout.get();
}

void PlaceSch::initPlacement(const Cluster& cluster){
	using plen_t = PartSch::partlen_t;

	pos_t* curIdx = permuteOrder.get();

	plen_t step[4] = {1,1,1,1};
	step[order[3]] = 1;
	step[order[2]] = part[order[3]];
	step[order[1]] = part[order[3]] * part[order[2]];
	step[order[0]] = part[order[3]] * part[order[2]] * part[order[1]];
	for(plen_t k = 0; k < part.K; ++k){
		for(plen_t b = 0; b < part.B; ++b){
			for(plen_t h = 0; h < part.H; ++h){
				for(plen_t w = 0; w < part.W; ++w){
					// Init (b, k, h, w)
					plen_t idx = step[0] * k + step[1] * b + step[2] * h + step[3] * w;
					*(curIdx++) = cluster[idx];
				}
			}
		}
	}
}

void PlaceSch::update(PlaceSch&& sch){
	part = sch.part;
	memcpy(order, sch.order, sizeof(order[0])*4);
}

void PlaceSch::finalize(){
	ifmLayout->finalize();
	wgtLayout->finalize();
	ofmLayout->finalize();
	permuteOrder.reset();
}

std::ostream& operator<<(std::ostream& os, const PlaceSch& sch){
	os << '(';
	for(int i=0;i<4;++i){
		switch(sch.order[i]){
		case 0: os << "K:" << sch.part.K;
			break;
		case 1: os << "B:" << sch.part.B;
			break;
		case 2: os << "H:" << sch.part.H;
			break;
		case 3: os << "W:" << sch.part.W;
			break;
		default: os << "X:X";
			break;
		}
		if(i != 3) os << ',';
	}
	return os << ')';
}

PlaceIter PlaceEngine::init(PlaceSch& cur_sch){
	return PlaceIter(cur_sch);
}

PlaceIter::PlaceIter(PlaceSch& placeSch) : curSch(placeSch){
	std::uint8_t first = 0, last = 3;
	for(std::uint8_t i=0; i<4; ++i){
		if(curSch.part[i] == 1){
			curSch.order[last--]=i;
		}else{
			curSch.order[first++]=i;
		}
	}
	assert(first == ++last);
	perm_len = first;
	hasNext = true;
}

bool PlaceIter::nextPlace(cost_t cost){
	(void) cost;
	return hasNext = std::next_permutation(curSch.order, curSch.order+perm_len);
}

PlaceIter::operator bool() const{
	return hasNext;
}
