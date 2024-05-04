#include "noc.h"

#include <algorithm>
#include <cassert>

#include "cluster.h"
#include "datalayout.h"
#include "memlayout.h"
#include "util.h"


energy_t NoC::hop_cost;
energy_t NoC::DRAM_acc_cost;
bw_t NoC::DRAM_bw;
bw_t NoC::NoC_bw;
std::vector<pos_t> NoC::dram_list;
bool NoC::unicast_only;
bool NoC::DRAM_interleave;
thread_local NoC NoC::_noc(false);

NoC::NoC(bool _calc_bw): calc_bw(_calc_bw), tot_hops(0), tot_DRAM_acc(0){}

/*
void NoC::set_calc_bw(bool _calc_bw){
	calc_bw = _calc_bw;
	if(!calc_bw) link_hops.clear();
}
*/

void NoC::clear(){
	tot_hops = 0;
	tot_DRAM_acc = 0;
	link_hops.clear();
}

void NoC::_fromRemoteMem(const std::vector<pos_t>& from, const DataLayout& to){
	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, from);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, from);
		}
	}
}

void NoC::_fromRemoteMem(const std::vector<pos_t>& from, const DataLayout& to, len_t fromC, len_t toC){
	fmap_range::dim_range truncRange = {fromC, toC};

	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		fmap_range range = it.range;
		range.c = range.c.intersect(truncRange);
		vol_t curSize = range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, from);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, from);
		}
	}
}

void NoC::fromRemoteMem(const DataLayout& to){
	if(DRAM_interleave){
		_fromRemoteMem(dram_list, to);
		return;
	}

	// Do not interleave, find nearest DRAM.
	pos_t d = nearest_dram_to(to);
	if(!calc_bw){
		tot_hops += _noc.tot_hops;
		tot_DRAM_acc += _noc.tot_DRAM_acc;
		return;
	}
	_fromRemoteMem({d}, to);
}

void NoC::fromRemoteMem(const MemLayout& from, const DataLayout& to){
	const auto& drams = from.get_layouts();
	_fromRemoteMem(drams, to);
}

void NoC::fromRemoteMem(const DataLayout& to, len_t fromC, len_t toC){
	if(toC <= fromC) return;
	if(DRAM_interleave){
		_fromRemoteMem(dram_list, to, fromC, toC);
		return;
	}

	// Do not interleave, find nearest DRAM.
	pos_t d = nearest_dram_to(to);
	if(!calc_bw){
		tot_hops += _noc.tot_hops;
		tot_DRAM_acc += _noc.tot_DRAM_acc;
		return;
	}
	_fromRemoteMem({d}, to, fromC, toC);
}

void NoC::fromRemoteMem(const MemLayout& from, const DataLayout& to, len_t fromC, len_t toC){
	if(toC <= fromC) return;
	const auto& drams = from.get_layouts();
	_fromRemoteMem(drams, to, fromC, toC);
}

void NoC::toRemoteMem(const UniqueLayout& from, MemLayout& to){
	if(DRAM_interleave){
		to.set_layout(dram_list);
	}else{
		pos_t d = nearest_dram(from);
		to.set_layout({d});
		if(!calc_bw){
			tot_hops += _noc.tot_hops;
			tot_DRAM_acc += _noc.tot_DRAM_acc;
			return;
		}
	}

	toRemoteMem_const(from, to);
}

void NoC::toRemoteMem_const(const UniqueLayout& from, const MemLayout& to){
	for(cidx_t i=0; i<from.totLength(); ++i){
		auto it = from[i];
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		unicast_to_dram(it.tile, curSize, to.get_layouts());
	}
}

void NoC::betweenLayout(const UniqueLayout& from, const DataLayout& to, len_t fromCOffset, len_t fromB, len_t toB){
	hop_t h = 0;
	// TODO: change to generic UniqueLayout
	const auto* fLayout = dynamic_cast<const StdULayout*>(&from);
	if(fLayout == nullptr){
		// TODO: add general case.
		assert(false);
		return;
	}
	bool diffB = (fromB != toB);
	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto toEntry = to.at(i);
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

void NoC::div(len_t batch){
	tot_hops /= batch;
	tot_DRAM_acc /= batch;
	if(calc_bw) link_hops.div(batch);
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
	link_hops.flat_factor();
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
	if(unicast_only){
		hop_t h = 0;
		for(cidx_t i=0; i<len; ++i){
			h += unicastCalc(src, dst[i], size);
		}
		return h;
	}
	link_hops.flat_factor();

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

void NoC::unicast_dram(pos_t dst, vol_t size, const std::vector<pos_t>& drams){
	size_t llen = drams.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: drams){
		vol_t to_size = (size * ++i) / llen;
		unicast(dram, dst, to_size - from_size);
		from_size = to_size;
	}
	tot_DRAM_acc += size;
}

void NoC::unicast_to_dram(pos_t dst, vol_t size, const std::vector<pos_t>& drams){
	size_t llen = drams.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: drams){
		vol_t to_size = (size * ++i) / llen;
		unicast(dst, dram, to_size - from_size);
		from_size = to_size;
	}
	tot_DRAM_acc += size;
}

