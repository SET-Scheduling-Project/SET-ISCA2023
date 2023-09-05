#include "noc.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include "schnode.h"
#include "util.h"

energy_t NoC::hop_cost;
energy_t NoC::DRAM_acc_cost;
bw_t NoC::DRAM_bw;
bw_t NoC::NoC_bw;
std::vector<pos_t> NoC::dram_list;

NoC::NoC(bool _calc_bw): calc_bw(_calc_bw), tot_hops(0), tot_DRAM_acc(0){}

/*
void NoC::set_calc_bw(bool _calc_bw){
	calc_bw = _calc_bw;
	if(!calc_bw) link_hops.clear();
}
*/

void NoC::reset(){
	tot_hops = 0;
	tot_DRAM_acc = 0;
	link_hops.clear();
}

void NoC::fromRemoteMem(const DataLayout& toLayout){
	auto rLen = toLayout.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = toLayout.at(i);
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize);
		}
	}
}

void NoC::fromRemoteMem(const DataLayout& toLayout, len_t fromC, len_t toC){
	if(toC <= fromC) return;
	fmap_range::dim_range truncRange = {fromC, toC};

	auto rLen = toLayout.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = toLayout.at(i);
		fmap_range range = it.range;
		range.c = range.c.intersect(truncRange);
		vol_t curSize = range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize);
		}
	}
}

void NoC::toRemoteMem(const UniqueLayout& fromLayout){
	for(cidx_t i=0; i<fromLayout.totLength(); ++i){
		auto it = fromLayout[i];
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		unicast_to_dram(it.tile, curSize);
	}
}

void NoC::betweenLayout(const UniqueLayout& fromLayout, const DataLayout& toLayout, len_t fromCOffset, len_t fromB, len_t toB){
	hop_t h = 0;
	// TODO: change to generic UniqueLayout
	const auto* fLayout = dynamic_cast<const StdULayout*>(&fromLayout);
	if(fLayout == nullptr){
		// TODO: add general case.
		assert(false);
		return;
	}
	bool diffB = (fromB != toB);
	auto rLen = toLayout.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto toEntry = toLayout.at(i);
		fmap_range toRange = toEntry.range;
		if(toRange.c.to <= fromCOffset) continue;
		toRange.c -= fromCOffset;
		for(auto it = fLayout->get_intersect(toRange, diffB);it.isValid();it.next()){
			auto fromEntry = *it;
			vol_t v = calc_intersect(fromEntry.range, toRange, fromB, toB);
			if(v == 0) continue;
			if(toEntry.numTile == 1){
				h += unicastCalc(fromEntry.tile, *toEntry.tiles, v);
			}else{
				h += multicastCalc(fromEntry.tile, toEntry.tiles, toEntry.numTile, v);
			}
		}
	}
	if(fromB > toB)  h /= (fromB / toB);
	tot_hops += h;
}

NoC NoC::operator+(const NoC& other) const{
	NoC x = *this;
	return x += other;
}

NoC& NoC::operator+=(const NoC& other){
	tot_hops += other.tot_hops;
	tot_DRAM_acc += other.tot_DRAM_acc;
	if(calc_bw || other.calc_bw){
		assert(calc_bw && other.calc_bw);
		link_hops += other.link_hops;
	}
	return *this;
}

NoC NoC::operator*(const len_t& batch) const{
	NoC x = *this;
	return x *= batch;
}

NoC& NoC::operator*=(const len_t& batch){
	tot_hops *= batch;
	tot_DRAM_acc *= batch;
	if(calc_bw) link_hops *= batch;
	return *this;
}

NoC& NoC::operator/=(const len_t& batch){
	tot_hops /= batch;
	tot_DRAM_acc /= batch;
	if(calc_bw) link_hops /= batch;
	return *this;
}

NoC::hop_t NoC::get_tot_hops() const{
	return tot_hops;
}

energy_t NoC::get_hop_cost() const{
	return tot_hops*hop_cost;
}

