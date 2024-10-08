#ifndef NOC_H
#define NOC_H

#include <iostream>
#include <valarray>
#include <vector>
#include <unordered_map>
#include <utility>

#include "util.h"

class DataLayout;
class UniqueLayout;
class MemLayout;
//#include "datalayout.h"
//#include "memlayout.h"

class NoC{
public:
	typedef vol_t hop_t;
	static energy_t hop_cost, DRAM_acc_cost;
	static bw_t DRAM_bw, NoC_bw;
	static bool unicast_only;
private:
	struct PortInfo{
		pos_t pos;
		didx_t DRAM_idx;
		didx_t port_idx;
	};

	static bool full_interleave;
	static didx_t DRAM_num, il_group_num;
	static std::vector<PortInfo> port_list;
	static std::vector<didx_t> il_group_start;
	static thread_local NoC _noc;

	typedef std::vector<didx_t> GroupVec;

	class HopCount{
		// TODO: change size_t to appropriate size
		std::unordered_map<size_t, hop_t> link_hops;
		hop_t factor;
	public:
		HopCount();
		HopCount& operator+=(const HopCount& other);
		HopCount& operator-=(const HopCount& other);
		HopCount& operator*=(const len_t& batch);
		HopCount& operator/=(const len_t& batch);
		void div(len_t batch);
		hop_t& get(mlen_t x, mlen_t y, mlen_t dir);
		void clear();
		hop_t max() const;

		void flat_factor();

		friend class NoC;
	};

	// TODO: handle calc_bw=true;
	bool calc_bw;
	// TODO: get rid of all static_cast
	// (may need to change PartSch to cidx_t)

	hop_t tot_hops;
	std::valarray<access_t> DRAM_acc;
	// Direction: ESWN = 0123
	HopCount link_hops;

	static vol_t calc_intersect(const fmap_range& rng1, const fmap_range& rng2, len_t bat1, len_t bat2);
	static void nearest_groups(const UniqueLayout& layout, MemLayout& mem);
	static void nearest_groups_to(const DataLayout& layout, MemLayout& mem);

	void _fromRemoteMem(const GroupVec& groups, const DataLayout& to);
	void _fromRemoteMem(const GroupVec& groups, const DataLayout& to, len_t fromC, len_t toC);
	void _fromRemoteMem(didx_t group, const DataLayout& to);
	void _fromRemoteMem(didx_t group, const DataLayout& to, len_t fromC, len_t toC);
	void _toRemoteMem(const UniqueLayout& from, const GroupVec& groups);
	void _toRemoteMem(const UniqueLayout& from, didx_t group);

public:
	NoC(bool _calc_bw = true);
	NoC(const NoC& other) = default;
	NoC(NoC&& other) = default;
	NoC& operator=(const NoC& other) = default;
	NoC& operator=(NoC&& other) = default;

	NoC operator+(const NoC& other) const;
	NoC& operator+=(const NoC& other);
	NoC& operator-=(const NoC& other);
	NoC operator*(const len_t& batch) const;
	NoC& operator*=(const len_t& batch);
	NoC& operator/=(const len_t& batch);
	~NoC() = default;

	void div(len_t batch);

	static void set_DRAMs(const std::vector<std::vector<pos_t>>& port_lists);
	static void set_interleave(const std::vector<std::vector<std::pair<didx_t, didx_t>>>& port_groups, didx_t ngroups = 1);

	//void set_calc_bw(bool _calc_bw);
	void clear();

	void fromRemoteMem(MemLayout& from, const DataLayout& to);
	void fromRemoteMem_const(const MemLayout& from, const DataLayout& to);
	void fromRemoteMem(MemLayout& from, const DataLayout& to, len_t fromC, len_t toC);
	void fromRemoteMem_const(const MemLayout& from, const DataLayout& to, len_t fromC, len_t toC);
	void fromRemoteMem_upd(const MemLayout& from_old, const MemLayout& from_cur, const DataLayout& to, len_t fromC, len_t toC);
	void toRemoteMem(const UniqueLayout& from, MemLayout& to);
	void toRemoteMem_const(const UniqueLayout& from, const MemLayout& to);
	void betweenLayout(const UniqueLayout& from, const DataLayout& to, len_t fromCOffset, len_t fromB, len_t toB);

	hop_t get_tot_hops() const;
	access_t get_tot_DRAM_acc() const;
	energy_t get_hop_cost() const;
	energy_t get_cost() const;
	cycle_t get_time() const;
	cycle_t get_dram_time() const;
	static didx_t get_dram_num();
	static didx_t get_il_group_num();
	

	void unicast(pos_t src, pos_t dst, vol_t size, bool is_add = true);
	hop_t unicastCalc(pos_t src, pos_t dst, vol_t size);
	hop_t unicastCalc_sub(pos_t src, pos_t dst, vol_t size);
	// TODO: dst needs to be in inc. order.
	void multicast(pos_t src, const pos_t* dst, cidx_t len, vol_t size, bool is_add = true);
	hop_t multicastCalc(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	hop_t multicastCalc_sub(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	// DRAM is at (-1,x) and (n,x)
	void unicast_dram(pos_t dst, vol_t size, const GroupVec& groups, bool is_add = true);
	void unicast_to_dram(pos_t dst, vol_t size, const GroupVec& groups, bool is_add = true);
	void multicast_dram(const pos_t* dst, cidx_t len, vol_t size, const GroupVec& groups, bool is_add = true);

	void unicast_dram(pos_t dst, vol_t size, didx_t group, bool is_add = true);
	void unicast_to_dram(pos_t dst, vol_t size, didx_t group, bool is_add = true);
	void multicast_dram(const pos_t* dst, cidx_t len, vol_t size, didx_t group, bool is_add = true);


	friend std::ostream& operator<<(std::ostream& os, const NoC& noc);

	struct link_info{
		pos_t from, to;
		hop_t total_hops;

		bool operator<(const link_info& other) const;
		bool operator==(const link_info& other) const;
		bool operator>(const link_info& other) const;

		friend std::ostream& operator<<(std::ostream& os, const link_info& info);
	};
	std::vector<link_info> get_link_info() const;
	hop_t get_max_link() const;
};

#endif // NOC_H
