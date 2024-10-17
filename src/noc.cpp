#include "noc.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cluster.h"
#include "datalayout.h"
#include "memlayout.h"
#include "util.h"


energy_t NoC::hop_cost;
energy_t NoC::DRAM_acc_cost;
bw_t NoC::DRAM_bw;
bw_t NoC::NoC_bw;
bool NoC::unicast_only = false;
bool NoC::full_interleave = true;
didx_t NoC::DRAM_num = 0;
didx_t NoC::il_group_num;
std::vector<NoC::PortInfo> NoC::port_list;
std::vector<didx_t> NoC::il_group_start;
thread_local NoC NoC::_noc(false);

NoC::NoC(bool _calc_bw): calc_bw(_calc_bw), tot_hops(0), DRAM_acc(DRAM_num){}

/*
void NoC::set_calc_bw(bool _calc_bw){
	calc_bw = _calc_bw;
	if(!calc_bw) link_hops.clear();
}
*/

void NoC::clear(){
	tot_hops = 0;
	DRAM_acc.resize(DRAM_num, 0);
	link_hops.clear();
}

void NoC::_fromRemoteMem(const GroupVec& groups, const DataLayout& to){
	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, groups);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, groups);
		}
	}
}

void NoC::_fromRemoteMem(const GroupVec& groups, const DataLayout& to, len_t fromC, len_t toC){
	fmap_range::dim_range truncRange = {fromC, toC};

	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		fmap_range range = it.range;
		range.c = range.c.intersect(truncRange);
		vol_t curSize = range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, groups);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, groups);
		}
	}
}

void NoC::_fromRemoteMem(didx_t group, const DataLayout& to){
	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, group);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, group);
		}
	}
}

void NoC::_fromRemoteMem(didx_t group, const DataLayout& to, len_t fromC, len_t toC){
	fmap_range::dim_range truncRange = {fromC, toC};

	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		fmap_range range = it.range;
		range.c = range.c.intersect(truncRange);
		vol_t curSize = range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, group);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, group);
		}
	}
}

void NoC::_toRemoteMem(const UniqueLayout& from, const GroupVec& groups){
	for(cidx_t i=0; i<from.totLength(); ++i){
		auto it = from[i];
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		unicast_to_dram(it.tile, curSize, groups);
	}
}

void NoC::_toRemoteMem(const UniqueLayout& from, didx_t group){
	for(cidx_t i=0; i<from.totLength(); ++i){
		auto it = from[i];
		vol_t curSize = it.range.size();
		if(curSize <= 0) continue;
		unicast_to_dram(it.tile, curSize, group);
	}
}

void NoC::fromRemoteMem(MemLayout& from, const DataLayout& to){
	if(full_interleave){
		from.set_layout({0});
	}else{
		// Do not interleave, find nearest DRAM.
		nearest_groups_to(to, from);
		if((!calc_bw) && il_group_num == 1){
			tot_hops += _noc.tot_hops;
			DRAM_acc += _noc.DRAM_acc;
			return;
		}
	}
	_fromRemoteMem(from.get_layouts(), to);
}

void NoC::fromRemoteMem_const(const MemLayout& from, const DataLayout& to){
	_fromRemoteMem(from.get_layouts(), to);
}

void NoC::fromRemoteMem(MemLayout& from, const DataLayout& to, len_t fromC, len_t toC){
	if(toC <= fromC) return;
	if(full_interleave){
		from.set_layout({0});
	}else{
		// Do not interleave, find nearest DRAM.
		nearest_groups_to(to, from);
		if((!calc_bw) && il_group_num == 1){
			tot_hops += _noc.tot_hops;
			DRAM_acc += _noc.DRAM_acc;
			return;
		}
	}
	_fromRemoteMem(from.get_layouts(), to, fromC, toC);
}

void NoC::fromRemoteMem_const(const MemLayout& from, const DataLayout& to, len_t fromC, len_t toC){
	if(toC <= fromC) return;
	_fromRemoteMem(from.get_layouts(), to, fromC, toC);
}

