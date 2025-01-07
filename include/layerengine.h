#ifndef LAYERENGINE_H
#define LAYERENGINE_H

#include "coremapping.h"
#include "memlayout.h"
#include "noc.h"
#include "placement.h"
#include "schnode.h"
#include "spatial_mapping/light_placement.h"
#include "util.h"

struct LayerScheme{
	// Total cost.
	SchNode::SchCost totCost, coreCost;
	// Used to update external ubuf cost.
	energy_t extUbufEnergy;
	CoreMapper::CoreMapping tileSch;
	PlaceSch place;
	MemLayouts memLayouts;
	NoC noc;
	bool isValid() const;
};

class LayerEngine{
public:
	virtual LayerScheme search(LNode* curNode) const = 0;
	// TODO: put it somewhere else.
	virtual vol_t get_ubuf_size() const = 0;
	virtual LayerScheme fillin(LNode* curNode, const Light_placement &place, bool calc_noc = true,bool base = false) = 0;
	virtual bool updateNoC(LNode* curNode, NoC& old_noc) const = 0;
};

class StdLayerEngine : public LayerEngine{
	CoreMapper* mapper;
public:
	StdLayerEngine(CoreMapper* _mapper);
	virtual vol_t get_ubuf_size() const override;
	virtual LayerScheme search(LNode* curNode) const override;
	void initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const;
	void initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B, const Light_placement &light_place) const;
	void calcNoC(NoC& noc, const PlaceSch& place, MemLayouts& memLayouts, LNode* curNode) const;
	virtual bool updateNoC(LNode* curNode, NoC& old_noc) const override;
	void calcNoC(NoC& noc, const PlaceSch& place, const Light_placement& light_place, LNode* curNode, bool unordered = false) const;
	virtual LayerScheme fillin(LNode* curNode, const Light_placement &place, bool calc_noc = true,bool base=false);
};

#endif // LAYERENGINE_H
