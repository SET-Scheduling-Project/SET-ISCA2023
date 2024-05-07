#ifndef LAYERENGINE_H
#define LAYERENGINE_H

#include "coremapping.h"
#include "memlayout.h"
#include "noc.h"
#include "placement.h"
#include "schnode.h"
#include "util.h"


struct LayerScheme{
	// Total cost.
	SchNode::SchCost totCost;
	// Used to update external ubuf cost.
	energy_t extUbufEnergy;
	CoreMapper::CoreMapping tileSch;
	PlaceSch place;
	std::vector<MemLayout> iMemLayouts;
	MemLayout oMemLayout;
	NoC noc;
	bool isValid() const;
};

class LayerEngine{
public:
	virtual LayerScheme search(LNode* curNode) const = 0;
	// TODO: put it somewhere else.
	virtual vol_t get_ubuf_size() const = 0;
	virtual bool updateNoC(LNode* curNode, NoC& old_noc) const = 0;
};

class StdLayerEngine : public LayerEngine{
	CoreMapper* mapper;
public:
	StdLayerEngine(CoreMapper* _mapper);
	virtual vol_t get_ubuf_size() const override;
	virtual LayerScheme search(LNode* curNode) const override;
	void initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const;
	void calcNoC(NoC& noc, const PlaceSch& place, std::vector<MemLayout>& iMemLayouts, MemLayout& oMemLayout, LNode* curNode) const;
	virtual bool updateNoC(LNode* curNode, NoC& old_noc) const override;
};

#endif // LAYERENGINE_H
