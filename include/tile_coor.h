#ifndef TILE_COOR_H
#define TILE_COOR_H

#include "partition.h"
#include "util.h"

class TileCoor{
	const PartSch tile_part;
public:
	struct Coor{
		const len_t b,c,h,w;
		const PartSch* tile_part;
		Coor(len_t _b, len_t _c, len_t _h, len_t _w, const PartSch& _tile_part);
		fmap_range to_tile(const fmap_range &range);
	};
	class Iterator{
		len_t b,c,h,w;
		const TileCoor &tile_coor;
	public:
		Iterator(const TileCoor& _tile_coor);
		Iterator(const TileCoor& _tile_coor, len_t _b, len_t _c, len_t _h, len_t _w);
		Iterator& operator++();
		Coor operator*() const;
		bool operator!=(const Iterator& rhs) const;
	};
	TileCoor(const PartSch &_tile_part);
	Iterator begin() const;
	Iterator end() const;
};

#endif //TILE_COOR_H