void NoC::fromRemoteMem_upd(const MemLayout& from_old,
							const MemLayout& from_cur,
							const DataLayout& to,
							len_t fromC, len_t toC)
{
	if(toC <= fromC) return;
	const auto& drams_old = from_old.get_layouts();
	const auto& drams_cur = from_cur.get_layouts();
	fmap_range::dim_range truncRange = {fromC, toC};

	auto rLen = to.rangeLength();
	for(cidx_t i=0; i<rLen; ++i){
		auto it = to.at(i);
		fmap_range range = it.range;
		range.c = range.c.intersect(truncRange);
		vol_t curSize = range.size();
		if(curSize <= 0) continue;
		if(it.numTile == 1){
			unicast_dram(it.tiles[0], curSize, drams_old, false);
			unicast_dram(it.tiles[0], curSize, drams_cur, true);
		}else{
			multicast_dram(it.tiles, it.numTile, curSize, drams_old, false);
			multicast_dram(it.tiles, it.numTile, curSize, drams_cur, true);
		}
	}
}

void NoC::toRemoteMem(const UniqueLayout& from, MemLayout& to){
	if(full_interleave){
		to.set_layout({0});
	}else{
		nearest_groups(from, to);
		if((!calc_bw) && il_group_num == 1){
			tot_hops += _noc.tot_hops;
			DRAM_acc += _noc.DRAM_acc;
			return;
		}
	}
	_toRemoteMem(from, to.get_layouts());
}

void NoC::toRemoteMem_const(const UniqueLayout& from, const MemLayout& to){
	_toRemoteMem(from, to.get_layouts());
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
	DRAM_acc += other.DRAM_acc;
	if(calc_bw || other.calc_bw){
		assert(calc_bw && other.calc_bw);
		link_hops += other.link_hops;
	}
	return *this;
}

NoC& NoC::operator-=(const NoC& other){
	assert(tot_hops >= other.tot_hops);
	tot_hops -= other.tot_hops;
	assert((DRAM_acc >= other.DRAM_acc).min());
	DRAM_acc -= other.DRAM_acc;
	if(calc_bw || other.calc_bw){
		assert(calc_bw && other.calc_bw);
		link_hops -= other.link_hops;
	}
	return *this;
}

NoC NoC::operator*(const len_t& batch) const{
	NoC x = *this;
	return x *= batch;
}

NoC& NoC::operator*=(const len_t& batch){
	tot_hops *= batch;
	DRAM_acc *= batch;
	if(calc_bw) link_hops *= batch;
	return *this;
}

NoC& NoC::operator/=(const len_t& batch){
	tot_hops /= batch;
	DRAM_acc /= batch;
	if(calc_bw) link_hops /= batch;
	return *this;
}

void NoC::div(len_t batch){
	tot_hops /= batch;
	DRAM_acc /= batch;
	if(calc_bw) link_hops.div(batch);
}

void NoC::set_DRAMs(const std::vector<std::vector<pos_t>>& port_lists){
	port_list.clear();

	if(port_lists.empty()){
		throw std::invalid_argument("No ports input in set_DRAMs()!");
	}
	didx_t DRAM_idx = 0;
	for(const auto& cur_list : port_lists){
		if(cur_list.empty()){
			std::string msg = "DRAM ";
			msg += std::to_string(DRAM_idx);
			msg += " has no ports!";
			throw std::invalid_argument(msg);
		}
		didx_t port_idx = 0;
		for(const auto& pos : cur_list){
			port_list.push_back({pos, DRAM_idx, port_idx});
			++port_idx;
		}
		++DRAM_idx;
	}

	std::unordered_set<pos_t, pos_hash> s;
	for(const auto& port_info : port_list){
		const auto& port_pos = port_info.pos;
		if(!s.insert(port_pos).second){
			std::string msg = "Duplicate port ";
			msg += port_pos.to_string();
			msg += " in port_lists!";
			throw std::invalid_argument(msg);
		}
	}

	full_interleave = true;
	DRAM_num = port_lists.size();
}

