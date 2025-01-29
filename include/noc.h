/* This file contains
 *	NoC: Records all NoC-related information of a SchNode.
 */

#ifndef NOC_H
#define NOC_H

#include <iostream>
#include <vector>
#include <unordered_map>

#include "util.h"

class DataLayout;
class UniqueLayout;
//#include "datalayout.h"


class NoC{
public:
	typedef vol_t hop_t;

	/*
	 * Global variables
	 *
	 * hop_cost:      energy cost of one hop.
	 * DRAM_acc_cost: energy cost of one DRAM access (one data).
	 * DRAM_bw:       DRAM bandwidth (#data / cycle)
	 * NoC_bw:        NoC bandwidth (#data / cycle)
	 * dram_list:     List of all DRAM ports
	 */
	static energy_t hop_cost, DRAM_acc_cost;
	static bw_t DRAM_bw, NoC_bw;
	static std::vector<pos_t> dram_list;

private:
	// Records hop count (#hops) of all links
	class HopCount{
		friend NoC;

		typedef std::int32_t linkIdx_t;

		// link_hops[link_id] = hops_on_link_id
		std::unordered_map<linkIdx_t, hop_t> link_hops;
		/*
		 * factor: for faster mult (lazy update).
		 *
		 * When the size of all hops are multiplied by n,
		 * instead of multiplying all items in the dict,
		 * we only need to multiply *factor* by n.
		 * *factor* will be expanded in later operations.
		 */
		hop_t factor;

	public:
		HopCount();

		HopCount& operator+=(const HopCount& other);
		HopCount& operator*=(const len_t& batch);
		HopCount& operator/=(const len_t& batch);
		// Similar to operator/=(b), but allows rounding.
		void div(len_t batch);

		// Maximal #hops of one link.
		hop_t max() const;

		// Gets #hops on the link from (x, y) to direction dir.
		hop_t& get(mlen_t x, mlen_t y, mlen_t dir);

		// Conversion between (x, y, dir) and link_idx
		static linkIdx_t get_idx(mlen_t x, mlen_t y, mlen_t dir);
		static void get_dir(linkIdx_t link_idx, mlen_t& x, mlen_t& y, mlen_t& dir);

		// Clear all #hops.
		void clear();

		// Expand *factor*
		void flat_factor();
	};

	// Whether calculates hops on each link.
	// When set to false, only calculate total hops.
	bool calc_bw;

	// Total count of hops and DRAM access
	hop_t tot_hops;
	access_t tot_DRAM_acc;

	// Hops on each link (only used when calc_bw = true)
	// Direction: ESWN = 0123
	HopCount link_hops;

	/*
	 * Calculate the volume of intersection between "rng1" and "rng2"
	 * The batch dimension of "rng1/2" is in [0, bat1/2)
	 * Returned value corresponds to bat2 batches.
	 */
	static vol_t calc_intersect(const fmap_range& rng1, const fmap_range& rng2, len_t bat1, len_t bat2);

	// Functions for unicast/multicast calc
	// Notice: for multiple dests, *dst* needs to be in increasing order.
	void unicast(pos_t src, pos_t dst, vol_t size);
	hop_t unicastCalc(pos_t src, pos_t dst, vol_t size);
	void multicast(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	hop_t multicastCalc(pos_t src, const pos_t* dst, cidx_t len, vol_t size);
	void unicast_from_dram(pos_t dst, vol_t size);
	void unicast_to_dram(pos_t src, vol_t size);
	void multicast_from_dram(const pos_t* dst, cidx_t len, vol_t size);

public:
	NoC(bool _calc_bw = true);

	NoC(const NoC& other) = default;
	NoC(NoC&& other) = default;
	NoC& operator=(const NoC& other) = default;
	NoC& operator=(NoC&& other) = default;

	~NoC() = default;

	NoC operator+(const NoC& other) const;
	NoC& operator+=(const NoC& other);
	NoC operator*(const len_t& batch) const;
	NoC& operator*=(const len_t& batch);
	NoC& operator/=(const len_t& batch);
	// Similar to operator/=(b), but allows rounding.
	void div(len_t batch);

	// Clear all noc data.
	void clear();

	// DRAM -> toLayout
	void fromRemoteMem(const DataLayout& toLayout);
	// DRAM -> toLayout, only channels in [fromC, toC) is fetched
	void fromRemoteMem(const DataLayout& toLayout, len_t fromC, len_t toC);
	// fromLayout -> DRAM
	void toRemoteMem(const UniqueLayout& fromLayout);
	/*
	 * fromLayout -> toLayout
	 *
	 * fromCOffset: channels in toLayout is padded by fromCOffset (fromCOffset+i is the ith channel)
	 * fromB:       batch size of fromLayout (used in calc_intersect)
	 * toB:         batch size of toLayout (used in calc_intersect)
	 */
	void betweenLayout(const UniqueLayout& fromLayout, const DataLayout& toLayout, len_t fromCOffset, len_t fromB, len_t toB);

	// Getter functions.
	cycle_t get_time() const;
	energy_t get_cost() const;
	energy_t get_hop_cost() const;
	energy_t get_DRAM_cost() const;
	hop_t get_tot_hops() const;
	access_t get_tot_DRAM_acc() const;
	// Maximal #hops of one link. Used for bandwidth calculation.
	hop_t get_max_link() const;

	// Prints noc information
	friend std::ostream& operator<<(std::ostream& os, const NoC& noc);

	// Pretty link hops
	struct link_info{
		pos_t from, to;
		hop_t total_hops;

		bool operator<(const link_info& other) const;
		bool operator==(const link_info& other) const;
		bool operator>(const link_info& other) const;

		friend std::ostream& operator<<(std::ostream& os, const link_info& info);
	};
	// Gets all link hops
	std::vector<link_info> get_link_info() const;
};

#endif // NOC_H
