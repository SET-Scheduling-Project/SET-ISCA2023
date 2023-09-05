#ifndef LAYERENGINE_H
#define LAYERENGINE_H

#include "schnode.h"

class CoreMapper;
//#include "coremapping.h"

struct LayerScheme{
	// Total cost.
	SchNode::SchCost totCost;
	// Used to update external ubuf cost.
	energy_t extUbufEnergy;
	CoreMapper::CoreMapping tileSch;
	PlaceSch place;
	NoC noc;
	bool isValid() const;
};

class LayerEngine{
public:
	virtual LayerScheme search(LNode* curNode) const = 0;
	// TODO: put it somewhere else.
	virtual vol_t get_ubuf_size() const = 0;
};

class StdLayerEngine : public LayerEngine{
	CoreMapper* mapper;
public:
	StdLayerEngine(CoreMapper* _mapper);
	virtual vol_t get_ubuf_size() const override;
	virtual LayerScheme search(LNode* curNode) const override;
	void initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const;
	void calcNoC(NoC& noc, const PlaceSch& place, LNode* curNode) const;
};

#endif // LAYERENGINE_H