void NoC::set_interleave(const std::vector<std::vector<std::pair<didx_t, didx_t>>>& port_groups, didx_t ngroups){
	if(ngroups > port_groups.size()){
		std::string msg = "Not enough groups to choose! Need ";
		msg += std::to_string(ngroups);
		msg += " but has only ";
		msg += std::to_string(port_groups.size());
		throw std::invalid_argument(msg);
	}
	if(ngroups == 0){
		throw std::invalid_argument("At least one group should be chosen.");
	}

	if(ngroups == port_groups.size()){
		std::cout << "Warning: when all groups are used, it's the same as full interleaving." << std::endl;
		std::cout << "Falling back to full inverleaving." << std::endl;
		full_interleave = true;
		return;
	}

	// Since no built-in hash available, for simplicity just use map here.
	std::map<std::pair<didx_t, didx_t>, didx_t> port_idxs;
	for(didx_t port_idx = 0; port_idx < port_list.size(); ++port_idx){
		const auto& port = port_list[port_idx];
		auto it = port_idxs.insert({{port.DRAM_idx, port.port_idx}, port_idx});
		assert(it.second);
	}

	didx_t port_num = port_idxs.size();
	std::vector<didx_t> idx_map(port_num, port_num);

	il_group_start.clear();
	il_group_start.reserve(port_groups.size()+1);
	il_group_start.push_back(0);
	didx_t cur_idx = 0;
	for(const auto& port_group : port_groups){
		for(const auto& port : port_group){
			didx_t port_idx;
			try {
				port_idx = port_idxs.at(port);
			} catch (std::out_of_range&) {
				std::string msg = "Port (DRAM ";
				msg += std::to_string(port.first);
				msg += ", port ";
				msg += std::to_string(port.second);
				msg += ") not found!";
				throw std::invalid_argument(msg);
			}

			auto& val = idx_map[port_idx];
			if(val != port_num){
				std::string msg = "Duplicate port (DRAM ";
				msg += std::to_string(port.first);
				msg += ", port ";
				msg += std::to_string(port.second);
				msg += ") in port_groups!";
				throw std::invalid_argument(msg);
			}
			val = cur_idx++;
		}
		il_group_start.push_back(il_group_start.back() + port_group.size());
	}

	if(il_group_start.back() < port_num){
		throw std::invalid_argument("All ports should belong to a port_group!");
	}

	auto _port_list = port_list;
	for(didx_t port_idx = 0; port_idx < port_num; ++port_idx){
		port_list[idx_map[port_idx]] = _port_list[port_idx];
	}

	full_interleave = false;
	il_group_num = ngroups;
}

NoC::hop_t NoC::get_tot_hops() const{
	return tot_hops;
}

energy_t NoC::get_hop_cost() const{
	return tot_hops*hop_cost;
}

energy_t NoC::get_cost() const{
	//std::cout << "GC " << tot_hops << ' ' << tot_DRAM_acc << std::endl;
	return tot_hops*hop_cost + get_tot_DRAM_acc()*DRAM_acc_cost;
}

cycle_t NoC::get_time() const{
	cycle_t dram_time = get_dram_time();
	if(!calc_bw) return dram_time;
	cycle_t noc_time = DIVCEIL(link_hops.max(), NoC_bw);
	return MAX(dram_time, noc_time);
}

cycle_t NoC::get_dram_time() const{
	auto max_DRAM_acc = DRAM_acc.max();
	return DIVCEIL(max_DRAM_acc, DRAM_bw);
}

didx_t NoC::get_dram_num(){
    return DRAM_num;
}

didx_t NoC::get_il_group_start_size(){
    return il_group_start.size();
}

bool NoC::get_full_interleave(){
    return full_interleave;
}

