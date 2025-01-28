/* This file contains
 *	DataLayout:    base class for all data layouts (which core stores which data in a layer)
 *  UniqueLayout:  base class for data layouts without duplication
 *  StdDataLayout: standard implementation of DataLayout
 *  StdULayout:    standard implementation of UniqueLayout
 *
 *  One can add their own implementation of data layouts as classes here.
 */

#ifndef DATALAYOUT_H
#define DATALAYOUT_H

#include <memory>
#include <vector>
#include "util.h"

class BufferUsage;
class StdLayerEngine;
//struct PartSch;
//#include "bufferusage.h"
//#include "layerengine.h"
//#include "partition.h"


/*
 * notes about finalize():
 *
 * During scheme exploration of a layer, multiple
 * placement schemes (permutations of which core stores which data)
 * will be searched. Since this is used by datalayouts of
 * ifmaps, weights and ofmaps, the scheme is stored in
 * PlaceSch::permuteOrder, and shared among
 * ifmLayout, wgtLayout and ofmLayout in PlaceSch.
 *
 * Thus, one modification of permuteOrder can modify three datalayouts,
 * which makes the searching more efficient.
 *
 * However, after the searching process, when permuteOrder is fixed,
 * we want each datalayout to store its own share of data
 * in its preferred format. This is when finalize() is called.
 * finalize() will initialize the datalayout's own data structure,
 * and copy the data of permuteOrder into the datalayout.
 * In this way, the datalayout is "finalized".
 */

/**
 * @brief Base class for all data layouts
 */
class DataLayout{
	friend StdLayerEngine;

public:
	typedef cidx_t dataLen_t;

	/**
	 * @brief The Entry class
	 * Describes that "1/divN" of "range" is stored/needed by
	 * the array "tiles" of length "numTile".
	 *
	 * Note: divN is not used (always equal to 1), thus commented out.
	 * Note: tiles needs to be in increasing order for NoC to compute correctly.
	 */
	struct Entry{
		const fmap_range& range;
		const pos_t* tiles;
		dataLen_t numTile;
		// dataLen_t divN;
	};

	/**
	 * @brief The UniqueEntry class
	 * Describes that "1/divN" of "range" is stored/needed by "tile".
	 *
	 * Note: divN is not used (always equal to 1), thus commented out.
	 */
	struct UniqueEntry{
		const fmap_range& range;
		const pos_t& tile;
		// dataLen_t divN;
	};

protected:
	// totVolume: total volume of all data (sum of all cores)
	// maxVolume: maximal volume of data on one core
	vol_t totVolume, maxVolume;

	/*
	 * multFactor: for faster mult (lazy update).
	 *
	 * When the size of all buffers are multiplied by n,
	 * instead of multiplying all items in the dict,
	 * we only need to multiply *multFactor* by n.
	 * *multFactor* will be expanded in later operations.
	 */
	len_t multFactor;

	// Only updates *totVolume* and *maxVolume* by "range.size()"
	void update(const fmap_range& range);

public:
	DataLayout();

	// Returns totVolume
	vol_t totalSize() const;
	// Returns maxVolume
	vol_t maxRange() const;

	// Mult all data size by *num*
	void sizeMult(len_t num);
	// Clears all data size
	void clear();

	// Updates *usage* by this DataLayout.
	// Returns whether *usage* is valid.
	bool update(BufferUsage& usage) const;

	// Duplicate current DataLayout
	virtual DataLayout* clone() const = 0;

	// See "notes about finalize()"
	virtual void finalize() = 0;

	// Resets current DataLayout (dict)
	virtual void reset() = 0;

	// Total num of data blocks (with duplication)
	virtual dataLen_t totLength() const = 0;
	// Total num of data ranges (without duplication)
	virtual dataLen_t rangeLength() const = 0;
	// Duplication length (totLength() / rangeLength())
	dataLen_t bcastLength() const;

	// Functions for iteration

	// The idx-th data range (duplicated data in one entry)
	// 0 <= idx < rangeLength()
	virtual Entry at(dataLen_t idx) const = 0;
	// The idx-th data block (duplicated data in multiple entries)
	// 0 <= idx < totLength()
	virtual UniqueEntry operator[](dataLen_t idx) const = 0;

	virtual ~DataLayout() = default;
};