energy_t NoC::get_cost() const{
	//std::cout << "GC " << tot_hops << ' ' << tot_DRAM_acc << std::endl;
	return tot_hops*hop_cost + tot_DRAM_acc*DRAM_acc_cost;
}
// TODO: add NoC time.
cycle_t NoC::get_time() const{
	cycle_t dram_time = DIVCEIL(tot_DRAM_acc, (4*DRAM_bw));
	if(!calc_bw) return dram_time;
	cycle_t noc_time = DIVCEIL(link_hops.max(), NoC_bw);
	return MAX(dram_time, noc_time);
}

void NoC::unicast(pos_t src, pos_t dst, vol_t size){
	tot_hops += unicastCalc(src, dst, size);
}

NoC::hop_t NoC::unicastCalc(pos_t src, pos_t dst, vol_t size){
	if(calc_bw){
		size_t x_dir = (dst.x > src.x)?0:2;
		size_t y_dir = (dst.y > src.y)?3:1;
		mlen_t dx = (dst.x > src.x)?1:-1;
		mlen_t dy = (dst.y > src.y)?1:-1;
		for(mlen_t x = src.x; x != dst.x; x+= dx){
			link_hops.get(x, src.y, x_dir) += size;
		}
		for(mlen_t y = src.y; y != dst.y; y+= dy){
			link_hops.get(dst.x, y, y_dir) += size;
		}
	}
	return static_cast<hop_t>(abs(src.x-dst.x)+abs(src.y-dst.y)) * size;
}

void NoC::multicast(pos_t src, const pos_t* dst, cidx_t len, vol_t size){
	tot_hops += multicastCalc(src, dst, len, size);
}

NoC::hop_t NoC::multicastCalc(pos_t src, const pos_t* dst, cidx_t len, vol_t size){
	mlen_t cur_x = dst[0].x;
	mlen_t min_y = dst[0].y;
	hop_t h = 0;
	if(calc_bw){
		for(mlen_t x = src.x; x > dst[0].x; --x){
			link_hops.get(x, src.y, 2) += size;
		}
		for(mlen_t x = src.x; x < dst[len-1].x; ++x){
			link_hops.get(x, src.y, 0) += size;
		}
	}
	h += MAX(src.x, dst[len-1].x) - MIN(src.x, dst[0].x);

	for(cidx_t i=1; i<=len; ++i){
		if(i<len && dst[i].x == cur_x) continue;
		if(calc_bw){
			for(mlen_t y = src.y; y > min_y; --y){
				link_hops.get(cur_x, y, 1) += size;
			}
			for(mlen_t y = src.y; y < dst[i-1].y; ++y){
				link_hops.get(cur_x, y, 3) += size;
			}
		}
		h += MAX(src.y, dst[i-1].y) - MIN(src.y, min_y);
		if(i == len) break;
		cur_x = dst[i].x;
		min_y = dst[i].y;
	}
	return h * size;
}

void NoC::unicast_dram(pos_t dst, vol_t size){
	size_t llen = dram_list.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: dram_list){
		vol_t to_size = (size * ++i) / llen;
		unicast(dram, dst, to_size - from_size);
		from_size = to_size;
	}
	tot_DRAM_acc += size;
}

void NoC::unicast_to_dram(pos_t dst, vol_t size){
	size_t llen = dram_list.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: dram_list){
		vol_t to_size = (size * ++i) / llen;
		unicast(dst, dram, to_size - from_size);
		from_size = to_size;
	}
	tot_DRAM_acc += size;
}

void NoC::multicast_dram(const pos_t* dst, cidx_t len, vol_t size){
	size_t llen = dram_list.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: dram_list){
		vol_t to_size = (size * ++i) / llen;
		multicast(dram, dst, len, to_size - from_size);
		from_size = to_size;
	}
	tot_DRAM_acc += size;
}

std::vector<NoC::link_info> NoC::get_link_info() const{
	std::vector<link_info> info;
	if(!calc_bw) return info;
	info.reserve(link_hops.link_hops.size());
	for(const auto& it: link_hops.link_hops){
		size_t idx = it.first;
		size_t dir = idx % 4;
		idx /= 4;
		mlen_t y = idx % Cluster::ylen;
		idx /= Cluster::ylen;
		mlen_t x = idx;
		pos_t to;
		switch(dir){
		case 0:
			to = {static_cast<mlen_t>(x+1), y};
			break;
		case 1:
			to = {x, static_cast<mlen_t>(y-1)};
			break;
		case 2:
			to = {static_cast<mlen_t>(x-1), y};
			break;
		case 3:
			to = {x, static_cast<mlen_t>(y+1)};
			break;
		default:
			assert(false);
		}
		info.push_back({{x, y}, to, it.second * link_hops.factor});
	}
	// Sort in descending.
	std::sort(info.rbegin(), info.rend());
	return info;
}

