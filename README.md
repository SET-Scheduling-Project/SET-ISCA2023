# SET-ISCA2023

The framework for the paper ***["Inter-layer Scheduling Space Definition and Exploration for Tiled Accelerators"](https://dl.acm.org/doi/10.1145/3579371.3589048)*** in ISCA 2023. If you utilize this tool for your academic work, we kindly request that you reference our paper and send us a citation in your work. For industry applications, we kindly request that you inform us via email. We warmly welcome potential collaborations in both academic and industrial domains, and we are committed to offering our best support.

We will continue updating the framework codes and annotations in the future.

The "Scheduling Space Size Calculation" and "Optimal HW-tile Allocation Algorithm" mentioned in the paper can be found in ***[this repo](https://github.com/SET-ISCA2023/Tile-Alloc-Algorithm)***. The same repo is cited in the paper.


## How to run

1. Run *make* to build the executable "./build/stschedule"

2. Run "./build/stschedule [IR_file] < input_file"

- Where IR_file is the file name of the output IR json.

- input_file is a plain text file with the following format, an example can be seen at *example.txt*:

  - "dataflow net batch x y stride round cost bw"

    - dataflow: Dataflow of the PE array, 0 for Simba, 1 for Eyeriss.

    - net: Workload (NN network), we support 13 networks listed below:

      0: darknet19
      
      1: vgg19

      2: resnet50

      3: googlenet

      4: resnet101

      5: densenet

      6: inception_resnet_v1

      7: gnmt

      8: lstm

      9: zfnet

      10: transformer

      11: transformer (one cell)

      12: pnasnet

	- batch: Workload batch size.

    - x, y: Length of the x/y axis in the mesh.

    - stride: The stride used in initial placement, must be a divisor of x.

    - round: Parameter controlling #rounds in SA, #rounds = round * "#layers in net"

    - cost: Cost function, we use $e$ and $d$ to represent total energy and delay.

	  - When cost = 0, cost function is $d$.
	  - When cost = -1, cost function is $e$.
	  - When cost > 0, cost function is $e^{cost}*d$.
	  - Otherwise, cost function is $e*d^{-cost}$.
	  - (Setting cost_f = 1 will set EDP as the cost function)

	- bw: Bandwidth of each NoC link.

  - Since these parameters are read by cin, you can also run the framework using:
    - "echo *dataflow net batch x y stride round cost_f bw* | ./build/stschedule [IR_file]"

- The current running method is not elegant, and we will improve it soon.

## Current Plans

Since current APIs of the main program mostly relies on cin, it is mostly inconvenient and difficult to start with.

Thus we are planning to add better input formats, i.e. config files, and command line arguments.

Also, we are planning to add comments or docs for better understanding of the modules(classes) in the codes.

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
