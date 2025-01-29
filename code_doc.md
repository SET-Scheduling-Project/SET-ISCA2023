# About SET Codes

This documentation contains

- The overall structure of SET codes.

- The complete cost model of SET codes.

- How to add customized components to SET codes. (e.g. new type of layer, new core architecture, new scheduling algorithm, etc.)

- Notes about IR generation.

## Overall Structure

SET contains the files listed below:

The following files are mainly used in the searching process:

- `main.cpp`: Contains the main function, hardware configuration and parameter settings. Calls `sa.cpp` to run the SA searching algorithm.

  - `sa.h/cpp`: Contains the SA engine that performs the SA searching algorithm. Builds and mutates RA Trees. Each RA Tree has two representations, `LTreeNode` and `SchNode`.

  - Since in SA we need to frequently mutate the current RA Tree, we want a lightweighted Tree structure to minimize the computation of mutation. Thus mutation is only performed on `LTreeNode`, and after mutation we will generate the corresponding `SchNode` with scheduling information.

    - `ltreenode.h/cpp`: Contains `LTreeNode`, the pure structural representation of an RA Tree. `LTreeNode` includes structural information like parent/child, type, num_batch, etc., but contains no scheduling information (like core cluster, tiling, NoC hops, ...).

    - `schnode.h/cpp`: Contains `SchNode`, the whole RA Tree that contains all information of the scheduling scheme, including RA Tree structure, intra-layer scheduling scheme, buffer usage, NoC hops, DRAM accesses, etc.

    - `SchNode` computes the scheduling schemes in a bottom-up and recursive way: For each layer, `LNode` will call `LayerEngine` to get the best scheme and its detailed cost, which will be updated to the cost of `LNode`. Then, each `Cut` (S or T) will combine the cost of all its children, using specific rules determined by the Cut's type. For example, the energy cost of a `Cut` is the sum of the energy cost of all its children. Detailed rules can be found in the **Cost Model** section below.

      - `layerengine.h/cpp`: Contains `LayerEngine`, which finds the best intra-layer scheme of a single layer and computes its detailed cost. The best intra-core dataflows (e.g. for loop tiling and reordering) are searched and returned by `CoreMapper`, while all other components, including partition & placement, NoC, DRAM and data layout, are handled inside `LayerEngine`.

        - `coremapping.h/cpp`: Contains `CoreMapper`, which finds the best intra-core dataflow. In Eyeriss/Polar, this includes finding the tiling and order of the for loops, while handling buffer/bus bandwidth and intra-PE partition.

<br/>

The following files models different hardware components:

- `core.h/cpp`: Contains `Core`, which describes the hardware architecture of a compute core (*HW-tile* in SET paper).

- `cluster.h/cpp`: Contains `Cluster`, which describes a set of cores (*HW-tile group* in SET paper). All available cores can be seen as the largest cluster (*HWT* in SET paper).

- `noc.h/cpp`: Contains `NoC`, which describes the hardware of NoC and DRAM, as well as the amount of NoC hops on each link and DRAM accesses. Since NoC and DRAM are both related to data I/O, they are handled in the same class.

<br/>

The following files describes scheduling schemes and related information:

- `partition.h/cpp`: Contains `PartSch`, which describes a partition scheme (how to divide a layer to multiple cores), and `PartEngine/PartIter`, which iterates through all valid partition schemes.

- `placement.h/cpp`: Contains `PlaceSch`, which describes a placement scheme (how to divide a layer to multiple cores, and what is the correspondence between sub-layers and cores), and `PlaceEngine/PlaceIter`, which iterates through all valid placement schemes.

- `bufferusage.h/cpp`: Contains `BufferUsage`, which records the amount of occupied buffer on each core.

- `datalayout.h/cpp`: Contains `DataLayout`, which records the layout of ifmaps, weights and ofmaps on the cores, i.e. the tiles that each core stores.

<br/>

The following files describes NN networks:

- `layer.h/cpp`: Contains `Layer`, which describes a layer in the NN network.

- `network.h/cpp`: Contains `Network`, which describes a NN network.

- `nns/*`: Contains NN networks modelled by SET. For a complete list, see `README.md` or `nns/nns.h`.

<br/>

The following files provides definitions and helper types and functions for SET:

- `bitset.h/cpp`: Contains `Bitset`, which is almost an alias for `std::bitset`.

- `util.h/cpp`: Contains definition for all basic data type definitions (`cycle_t` for latency, `cost_t` for energy, etc.) and useful functions.

## Cost Model

From a high-level perspective, the cost model of SET contains two parts: *intra-layer* and *inter-layer*.

- For each network layer, the cost of scheduling that layer is determined by intra-layer scheduling in `LayerEngine`.

- Then the cost of the whole network is recursively computed in `SchNode` (and its derived classes):
  - Firstly, the cost of each *LNode* is derived from intra-layer scheduling, as mentioned above.

  - Then for each *Cut*, its cost is recursively computed from the cost if its children. The computation methods are defined according to the type of the cut.

Detailed cost models of SET are explained below.