NoC::hop_t NoC::get_max_link() const{
	return link_hops.max();
}

access_t NoC::get_tot_DRAM_acc() const{
	return tot_DRAM_acc;
}

vol_t NoC::calc_intersect(const fmap_range& rng1, const fmap_range& rng2, len_t bat1, len_t bat2){
	fmap_range ints = rng1.intersect(rng2);
	if(bat1 == bat2) return ints.size();

	len_t sb_st, sb_ed, lb_st, lb_ed, tot_b=0;
	if(bat1 > bat2){
		assert(bat1 % bat2 == 0);
		sb_st = rng2.b.from;
		sb_ed = rng2.b.to;
		lb_st = rng1.b.from;
		lb_ed = rng1.b.to;
		for(;sb_st < lb_ed; sb_st+=bat2, sb_ed+=bat2){
			if(sb_ed <= lb_st) continue;
			tot_b += MIN(sb_ed, lb_ed) - MAX(sb_st, lb_st);
		}
	}else{
		assert(bat2 % bat1 == 0);
		sb_st = rng1.b.from;
		sb_ed = rng1.b.to;
		lb_st = rng2.b.from;
		lb_ed = rng2.b.to;
		for(;sb_st < lb_ed; sb_st+=bat1, sb_ed+=bat1){
			if(sb_ed <= lb_st) continue;
			tot_b += MIN(sb_ed, lb_ed) - MAX(sb_st, lb_st);
		}
	}
	ints.b.from=0;
	ints.b.to=tot_b;
	return ints.size();
}

std::ostream& operator<<(std::ostream& os, const NoC& noc){
	return os<<"NoC(hops="<<noc.tot_hops<<", DRAM acc="<<noc.tot_DRAM_acc<<")";
}

NoC::HopCount::HopCount():factor(1){}

NoC::HopCount& NoC::HopCount::operator+=(const HopCount& other){
	if(factor > 1){
		for(const auto& it: link_hops){
			link_hops[it.first] *= factor;
		}
	}
	for(const auto& it : other.link_hops){
		link_hops[it.first] += it.second * other.factor;
	}
	return *this;
}

NoC::HopCount& NoC::HopCount::operator*=(const len_t& batch){
	factor *= batch;
	return *this;
}

NoC::HopCount& NoC::HopCount::operator/=(const len_t& batch){
	if(factor % batch == 0){
		factor /= batch;
	}else{
		for(const auto& it: link_hops){
			link_hops[it.first] = (it.second * factor) / batch;
		}
		factor = 1;
	}
	return *this;
}

NoC::hop_t& NoC::HopCount::get(mlen_t x, mlen_t y, mlen_t dir){
	size_t idx = (x * Cluster::ylen + y) * 4 + dir;
	assert(idx < 4u*Cluster::xlen*Cluster::ylen);
	return link_hops[idx];
}

void NoC::HopCount::clear(){
	link_hops.clear();
}

NoC::hop_t NoC::HopCount::max() const{
	hop_t h = 0;
	for(const auto& it: link_hops){
		h = MAX(h, it.second);
	}
	return h * factor;
}

bool NoC::link_info::operator<(const link_info& other) const{
	if(total_hops != other.total_hops) return total_hops < other.total_hops;
	if(from != other.from) return from < other.from;
	return to < other.to;
}

bool NoC::link_info::operator==(const link_info& other) const{
	return total_hops == other.total_hops && from == other.from && to == other.to;
}

bool NoC::link_info::operator>(const link_info& other) const{
	if(total_hops != other.total_hops) return total_hops > other.total_hops;
	if(from != other.from) return from > other.from;
	return to > other.to;
}

std::ostream& operator<<(std::ostream& os, const NoC::link_info& info){
	return os<<info.from<<" -> "<<info.to<<'\t'<<info.total_hops;
}


