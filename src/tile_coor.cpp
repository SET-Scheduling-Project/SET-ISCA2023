#include "tile_coor.h"



TileCoor::Iterator::Iterator(const TileCoor& _tile_coor):
	b(0),
	c(0),
	h(0),
	w(0),
	tile_coor(_tile_coor)
{}

TileCoor::Iterator::Iterator(const TileCoor& _tile_coor, len_t _b, len_t _c, len_t _h, len_t _w):
	b(_b),
	c(_c),
	h(_h),
	w(_w),
	tile_coor(_tile_coor)
{}

TileCoor::Iterator& TileCoor::Iterator::operator++(){
	++w;
	if(w==tile_coor.tile_part.W){
		w=0;
		++h;
	}
	if(h==tile_coor.tile_part.H){
		h=0;
		++c;
	}
	if(c==tile_coor.tile_part.K){
		c=0;
		++b;
	}
	return *this;
}

TileCoor::Coor TileCoor::Iterator::operator*() const{
	return Coor(b,c,h,w,tile_coor.tile_part);
}

bool TileCoor::Iterator::operator!=(const Iterator& rhs) const{
	return b!=rhs.b ||
		c!=rhs.c ||
		h!=rhs.h ||
		w!=rhs.w;
}

TileCoor::TileCoor(const PartSch& _tile_part):
	tile_part(_tile_part)
{}

TileCoor::Iterator TileCoor::begin() const{
	return Iterator(*this);
}

TileCoor::Iterator TileCoor::end() const{
	return Iterator(*this,tile_part.B,0,0,0);
}

TileCoor::Coor::Coor(len_t _b, len_t _c, len_t _h, len_t _w, const PartSch& _tile_part):
	b(_b),
	c(_c),
	h(_h),
	w(_w),
	tile_part(&_tile_part)
{}

fmap_range TileCoor::Coor::to_tile(const fmap_range& range){
	auto fraction = [](len_t n, len_t d, len_t coor){
		return n*coor/d;
	};
	fmap_range ret;
	ret.b.from=range.b.from+fraction(range.b.size(),tile_part->B,b);
	ret.b.to=range.b.from+fraction(range.b.size(),tile_part->B,b+1);
	ret.c.from=range.c.from+fraction(range.c.size(),tile_part->K,c);
	ret.c.to=range.c.from+fraction(range.c.size(),tile_part->K,c+1);
	ret.h.from=range.h.from+fraction(range.h.size(),tile_part->H,h);
	ret.h.to=range.h.from+fraction(range.h.size(),tile_part->H,h+1);
	ret.w.from=range.w.from+fraction(range.w.size(),tile_part->W,w);
	ret.w.to=range.w.from+fraction(range.w.size(),tile_part->W,w+1);
	return ret;
}