(In the following sections, denote `n` as the number of subbatches of a *Cut*)

### Intra-layer Computation

The cost of a layer (*LNode*) is computed by `CoreMapper` (intra-core dataflow part) and `LayerEngine` (other parts).

For each layer, `LayerEngine` will iterate through all valid partition and placement schemes, compute the cost of each scheme, and returns the scheme with minimal cost. For each partition and placement scheme,

- `LayerEngine` calls `CoreMapper` to get the intra-core dataflow cost, including total energy, total latency and energy of each hardware component.

- Then `LayerEngine` counts the number NoC hops of each link and DRAM access according to the current placement scheme, while adding up access to the global buffer and recording global buffer usage.

- At last, `LayerEngine` adds up the energy cost of intra-core, NoC hops, DRAM access and global buffer access, to get the overall energy of the layer. Latency is also taken as the maximal of intra-tile computation and DRAM access latency.

The cost modeling of `CoreMapper` is similar to existing works on intra-core dataflow: we just enumerate through different loop tiling schemes, counting the number of buffer, bus and register access for each scheme, and multiplying it with the unit energy and latency to get the total cost of each scheme.

### Inter-layer Computation

The cost of a *Cut* is computed recursively from the cost of each child.

Computation of the energy and latency of the *Cut* are quite simple.

- The total energy, and component-wise energy of the *Cut* is just the sum of the energy of all its children multiplied by `n`.

- Computation of the total latency for *TCut* and *SCut* are slightly different

  - For a *TCut*, since the children are processed one by one, the latency of the *TCut* is just the sum of the latency of all its children multiplied by `n`.

  - For a *SCut*, the children are processed in parallel, so simply speaking, the total latency should be the maximal child latency multiplied by `n`. However, when there are dependencies from child A to child B, child B must start at least one subbatch later than child A, forming a "pipeline". So we need to compute the starting subbatch of each child (`stage` variable in class `SCut`), and multiply by `n + max(stage)` instead.

By recursively calculating from bottom (leaf nodes) to top (root node), we can get the total energy and latency of the root *Cut*, which is just the cost of the RA Tree.

For "the number of NoC hops on each link" and "the number of DRAM accesses", the computation are in the same format: adding up each child, and then multiply by `n`.

### Buffer Usage

The only complicated part is the computation of buffer usage. We also model buffer usage in a recursive way. Each node in the RA Tree has three buffer usages:

- Ifmap Usage (`ifm_usage`): The maximal amount of buffer needed for data prefetching (before computation).
  - In SET, the ifmaps of a layer are prefetched and stored one subbatch before computation. Since each layer in SET produces ofmaps in a "streaming" way, if the ifmaps come from another layer on-chip, the producer layer will constantly send tiles of fmaps (with size `ofm_ubuf_vol`) to the consumer layer, and the consumer layer is responsible for pre-buffering its ifmaps. If the ifmaps come from DRAM, they will also be prefetched and pre-buffered.

- Weight Usage (`wgt_usage`): The maximal amount of buffer needed for weights.
  - In SET, normally weights are pinned on-chip during the computation of the segment. In addition, weights of multiple batches has the same size with that of one batch, which is different to ifmaps. Therefore, weights needs to be modelled additionally.

- Buffer Usage (`buf_usage`): The maximal amount of buffer needed during computation, excluding weights.

For *LNode*, `ifm_usage` is the buffer needed for ifmaps, `wgt_usage` is the buffer needed for weights, and `buf_usage` is the buffer needed for ifmaps and ofmaps.

For *Cut*, these buffer usages can be computed recursively from the buffer usages of its children. Here we will introduce the common cases. For detailed model with all corner-cases, one can refer to `SCut::construct()` and `TCut::construct()` in `schnode.cpp`.

- `ifm_usage`: In a recursive view, the whole *Cut* is seen as a new "merged layer", and its ifmaps are the combination of all the ifmaps of its children. So its `ifm_usage` is the sum of that of its children multiplied by `n`.

- `wgt_usage`: Similar to `ifm_usage`, the weights of a *Cut* is the combination of all the weights of its children, so its `wgt_usage` is the sum of `wgt_usage` of its children. Notice that here we do not need to multiply by `n`.

- `buf_usage`: For `buf_usage` we need to consider *SCut* and *TCut* separately.
  - In an *SCut*, different child are processed on different clusters, so their buffer are distinct. Thus, the `buf_usage` of children can be directly added up to the `buf_usage` of the *SCut*. However, when the *SCut* has multiple subbatches (`n > 1`), each child needs to compute multiple times, so the prefetched ifmaps of the `i+1`-th subbatch need to be stored during the computation of the `i`-th subbatch. Hence in this situation the `buf_usage` of the *SCut* is the sum of `buf_usage + ifm_usage` of its children.
  - In a *TCut*, its children are processed one by one. Thus, the buffer usage when processing child `i` is "`buf_usage` of child `i`" plus "`ifm_usage` of child `i+1`". Therefore, the `buf_usage` is the maximal of the above sum over all possible `i`. Notice that the first and last child are slightly different, and "wraparound" needs to be considered when `n > 1`.

