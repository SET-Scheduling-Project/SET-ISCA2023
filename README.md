# SET-ISCA2023

The framework for the paper ***["Inter-layer Scheduling Space Definition and Exploration for Tiled Accelerators"](https://dl.acm.org/doi/10.1145/3579371.3589048)*** in ISCA 2023. If you utilize this tool for your academic work, we kindly request that you reference our paper and send us a citation in your work. For industry applications, we kindly request that you inform us via email. We warmly welcome potential collaborations in both academic and industrial domains, and we are committed to offering our best support.

We will continue updating the framework codes and annotations in the future.

The "Scheduling Space Size Calculation" and "Optimal HW-tile Allocation Algorithm" mentioned in the paper can be found in ***[this repo](https://github.com/SET-ISCA2023/Tile-Alloc-Algorithm)***. The same repo is cited in the paper.

## Code Documentation

For code related documentations, one can refer to [`code_doc.md`](code_doc.md), which contains:

- The overall structure of SET codes.

- The complete cost model of SET codes.

- How to add customized components to SET codes. (e.g. new type of layer, new core architecture, new scheduling algorithm, etc.)

Also, we have added comments for most functions and variables, which can also help understanding the codes.

If you have found any bugs or possible improvements of the codes, or have any SET-related questions, feel free to post issues in this repo, or contact us directly by email (see section **Contacting Us**).

## How to run

### QuickStart example

```
# Make the executable
make

# Run with bash inputs
bash ./bash_example.sh

# Run with file inputs
bash ./file_example.sh

# Then txt files `bash_exp_*.txt` and `file_exp_*.txt` will be produced.
```

### Detailed steps

1. Run `make` to build the executable `./build/stschedule`

2. Run the executable with either *file input* or *bash input*.

- *File Input*: `./build/stschedule config_file`

  - config_file is a text file with the value of all configuration parameters. The parameters to be set is the same as the *Bash Input* below.
  - See `file_in.txt` for an example of config_file.

- *Bash Input*: `./build/stschedule --args exp net batch core x y stride bw cost round gen_IR`

  - `exp`: Name of the current experiment, used in the name of the output files. (When set to `None`, experiment name will be empty)

  - `net`: Name of the workload (NN network), we currently support 16 networks:

    - `resnet`: ResNet-50
    - `resnet101`: ResNet-101
    - `ires`: Inception-ResNet-v1
    - `goog`: GoogLeNet
    - `densenet`: DenseNet
    - `darknet`: DarkNet-19
    - `vgg`: VGG-19
    - `zfnet`: ZFNet
    - `gnmt`: GNMT
    - `lstm`: LSTM
    - `trans`: Transformer
    - `trans_cell`: Transformer (one cell)
    - `pnas`: PNASNet
    - `bert`: BERT-Large (one cell)
    - `gpt_prefill`: GPT2-XL prefill stage (one cell)
    - `gpt_decode`: GPT2-XL decode stage (one cell)

    - Note: For the LLM models, we provide their one-cell version due to the excessive length and identical cell structure of the network. To run the full network, see the comments in "nns/llm.cpp".

  - `batch`: Workload batch size.

  - `core`: Core arch and dataflow. Currently supports `eyeriss` and `polar`.

  - `x`, `y`: Length of the x/y axis in the mesh.

  - `stride`: The stride used in initial placement, must be a divisor of x.

  - `bw`: Bandwidth of each NoC link.

  - `cost`: Cost function, we use $e$ and $d$ to represent total energy and delay.
    - When cost = 0, cost function is $d$.
    - When cost = -1, cost function is $e$.
    - When cost > 0, cost function is $e^{cost}*d$.
    - Otherwise, cost function is $e*d^{-cost}$.
    - (Setting cost_f = 1 will set EDP as the cost function)

  - `round`: Parameter controlling #rounds in SA, #rounds = round * "#layers in net"

  - `gen_IR`: (0 or 1) Whether generates the IR file or not.

### Output Files

By default, SET will output the following files:

- `{exp}_{type}_tree.txt`: The pure tree structure of the scheme.

- `{exp}_{type}_summary.txt`: The cost summary of the scheme.

- `{exp}_{type}_scheme.txt`: All information about the scheme, including cost/noc/dram/buffer/... of each node.

- (If `gen_IR` = 1) `{exp}_{type}_IR.json`: The generated IR file.

Here `exp` is the name of the current experiment. `type` is the search type.

There are four default search types:

- `init`: The initial RA Tree, used as input of SA. No SA search is performed. (Currently IR is not generated for the initial RA Tree)

- `LP`: Layer-pipeline (LP), only LP-pattern RA Trees are searched in SA.

- `LS`: Layer-sequential (LS), only LS-pattern RA Trees are searched in SA.

- `SET`: SA has no constraints, all valid RA Trees can be reached.

## Update History

2025/01/30 Improved input format. Added code documentation.

2025/01/28 Added comments, tidied-up codes.

2025/01/08 Added LLM models (BERT-Large, GPT2-XL prefill/decode).

2024/05/11 Initial version.

## Current Plans

Since ***[GEMINI](https://github.com/SET-Scheduling-Project/GEMINI-HPCA2024)*** is developed on top of SET, and their codes share the same format, we are working on merging SET and GEMINI together.

# Citations ###
```
@inproceedings{10.1145/3579371.3589048,
author = {Cai, Jingwei and Wei, Yuchen and Wu, Zuotong and Peng, Sen and Ma, Kaisheng},
title = {Inter-layer Scheduling Space Definition and Exploration for Tiled Accelerators},
year = {2023},
isbn = {9798400700958},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3579371.3589048},
doi = {10.1145/3579371.3589048},
abstract = {With the continuous expansion of the DNN accelerator scale, inter-layer scheduling, which studies the allocation of computing resources to each layer and the computing order of all layers in a DNN, plays an increasingly important role in maintaining a high utilization rate and energy efficiency of DNN inference accelerators. However, current inter-layer scheduling is mainly conducted based on some heuristic patterns. The space of inter-layer scheduling has not been clearly defined, resulting in significantly limited optimization opportunities and a lack of understanding on different inter-layer scheduling choices and their consequences.To bridge the gaps, we first propose a uniform and systematic notation, the Resource Allocation Tree (RA Tree), to represent different inter-layer scheduling schemes and depict the overall space of inter-layer scheduling. Based on the notation, we then thoroughly analyze how different inter-layer scheduling choices influence the performance and energy efficiency of an accelerator step by step. Moreover, we show how to represent existing patterns in our notation and analyze their features. To thoroughly explore the space of the inter-layer scheduling for diverse tiled accelerators and workloads, we develop an end-to-end and highly-portable scheduling framework, SET. Compared with the state-of-the-art (SOTA) open-source Tangram framework, SET can, on average, achieves 1.78\texttimes{} performance improvement and 13.2\% energy cost reduction simultaneously. Moreover, the SET framework will be open-sourced.},
booktitle = {Proceedings of the 50th Annual International Symposium on Computer Architecture},
articleno = {13},
numpages = {17},
keywords = {tiled accelerators, neural networks, inter-layer scheduling, scheduling},
location = {Orlando, FL, USA},
series = {ISCA '23}
}

```
```
@inproceedings{cai2024gemini,
  title={Gemini: Mapping and Architecture Co-exploration for Large-scale DNN Chiplet Accelerators},
  author={Cai, Jingwei and Wu, Zuotong and Peng, Sen and Wei, Yuchen and Tan, Zhanhong and Shi, Guiming and Gao, Mingyu and Ma, Kaisheng},
  booktitle={2024 IEEE International Symposium on High-Performance Computer Architecture (HPCA)},
  pages={156--171},
  year={2024},
  organization={IEEE}
}
```

## Contacting Us

If you have any questions, bug reports, or suggetions, feel free to contact the authors at *1148821791@qq.com* and *weiyc22@mails.tsinghua.edu.cn*, or post issues under this repo. (-:
