#ifndef NOC_H
#define NOC_H

#include <vector>
#include <unordered_map>

#include "cluster.h"
#include "util.h"

class DataLayout;
class UniqueLayout;
struct PlaceSch;
class LNode;
class BufferUsage;
//#include "datalayout.h"
//#include "placement.h"
//#include "schnode.h"
//#include "bufferusage.h"

class NoC{
public:
	typedef Cluster::hop_t hop_t;
	static energy_t hop_cost, DRAM_acc_cost;
	static bw_t DRAM_bw, NoC_bw;
	static std::vector<pos_t> dram_list;
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
		hop_t& get(mlen_t x, mlen_t y, mlen_t dir);
		void clear();
		hop_t max() const;

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

	vol_t calc_intersect(const fmap_range& rng1, const fmap_range& rng2, len_t bat1, len_t bat2);
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

	//void set_calc_bw(bool _calc_bw);
	void reset();

	void fromRemoteMem(const DataLayout& toLayout);
	void fromRemoteMem(const DataLayout& toLayout, len_t fromC, len_t toC);
	void toRemoteMem(const UniqueLayout& fromLayout);
	void betweenLayout(const UniqueLayout& fromLayout, const DataLayout& toLayout, len_t fromCOffset, len_t fromB, len_t toB);

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
	void unicast_dram(pos_t dst, vol_t size);
	void unicast_to_dram(pos_t dst, vol_t size);
	void multicast_dram(const pos_t* dst, cidx_t len, vol_t size);


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