At last, `buf_usage + wgt_usage` of each segment is checked to make sure on-chip global buffer can store all data. If the sum exceeds the capacity of the global buffer, the scheme will be invalidated.

## How to Add Customized Components

We have summarized some common situations of adding new components below. For other situations, one can refer to the comments in the codes.

### Add New Core Architecture

To add a new core architecture, one needs to model the hardware components, as well as the intra-core dataflow (e.g. loop tiling and reordering). Detailed steps may include:

1. For hardware: Add a new class (e.g. `MyCore`) that inherits the base class `Core`.

- `MyCore` must implement the method `ubuf()`, which returns the global buffer of the core, for intra-layer scheduling.

- One may also include possible hardware components like PE array, bus, L1/L2 buffer, etc. in `MyCore`.

2. For intra-tile: Add a new class (e.g. `MyMapper`) that inherits `CoreMapper`.

- `MyMapper` must implement the method `genMapping(wl)`, which searches the best intra-tile dataflow for the workload `wl`, and returns its cost.

- If there are specific hardware characteristics that affects NPT (e.g. the core maps 8 channels at a time, and a workload of 4 channels will result in 50% utilization), one may also implement the methods `set_conv_utime(l)` and `set_lr_utime(l)` (overrides default implementation).

### Add New Intra-layer Scheduling Algorithm

To add a new intra-layer scheduling algorithm, one needs to:

1. Add a new class (e.g. `MyLayerEngine`) that inherits the base class `LayerEngine`.

- `MyLayerEngine` must implement the method `search(curNode)`, which searches the best inter-layer scheduling scheme for the node `curNode`, and returns the scheme as a `LayerScheme` object. A `LayerScheme` includes latency, energy, intra-tile dataflow, placement, NoC hops and DRAM accesses.

- `MyLayerEngine` must implement the method `get_ubuf_size()`, which gives the total capacity of the global buffer on a core. In most cases, one can just copy the implementation in `StdLayerEngine`.

- It is recommended to include a `CoreMapper*` pointer in `MyLayerEngine` for intra-tile dataflow searching. However, if the hardware core is also newly defined, and searching of intra-tile dataflow can be merged with intra-layer scheduling, one may also merge the functionality of the CoreMapper into `MyLayerEngine`, and directly searches the whole intra-layer scheduling.

### Add New Type of NN Layer

To add a new type of NN layer, one needs to:

1. Add a new class (e.g. `MyLayer`) that inherits either `ConvLayer` or `LRLayer`.

- `MyLayer` may override any virtual functions if the layer has its own implementation. For detailed information of each function, one can refer to the code comments.

- If the layer can be seen as a type of Convolution Layer, `MyLayer` should inherit `ConvLayer`; if the layer can be seen as a Local-region Layer, `MyLayer` should inherit `LRLayer`.
  - Convolutions are performed on PE arrays, with detailed searching of loop tiling; while Local-region Layers, which comes from Tangram, are directly performed on vector processing units.
  - Examples of Local-region Layers contains Pooling and Element-wise Add, which are all lightweighted ops that can be performed in a nearly point-to-point way, or region-to-point way if more precisely described.

- If the layer cannot be seen as either Convolution or Local-region layer, `MyLayer` may also directly inherits `Layer`. In this case, one should carefully check all codes that used `ConvLayer` or `LRLayer` (search globally under the folder), and modify each if necessary.
  - Luckily, except for IR generation, currently all occurrences of `ConvLayer` and `LRLayer` falls into `layer.h/cpp`, `layerengine.h/cpp`, `coremapping.h/cpp`. Therefore, one may only need to modify `LayerEngine` and `CoreMapping` in this case. This is **necessary** since one need to define the search algorithm of both intra-layer scheduling and intra-tile dataflows for a purely new layer.

### Add New NN Network

To add a new NN network, one needs to:

1. Add a new file `nns/net_name.cpp` in the `nns` folder, and put the construction of the NN network in this file.

- One can refer to existing files for examples of network construction.

- If the network contains a new type of layer that is not modelled by SET (not in `layer.h`), refer to section **Add New Type of NN Layer**.

2. Add a line `extern const Network net_name;` in `nns/nns.h`, so that `main.cpp` can see this network. `net_name` is the name of the Network in the cpp file.

3. Add a line in the definition of `All_Networks` in `main.cpp`, so that the main function can find the network via its name (defined as the key).

## Notes About IR Generation

Currently, IR generation can be switched *on* or *off* by both *file input* and *bash input*.

However, in some cases the IR file becomes very huge (in GBs) and IR generation will consume a considerable amount of memory (also in GBs). This happens when running large networks with large batches.

Thus, for those who prefer a lightweighted program and want to turn IR generation off permanently, we provide macros to remove IR generation codes during compilation.

To remove IR generation in compilation, one simply needs to define the macro `NOT_GEN_IR`, either by uncommenting it in `util.h`, or by adding it to the compiling flags of the makefile.
