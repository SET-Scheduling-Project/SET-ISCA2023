/* This file contains
 *	LayerScheme:    whole scheme of scheduling a layer
 *  LayerEngine:    base class for searching LayerScheme
 *  StdLayerEngine: standard implementation of LayerEngine
 *
 *  One can add their own implementation of layer scheme searching as classes here.
 */

#ifndef LAYERENGINE_H
#define LAYERENGINE_H

#include "coremapping.h"
#include "noc.h"
#include "placement.h"
#include "schnode.h"
#include "util.h"


struct LayerScheme{
	// Total cost.
	SchNode::SchCost totCost;
	// Used to update external ubuf cost.
	energy_t extUbufEnergy;
	// Tiling scheme by CoreMapper.
	CoreMapper::CoreMapping tileSch;
	// Placement scheme
	PlaceSch place;
	// Noc Info
	NoC noc;

	// Whether scheme is valid (determined by totCost)
	bool isValid() const;
};

class LayerEngine{
public:
	virtual vol_t get_ubuf_size() const = 0;

	// Searches and returns best scheme for current layer
	virtual LayerScheme search(LNode* curNode) const = 0;
};

class StdLayerEngine : public LayerEngine{
	CoreMapper* mapper;

	// Sets placement *place* when partition *place.part* is fixed
	void initLayouts(PlaceSch& place, const Node& layerT, const fmap_shape& ofmShape, len_t B) const;

	// Calculates NoC *noc* from current placement *place*
	void calcNoC(NoC& noc, const PlaceSch& place, LNode* curNode) const;

public:
	StdLayerEngine(CoreMapper* _mapper);

	virtual vol_t get_ubuf_size() const override;
	virtual LayerScheme search(LNode* curNode) const override;
};

#endif // LAYERENGINE_H
