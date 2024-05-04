#ifndef NOC_H
#define NOC_H

#include <iostream>
#include <vector>
#include <unordered_map>

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
	static std::vector<pos_t> dram_list;
	static bool unicast_only, DRAM_interleave;
	static thread_local NoC _noc;
private:
	class HopCount{
		// TODO: change size_t to appropriate size
		std::unordered_map<size_t, hop_t> link_hops;
		hop_t factor;
	public:
		HopCount();
		HopCount& operator+=(const HopCount& other);
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
	access_t tot_DRAM_acc;
	// Direction: ESWN = 0123
	HopCount link_hops;

	static vol_t calc_intersect(const fmap_range& rng1, const fmap_range& rng2, len_t bat1, len_t bat2);
	static pos_t nearest_dram(const UniqueLayout& layout);
	static pos_t nearest_dram_to(const DataLayout& layout);

	void _fromRemoteMem(const std::vector<pos_t>& from, const DataLayout& to);
	void _fromRemoteMem(const std::vector<pos_t>& from, const DataLayout& to, len_t fromC, len_t toC);
public:
	NoC(bool _calc_bw = true);
	NoC(const NoC& other) = default;
	NoC(NoC&& other) = default;
	NoC& operator=(const NoC& other) = default;
	NoC& operator=(NoC&& other) = default;

	NoC operator+(const NoC& other) const;
	NoC& operator+=(const NoC& other);
	NoC operator*(const len_t& batch) const;
	NoC& operator*=(const len_t& batch);
	NoC& operator/=(const len_t& batch);
	~NoC() = default;

	void div(len_t batch);

	//void set_calc_bw(bool _calc_bw);
	void clear();

	void fromRemoteMem(const DataLayout& to);
	void fromRemoteMem(const MemLayout& from, const DataLayout& to);
	void fromRemoteMem(const DataLayout& to, len_t fromC, len_t toC);
	void fromRemoteMem(const MemLayout& from, const DataLayout& to, len_t fromC, len_t toC);
	void toRemoteMem(const UniqueLayout& from, MemLayout& to);
	void toRemoteMem_const(const UniqueLayout& from, const MemLayout& to);
	void betweenLayout(const UniqueLayout& from, const DataLayout& to, len_t fromCOffset, len_t fromB, len_t toB);

	hop_t get_tot_hops() const;
	access_t get_tot_DRAM_acc() const;
	energy_t get_hop_cost() const;
	energy_t get_cost() const;
	cycle_t get_time() const;

	void unicast(pos_t src, pos_t dst, vol_t size);
	hop_t unicastCalc(pos_t src, pos_t dst, vol_t size);
	// TODO: dst needs to be in inc. order.
	void multicast(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	hop_t multicastCalc(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	// DRAM is at (-1,x) and (n,x)
	void unicast_dram(pos_t dst, vol_t size, const std::vector<pos_t>& drams);
	void unicast_to_dram(pos_t dst, vol_t size, const std::vector<pos_t>& drams);
	void multicast_dram(const pos_t* dst, cidx_t len, vol_t size, const std::vector<pos_t>& drams);


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
