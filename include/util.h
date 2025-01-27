/* This file contains
 *	pos_t:      Represents the position of a core
 *  fmap_shape: Represents the shape of fmap (C, H, W)
 *  fmap_range: Represents the range of a data block (C, B, H, W)
 *  Helper functions, type definitions, etc.
 */

#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <iostream>


// #define NOT_GEN_IR
#define MAX_CHIPS 258

// Need to guarantee that x is not 0
#define DIVCEIL(x,y) (((x)==0)?0:((x)-1)/(y)+1)
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

#define IS_INSTANCE(ptr, cls) (dynamic_cast<const cls*>(ptr) != nullptr)
#define REF_IS_INSTANCE(ref, cls) IS_INSTANCE(&(ref), cls)

// Basic type definition.

#define DEF_MAX_(tname, max_name)\
constexpr tname max_name = (std::numeric_limits<tname>::has_infinity)?(std::numeric_limits<tname>::infinity()):(std::numeric_limits<tname>::max());

#define DEF_MAX(type) DEF_MAX_(type##_t, type##_inf)

typedef std::uint32_t len_t;

// Volume of UBUF/REGF/... (in bytes)
typedef std::uint64_t vol_t;

typedef std::uint64_t access_t;

typedef double energy_t;
DEF_MAX(energy);

typedef std::uint64_t cycle_t;

typedef std::uint16_t bw_t;
DEF_MAX(bw);

typedef double cost_t;
DEF_MAX(cost);

typedef std::uint16_t lid_t;

typedef std::int16_t cidx_t;

typedef std::int8_t mlen_t;
#define IO_INT8_

// The unit time of one layer (one core && one batch).
typedef double utime_t;

// The bitwidth of data (in bits).
typedef std::uint8_t bwidth_t;
#define IO_UINT8_

#undef DEF_MAX_
#undef DEF_MAX

#ifdef IO_INT8_
#undef IO_INT8_
std::istream& operator>>(std::istream& in, std::int8_t& num);
std::ostream& operator<<(std::ostream& out, const std::int8_t& num);
#endif

#ifdef IO_UINT8_
#undef IO_UINT8_
std::istream& operator>>(std::istream& in, std::uint8_t& num);
std::ostream& operator<<(std::ostream& out, const std::uint8_t& num);
#endif


// Function, struct and global variables definition.

extern vol_t ofm_ubuf_vol;

extern std::function<cost_t(energy_t, cycle_t)> cost_func;

extern cost_t calc_cost(energy_t energy, cycle_t time);

// part_intv guarantees that the first element is always non-zero.
extern len_t* part_intv(len_t tot_len, len_t ncuts);

struct pos_t{
	typedef std::uint16_t pos_hash_t;
	mlen_t x,y;

	bool operator<(const pos_t& other) const;
	bool operator==(const pos_t& other) const;
	bool operator>(const pos_t& other) const;
	bool operator<=(const pos_t& other) const;
	bool operator>=(const pos_t& other) const;
	bool operator!=(const pos_t& other) const;

	friend std::ostream& operator<<(std::ostream& os, const pos_t& pos);
};

struct pos_hash {
	std::size_t operator()(const pos_t& pos) const {
		return std::hash<pos_t::pos_hash_t>{}(*reinterpret_cast<const pos_t::pos_hash_t*>(&pos));
	}
};

struct fmap_shape{
	len_t c, h, w;
	vol_t size;

	fmap_shape()=default;
	fmap_shape(len_t _c, len_t _h, len_t _w=0);

	bool operator==(const fmap_shape& other) const;

	void update_size();
	vol_t tot_size(len_t batch_size) const;

	friend std::ostream& operator<<(std::ostream& os, const fmap_shape& shape);
};

/* the range is a left-closed & right-opened interval
 * i.e. [from.X, to.X)
 */
struct fmap_range{
	struct dim_range{
		len_t from, to;

		bool operator<(const dim_range& other) const;
		bool operator==(const dim_range& other) const;
		bool operator!=(const dim_range& other) const;
		dim_range& operator+=(const len_t& offset);
		dim_range& operator-=(const len_t& offset);

		bool is_empty() const;
		vol_t size() const;
		dim_range intersect(const dim_range& other) const;

		friend std::ostream& operator<<(std::ostream& os, const dim_range& range);
	}c, b, h, w;

	fmap_range()=default;
	fmap_range(const dim_range& _c, const dim_range& _b, const dim_range& _h, const dim_range& _w);
	explicit fmap_range(const fmap_shape& shape, len_t B = 1);

	bool operator<(const fmap_range& other) const;
	bool operator==(const fmap_range& other) const;

	vol_t size() const;
	bool is_empty() const;

	const dim_range& get_range(std::uint8_t idx) const;
	fmap_range intersect(const fmap_range& other) const;

	friend std::ostream& operator<<(std::ostream& os, const fmap_range& range);
};

#endif // UTIL_H