void NoC::unicast(pos_t src, pos_t dst, vol_t size, bool is_add){
	if(is_add){
		tot_hops += unicastCalc(src, dst, size);
	}else{
		auto hops = unicastCalc_sub(src, dst, size);
		assert(tot_hops >= hops);
		tot_hops -= hops;
	}
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

NoC::hop_t NoC::unicastCalc_sub(pos_t src, pos_t dst, vol_t size){
	link_hops.flat_factor();
	if(calc_bw){
		size_t x_dir = (dst.x > src.x)?0:2;
		size_t y_dir = (dst.y > src.y)?3:1;
		mlen_t dx = (dst.x > src.x)?1:-1;
		mlen_t dy = (dst.y > src.y)?1:-1;
		for(mlen_t x = src.x; x != dst.x; x+= dx){
			auto& val = link_hops.get(x, src.y, x_dir);
			assert(val >= size);
			val -= size;
		}
		for(mlen_t y = src.y; y != dst.y; y+= dy){
			auto& val = link_hops.get(dst.x, y, y_dir);
			assert(val >= size);
			val -= size;
		}
	}
	return static_cast<hop_t>(abs(src.x-dst.x)+abs(src.y-dst.y)) * size;
}

void NoC::multicast(pos_t src, const pos_t* dst, cidx_t len, vol_t size, bool is_add){
	if(is_add){
		tot_hops += multicastCalc(src, dst, len, size);
	}else{
		auto hops = multicastCalc(src, dst, len, size);
		assert(tot_hops >= hops);
		tot_hops -= hops;
	}
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

NoC::hop_t NoC::multicastCalc_sub(pos_t src, const pos_t* dst, cidx_t len, vol_t size){
	if(unicast_only){
		hop_t h = 0;
		for(cidx_t i=0; i<len; ++i){
			h += unicastCalc_sub(src, dst[i], size);
		}
		return h;
	}
	link_hops.flat_factor();

	mlen_t cur_x = dst[0].x;
	mlen_t min_y = dst[0].y;
	hop_t h = 0;
	if(calc_bw){
		for(mlen_t x = src.x; x > dst[0].x; --x){
			auto& val = link_hops.get(x, src.y, 2);
			assert(val >= size);
			val -= size;
		}
		for(mlen_t x = src.x; x < dst[len-1].x; ++x){
			auto& val = link_hops.get(x, src.y, 0);
			assert(val >= size);
			val -= size;
		}
	}
	h += MAX(src.x, dst[len-1].x) - MIN(src.x, dst[0].x);

	for(cidx_t i=1; i<=len; ++i){
		if(i<len && dst[i].x == cur_x) continue;
		if(calc_bw){
			for(mlen_t y = src.y; y > min_y; --y){
				auto& val = link_hops.get(cur_x, y, 1);
				assert(val >= size);
				val -= size;

			}
			for(mlen_t y = src.y; y < dst[i-1].y; ++y){
				auto& val = link_hops.get(cur_x, y, 3);
				assert(val >= size);
				val -= size;
			}
		}
		h += MAX(src.y, dst[i-1].y) - MIN(src.y, min_y);
		if(i == len) break;
		cur_x = dst[i].x;
		min_y = dst[i].y;
	}
	return h * size;
}

void NoC::unicast_dram(pos_t dst, vol_t size, const GroupVec& groups, bool is_add){
	if(full_interleave){
		return unicast_dram(dst, size, 0, is_add);
	}
	if(groups.size() == 1){
		return unicast_dram(dst, size, groups[0], is_add);
	}
	size_t tot_len = 0;
	for(const auto& group_idx : groups){
		tot_len += il_group_start[group_idx+1] - il_group_start[group_idx];
	}
	size_t i = 0;
	vol_t from_size = 0;
	for(const auto& group_idx : groups){
		for(didx_t port_idx = il_group_start[group_idx];
			port_idx < il_group_start[group_idx+1];
			++port_idx
		){
			vol_t to_size = (size * ++i) / tot_len;
			vol_t cur_size = to_size - from_size;
			const auto& dram = port_list[port_idx];
			unicast(dram.pos, dst, cur_size, is_add);
			auto& val = DRAM_acc[dram.DRAM_idx];
			if(is_add){
				val += cur_size;
			}else{
				assert(val >= cur_size);
				val -= cur_size;
			}
			from_size = to_size;
		}
	}
}

void NoC::unicast_to_dram(pos_t dst, vol_t size, const GroupVec& groups, bool is_add){
	if(full_interleave){
		return unicast_to_dram(dst, size, 0, is_add);
	}
	if(groups.size() == 1){
		return unicast_to_dram(dst, size, groups[0], is_add);
	}
	size_t tot_len = 0;
	for(const auto& group_idx : groups){
		tot_len += il_group_start[group_idx+1] - il_group_start[group_idx];
	}
	size_t i = 0;
	vol_t from_size = 0;
	for(const auto& group_idx : groups){
		for(didx_t port_idx = il_group_start[group_idx];
			port_idx < il_group_start[group_idx+1];
			++port_idx
		){
			vol_t to_size = (size * ++i) / tot_len;
			vol_t cur_size = to_size - from_size;
			const auto& dram = port_list[port_idx];
			unicast(dst, dram.pos, cur_size, is_add);
			auto& val = DRAM_acc[dram.DRAM_idx];
			if(is_add){
				val += cur_size;
			}else{
				assert(val >= cur_size);
				val -= cur_size;
			}
			from_size = to_size;
		}
	}
}

void NoC::multicast_dram(const pos_t* dst, cidx_t len, vol_t size, const GroupVec& groups, bool is_add){
	if(full_interleave){
		return multicast_dram(dst, len, size, 0, is_add);
	}
	if(groups.size() == 1){
		return multicast_dram(dst, len, size, groups[0], is_add);
	}
	size_t tot_len = 0;
	for(const auto& group_idx : groups){
		tot_len += il_group_start[group_idx+1] - il_group_start[group_idx];
	}
	size_t i = 0;
	vol_t from_size = 0;
	for(const auto& group_idx : groups){
		for(didx_t port_idx = il_group_start[group_idx];
			port_idx < il_group_start[group_idx+1];
			++port_idx
		){
			vol_t to_size = (size * ++i) / tot_len;
			vol_t cur_size = to_size - from_size;
			const auto& dram = port_list[port_idx];
			multicast(dram.pos, dst, len, cur_size, is_add);
			auto& val = DRAM_acc[dram.DRAM_idx];
			if(is_add){
				val += cur_size;
			}else{
				assert(val >= cur_size);
				val -= cur_size;
			}
			from_size = to_size;
		}
	}
}

void NoC::unicast_dram(pos_t dst, vol_t size, didx_t group, bool is_add){
	didx_t start, end;
	if(full_interleave){
		start = 0;
		end = port_list.size();
	}else{
		start = il_group_start[group];
		end = il_group_start[group+1];
	}
	size_t tot_len = end - start;
	size_t i = 0;
	vol_t from_size = 0;
	for(didx_t port_idx = start; port_idx < end; ++port_idx){
		vol_t to_size = (size * ++i) / tot_len;
		vol_t cur_size = to_size - from_size;
		const auto& dram = port_list[port_idx];
		unicast(dram.pos, dst, cur_size, is_add);
		auto& val = DRAM_acc[dram.DRAM_idx];
		if(is_add){
			val += cur_size;
		}else{
			assert(val >= cur_size);
			val -= cur_size;
		}
		from_size = to_size;
	}
}

void NoC::unicast_to_dram(pos_t dst, vol_t size, didx_t group, bool is_add){
	didx_t start, end;
	if(full_interleave){
		start = 0;
		end = port_list.size();
	}else{
		start = il_group_start[group];
		end = il_group_start[group+1];
	}
	size_t tot_len = end - start;
	size_t i = 0;
	vol_t from_size = 0;
	for(didx_t port_idx = start; port_idx < end; ++port_idx){
		vol_t to_size = (size * ++i) / tot_len;
		vol_t cur_size = to_size - from_size;
		const auto& dram = port_list[port_idx];
		unicast(dst, dram.pos, cur_size, is_add);
		auto& val = DRAM_acc[dram.DRAM_idx];
		if(is_add){
			val += cur_size;
		}else{
			assert(val >= cur_size);
			val -= cur_size;
		}
		from_size = to_size;
	}
}

void NoC::multicast_dram(const pos_t* dst, cidx_t len, vol_t size, didx_t group, bool is_add){
	didx_t start, end;
	if(full_interleave){
		start = 0;
		end = port_list.size();
	}else{
		start = il_group_start[group];
		end = il_group_start[group+1];
	}
	size_t tot_len = end - start;
	size_t i = 0;
	vol_t from_size = 0;
	for(didx_t port_idx = start; port_idx < end; ++port_idx){
		vol_t to_size = (size * ++i) / tot_len;
		vol_t cur_size = to_size - from_size;
		const auto& dram = port_list[port_idx];
		multicast(dram.pos, dst, len, cur_size, is_add);
		auto& val = DRAM_acc[dram.DRAM_idx];
		if(is_add){
			val += cur_size;
		}else{
			assert(val >= cur_size);
			val -= cur_size;
		}
		from_size = to_size;
	}
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
	return DRAM_acc.sum();
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

void NoC::nearest_groups(const UniqueLayout& layout, MemLayout& mem){
	didx_t ngroup = il_group_start.size()-1;

	if(il_group_num == 1){
		_noc.clear();

		bool first = true;
		hop_t min_hops = 0;
		auto DRAM_acc = _noc.DRAM_acc;
		didx_t cur_group;

		for(didx_t group_idx = 0; group_idx < ngroup; ++group_idx){
			_noc._toRemoteMem(layout, group_idx);
			if(first || _noc.tot_hops < min_hops){
				first = false;
				min_hops = _noc.tot_hops;
				DRAM_acc = _noc.DRAM_acc;
				cur_group = group_idx;
			}
			_noc.clear();
		}
		_noc.tot_hops = min_hops;
		_noc.DRAM_acc = std::move(DRAM_acc);
		mem.set_layout({cur_group});
		return;
	}

	typedef std::pair<hop_t, didx_t> cost_pair;
	std::vector<cost_pair> costs;

	for(didx_t group_idx = 0; group_idx < ngroup; ++group_idx){
		_noc.clear();
		_noc._toRemoteMem(layout, group_idx);
		costs.push_back({_noc.tot_hops, group_idx});
	}

	constexpr auto comp = [](const cost_pair& a, const cost_pair& b){
		return a > b;
	};
	std::make_heap(costs.begin(), costs.end(), comp);

	std::vector<didx_t> group_vec;
	group_vec.reserve(il_group_num);
	for(didx_t i=0; i<il_group_num; ++i){
		std::pop_heap(costs.begin(), costs.end(), comp);
		group_vec.push_back(costs.back().second);
		costs.pop_back();
	}

	std::sort(group_vec.begin(), group_vec.end());
	mem.set_layout(std::move(group_vec));
}

void NoC::nearest_groups_to(const DataLayout& layout, MemLayout& mem){
	didx_t ngroup = il_group_start.size()-1;

	if(il_group_num == 1){
		_noc.clear();

		bool first = true;
		hop_t min_hops = 0;
		auto DRAM_acc = _noc.DRAM_acc;
		didx_t cur_group;

		for(didx_t group_idx = 0; group_idx < ngroup; ++group_idx){
			_noc._fromRemoteMem(group_idx, layout);
			if(first || _noc.tot_hops < min_hops){
				first = false;
				min_hops = _noc.tot_hops;
				DRAM_acc = _noc.DRAM_acc;
				cur_group = group_idx;
			}
			_noc.clear();
		}
		_noc.tot_hops = min_hops;
		_noc.DRAM_acc = std::move(DRAM_acc);
		mem.set_layout({cur_group});
		return;
	}

	typedef std::pair<hop_t, didx_t> cost_pair;
	std::vector<cost_pair> costs;

	for(didx_t group_idx = 0; group_idx < ngroup; ++group_idx){
		_noc.clear();
		_noc._fromRemoteMem(group_idx, layout);
		costs.push_back({_noc.tot_hops, group_idx});
	}

	constexpr auto comp = [](const cost_pair& a, const cost_pair& b){
		return a > b;
	};
	std::make_heap(costs.begin(), costs.end(), comp);

	std::vector<didx_t> group_vec;
	group_vec.reserve(il_group_num);
	for(didx_t i=0; i<il_group_num; ++i){
		std::pop_heap(costs.begin(), costs.end(), comp);
		group_vec.push_back(costs.back().second);
		costs.pop_back();
	}

	std::sort(group_vec.begin(), group_vec.end());
	mem.set_layout(std::move(group_vec));
}

std::ostream& operator<<(std::ostream& os, const NoC& noc){
	os << "NoC(hops=" << noc.tot_hops;
	os << ", tot DRAM acc=" <<noc.get_tot_DRAM_acc();
	os << "[";
	for(access_t acc : noc.DRAM_acc){
		os << acc << ',';
	}
	os << "])";
	return os;
}

NoC::HopCount::HopCount():factor(1){}

NoC::HopCount& NoC::HopCount::operator+=(const HopCount& other){
	flat_factor();
	for(const auto& it : other.link_hops){
		link_hops[it.first] += it.second * other.factor;
	}
	return *this;
}

NoC::HopCount& NoC::HopCount::operator-=(const HopCount& other){
	flat_factor();
	for(const auto& it : other.link_hops){
		auto& val = link_hops[it.first];
		auto val2 = it.second * other.factor;
		assert(val >= val2);
		val -= val2;
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


