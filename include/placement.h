/* This file contains
 *	PlaceSch:    Records a placement scheme.
 *  PlaceEngine: Generates valid placement schemes.
 *  PlaceIter:   Used to iterate through all valid placement schemes.
 *
 * The whole structure is similar to partition.h
 */

#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <cstdint>
#include <iostream>
#include <memory>

#include "datalayout.h"
#include "partition.h"
#include "util.h"

class Cluster;
//#include "cluster.h"


struct PlaceSch{
	// The underlying partition scheme
	PartSch part;

	/*
	 * Order of dimensions, 0123=KBHW
	 * An order of 1,2,3,0 means B,H,W,K.
	 */
	std::uint8_t order[4];

	// Data layouts of ifmap, weight and ofmap
	std::unique_ptr<DataLayout> ifmLayout, wgtLayout;
	std::unique_ptr<UniqueLayout> ofmLayout/*, memLayout*/;
	// Buffer for ifmLayout & wgtLayout & ofmLayout
	std::unique_ptr<pos_t[]> permuteOrder;

	PlaceSch() = default;
	PlaceSch(const PlaceSch& sch);
	PlaceSch(PlaceSch&& sch) = default;
	PlaceSch& operator=(PlaceSch&& sch) = default;

	// Getter functions
	DataLayout& getIfmL();
	const DataLayout& getIfmL() const;
	DataLayout& getWgtL();
	const DataLayout& getWgtL() const;
	UniqueLayout& getOfmL();
	const UniqueLayout& getOfmL() const;

	// Initialize placement.
	void initPlacement(const Cluster& cluster);

	// Update placement scheme from *sch*, only update "part" and "order".
	void update(PlaceSch&& sch);

	// See "notes about finalize()" in datalayout.h
	void finalize();

	friend std::ostream& operator<<(std::ostream& os, const PlaceSch& sch);
};

class PlaceIter;

class PlaceEngine{
public:
	PlaceEngine()=default;

	// Returns the iterator for placement schemes, which will be iterated in "cur_sch.order".
	PlaceIter init(PlaceSch& cur_sch);
}extern placeEngine;

class PlaceIter{
	friend PlaceEngine;

	std::uint8_t perm_len;
	bool hasNext;
	PlaceSch& curSch;
public:
	PlaceIter(PlaceSch& placeSch);

	// Next function for iterator, returns whether iterator is valid.
	bool nextPlace(cost_t cost = cost_inf);

	/**
	 * @brief operator bool: same as "eof" in an IO stream.
	 * Returns whether iterator is valid.
	 */
	operator bool() const;
};

#endif // PLACEMENT_H
