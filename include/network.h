/* This file contains
 *	InputData: Describes the input data of the whole network.
 *  Node:      Represents a layer in the network.
 *  Network:   Represents a NN network.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <cstdint>
#include <memory>
#include <vector>

#include "bitset.h"
#include "layer.h"
#include "util.h"

class CoreMapper;
//#include "coremapping.h"


class InputData{
private:
	std::string name;
	fmap_shape data_shape;

public:
	InputData(const std::string& _name, const fmap_shape& _data_shape);

	const fmap_shape& get_shape() const;

	~InputData()=default;
};

/*
 * A node represents a layer in the network, with prev/next info.
 *
 * Here we use Node instead of directly using Layer, since
 * 1. There will be many kinds of derived class from layer (Conv, Pool, ...)
 *        where a single Node class is more friendly to Network.
 * 2. In the future one node may contain several layers,
 *        which means in this way we can maintain compatibility.
 */
class Node{
public:
	typedef std::vector<lid_t> layer_set;

private:
	// The underlying layer.
	std::unique_ptr<const Layer> l;

	// Previous layers.
	const Bitset ifmPrevs; // previous inputs
	const Bitset wgtPrevs; // for GroupConv, where weight is also fmap.
	const Bitset prevs;    // prevs = ifmPrevs + wgtPrevs

	// Next layers.
	Bitset nexts;

	// #channels that comes from InputData
	len_t external_C;

public:
	Node(const Layer* _l, const Bitset& _ifmPrevs, len_t _external_C, bwidth_t width = 0, const Bitset& _wgtPrevs = {});
	Node(const Node& n) = delete;
	Node(Node&& n)=default;

	// Getter functions.
	const Layer& layer() const;
	const std::string& name() const;
	const Bitset& getIfmPrevs() const;
	const Bitset& getWgtPrevs() const;
	const Bitset& getPrevs() const;
	const Bitset& get_nexts() const;
	utime_t get_utime() const;
	len_t get_external_C() const;

	// Whether weight comes from prev layer's fmap (e.g. in GroupConv)
	bool hasWgtPrevs() const;

	// Adds l to "nexts"
	void add_next(lid_t l);

	~Node()=default;
};

class Network{
public:
	typedef Node::layer_set layer_set;

private:
	std::vector<InputData> inputs;
	std::vector<Node> layers;

	// Used to check data range validity.
	[[noreturn]] void err_mismatch(const std::string& lname, const fmap_shape& shape1, const fmap_shape& shape2, bool total=false);
	[[noreturn]] void err_eltwise(const std::string& lname, const len_t from_C, const len_t add_C, const len_t elt_C);

public:
	Network();
	Network(const Network& n)=delete;
	Network(Network&& n)=default;

	/*
	 * Append a new layer to the network
	 *
	 * [input]
	 *  l:        the layer to be added
	 *  ifmPrevs: previous layers (for ifmap)
	 *  width:    bitwidth (currently not used)
	 *  ext_data: external data (input of the network)
	 *  wgtPrevs: previous layers (for weight, used in GroupConv)
	 *
	 * [output]
	 *  index of the added layer
	 */
	lid_t add(const Layer* l, const layer_set& ifmPrevs={}, bwidth_t width=0, std::vector<InputData> ext_data={}, const layer_set& wgtPrevs={});

	// Get the i-th node.
	const Node& getNode(lid_t id) const;
	const Node& operator[](lid_t id) const;

	// Length of the network
	lid_t len() const;

	// Chain: a network where node i depends on node i-1
	bool is_chain() const;

	// Checks whether direct edge "s->d" exists for any s in src, d in dst.
	bool has_dep(const Bitset& src, const Bitset& dst) const;

	// Sets the utime of each node/layer. (utime: NPT in SET paper)
	void set_utime(const CoreMapper& mapper) const;

	~Network()=default;
};

// Global pointer to the network being scheduled.
extern const Network* network;

#endif // NETWORK_H