class UniqueLayout : public DataLayout{
protected:
	// total length
	dataLen_t len;

public:
	// (const) iterator for this layout.
	class Iterator{
		dataLen_t i;
		const UniqueLayout& layout;
	public:
		Iterator(const UniqueLayout& _layout, dataLen_t _i = 0);
		Iterator& operator++();
		std::pair<fmap_range, pos_t> operator*() const;
		bool operator!=(const Iterator& other) const;
	};

	UniqueLayout(dataLen_t _len);

	// virtual void init(const PartSch& sch) = 0;

	Iterator begin() const;
	Iterator end() const;

	virtual UniqueLayout* clone() const override = 0;
	virtual void finalize() override = 0;
	virtual void reset() override = 0;
	virtual dataLen_t totLength() const override final;
	virtual dataLen_t rangeLength() const override final;
	virtual Entry at(dataLen_t idx) const override final;
	virtual UniqueEntry operator[](dataLen_t idx) const override = 0;
};

class StdDataLayout : public DataLayout{
	friend StdLayerEngine;

private:
	/*
	 * rangeArr (len = range_len): stores data ranges
	 * contPosArr (len = tot_len): stores placed cores
	 * posArr: similar to contPosArr, but used before "finalize()"
	 *
	 * bcast (duplication) of data in posArr is in the following format:
	 * bcast has step "bcast_step" and len "bcast_len",
	 * i.e. bcast(i, bcast_step + i, ..., (bcast_len-1)*bcast_step + i)
	 * for i = 0, ..., bcast_step-1
	 *
	 * When bcast_len = 1, contPosArr is never used (posArr is used).
	 *
	 * see setCPosArr() for more details
	 */
	dataLen_t range_len, bcast_len, tot_len, bcast_step, bcast_down;
	std::unique_ptr<fmap_range[]> rangeArr;
	std::unique_ptr<pos_t[]> contPosArr;
	pos_t* posArr;

public:
	StdDataLayout(dataLen_t _len, pos_t* _posArr);

	// void init(const PartSch& sch, UniqueDataLayout& ofm_layout);

	virtual DataLayout* clone() const override;
	virtual void finalize() override;
	virtual void reset() override;
	virtual dataLen_t totLength() const override;
	virtual dataLen_t rangeLength() const override;
	virtual Entry at(dataLen_t idx) const override;
	virtual UniqueEntry operator[](dataLen_t idx) const override;

	// sets contPosArr
	void setCPosArr();
	// sets bcast params
	void setBcast(dataLen_t _bcastLen, dataLen_t _bcastStep);
};

class StdULayout : public UniqueLayout{
	friend StdLayerEngine;

private:
	/*
	 * dimLen, dimStep: records the length and step of each dim (BCHW)
	 * rangeArr: stores data ranges
	 * localPosArr: stores placed cores
	 * posArr: similar to localPosArr, but used before "finalize()"
	 */
	dataLen_t dimLen[4];
	dataLen_t dimStep[4];
	std::unique_ptr<fmap_range[]> rangeArr;
	std::unique_ptr<pos_t[]> localPosArr;
	pos_t* posArr;

public:
	// Iterator for all data ranges that intersects with a given data range.
	class IntersectIter{
		friend StdULayout;
		dataLen_t from[4], to[4], cur[4];
		const StdULayout& layout;
		// IntersectIter(const UniqueLayout& _layout);
		IntersectIter(dataLen_t _from[], dataLen_t _to[], const StdULayout& _layout);
	public:
		UniqueEntry operator*() const;
		bool isValid() const;
		void next();
	};

	StdULayout(dataLen_t _len, pos_t* _posArr);
	// virtual void init(const PartSch& sch) override;

	virtual UniqueLayout* clone() const override;
	virtual void finalize() override;
	virtual void reset() override;
	virtual UniqueEntry operator[](dataLen_t idx) const override;

	// set dimLen and dimStep
	void setDims(dataLen_t C, dataLen_t B, dataLen_t H, dataLen_t W);
	// get iterator for intersected entries (with *range*)
	IntersectIter get_intersect(const fmap_range& range, bool noBatch) const;
};

/*
class MemULayout : public UniqueLayout{
	friend StdLayerEngine;
private:
	fmap_range range;
	pos_t* posArr;
public:
	MemULayout(const fmap_range& _range, pos_t* _posArr, len_t memLen);
	// virtual void init(const PartSch& sch) override;
	virtual UniqueLayout* clone() const override;
	virtual void reset() override;
	virtual UniqueEntry operator[](dataLen_t idx) const override;
	//~MemULayout();
};
*/

#endif // DATALAYOUT_H