void NoC::multicast_dram(const pos_t* dst, cidx_t len, vol_t size, const std::vector<pos_t>& drams){
	size_t llen = drams.size();
	size_t i = 0;
	vol_t from_size = 0;
	for(const pos_t& dram: drams){
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

pos_t NoC::nearest_dram(const UniqueLayout& layout){
	bool first = true;
	hop_t minHops = 0;
	pos_t d;
	for(const auto& dram : dram_list){
		_noc.clear();
		for(cidx_t i=0; i<layout.totLength(); ++i){
			auto it = layout[i];
			vol_t curSize = it.range.size();
			if(curSize <= 0) continue;
			_noc.unicast(it.tile, dram, curSize);
			_noc.tot_DRAM_acc += curSize;
		}
		if(first || _noc.tot_hops < minHops){
			first = false;
			minHops = _noc.tot_hops;
			d = dram;
		}
	}
	_noc.tot_hops = minHops;
	return d;
}

pos_t NoC::nearest_dram_to(const DataLayout& layout){
	bool first = true;
	hop_t minHops = 0;
	pos_t d;
	auto rLen = layout.rangeLength();
	for(const auto& dram : dram_list){
		_noc.clear();
		for(cidx_t i=0; i<rLen; ++i){
			auto it = layout.at(i);
			vol_t curSize = it.range.size();
			if(curSize <= 0) continue;
			if(it.numTile == 1){
				_noc.unicast(dram, it.tiles[0], curSize);
			}else{
				_noc.multicast(dram, it.tiles, it.numTile, curSize);
			}
			_noc.tot_DRAM_acc += curSize;
		}
		if(first || _noc.tot_hops < minHops){
			first = false;
			minHops = _noc.tot_hops;
			d = dram;
		}
	}
	_noc.tot_hops = minHops;
	return d;
}

std::ostream& operator<<(std::ostream& os, const NoC& noc){
	return os<<"NoC(hops="<<noc.tot_hops<<", DRAM acc="<<noc.tot_DRAM_acc<<")";
}

NoC::HopCount::HopCount():factor(1){}

NoC::HopCount& NoC::HopCount::operator+=(const HopCount& other){
	flat_factor();
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
	}else if(factor == 1){
		for(const auto& it: link_hops){
			assert(it.second % batch == 0);
			link_hops[it.first] = it.second / batch;
		}
	}else{
		for(const auto& it: link_hops){
			auto val = it.second * factor;
			assert(val % batch == 0);
			link_hops[it.first] = val / batch;
		}
		factor = 1;
	}
	return *this;
}

void NoC::HopCount::div(len_t batch){
	if(factor % batch == 0){
		factor /= batch;
	}else if(factor == 1){
		for(const auto& it: link_hops){
			// assert(it.second % batch == 0);
			link_hops[it.first] = it.second / batch;
		}
	}else{
		for(const auto& it: link_hops){
			auto val = it.second * factor;
			// assert(val % batch == 0);
			link_hops[it.first] = val / batch;
		}
		factor = 1;
	}
}

NoC::hop_t& NoC::HopCount::get(mlen_t x, mlen_t y, mlen_t dir){
	assert(factor == 1);
	size_t idx = (x * Cluster::ylen + y) * 4 + dir;
	assert(idx < 4u*Cluster::xlen*Cluster::ylen);
	return link_hops[idx];
}

void NoC::HopCount::clear(){
	factor = 1;
	link_hops.clear();
}

NoC::hop_t NoC::HopCount::max() const{
	hop_t h = 0;
	for(const auto& it: link_hops){
		h = MAX(h, it.second);
	}
	return h * factor;
}

void NoC::HopCount::flat_factor(){
	if(factor > 1){
		for(const auto& it: link_hops){
			link_hops[it.first] *= factor;
		}
		factor = 1;
	}
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


