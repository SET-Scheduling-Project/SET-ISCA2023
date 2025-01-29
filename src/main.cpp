#include "cluster.h"
#include "layerengine.h"
#include "ltreenode.h"
#include "noc.h"
#include "schnode.h"
#include "util.h"
#include "nns/nns.h"

#include "sa.h"	         // Library for SA

#ifndef NOT_GEN_IR
#include "json/json.h"   // Json::StyledWriter
#endif

#include <cassert>       // assert
#include <cmath>         // std::pow
#include <cstdlib>       // std::srand, std::atoi
#include <ctime>         // std::time
#include <fstream>       // std::ifstream, std::ofstream
#include <functional>    // std::ref
#include <iostream>      // std::cin, std::cout, std::endl
#include <string>        // std::string
#include <thread>        // std::thread
#include <unordered_map> // std::unordered_map


static const std::unordered_map<std::string, const Network*> All_Networks = {
	{"resnet", &resnet50},
	{"resnet101", &resnet101},
	{"ires", &inception_resnet_v1},
	{"goog", &googlenet},
	{"dense", &densenet},
	{"darknet", &darknet19},
	{"vgg", &vgg19},
	{"zfnet", &zfnet},
	{"gnmt", &gnmt},
	{"lstm", &lstm},
	{"trans", &transformer},
	{"trans_cell", &transformer_cell},
	{"pnas", &PNASNet},
	{"bert", &BERT_block},
	{"gpt_prefill", &GPT2_prefill_block},
	{"gpt_decode", &GPT2_decode_block}
};

#define KB *1024

// Core-related hardware parameters defined in this function.
static void init_core(const std::string& core_type, Core*& core, CoreMapper*& cMapper);

int main(int argc, char** argv){
	unsigned seed = std::time(nullptr);
	std::srand(seed);

	// Number of threads for the SA experiment.
	// Each thread will run one concurrent SA.
	// Thus this is also the total number of tries.
	constexpr int tries = 4;

	// print_(.*): whether prints $1 to file
	constexpr bool print_summary = true;
	constexpr bool print_scheme = true;
	constexpr bool print_tree = true;

	std::cout.precision(4);

	/* ########## Configurations ########## */

	// Default parameters:

	// Experiment name, default empty
	std::string exp_name = "";

	// NN network name
	std::string net_name = "resnet";

	// Total batch size
	len_t tot_batch = 64;

	// Core: "polar" or "eyeriss"
	std::string core_type = "polar";

	// mesh size: x_len * y_len
	// stride is used in placement (similar to Tangram)
	int x_len = 8, y_len = 8, stride = 4;

	// NoC bandwidth: 24 GB/s
	bw_t noc_bw = 24;

	/*
	 * sets cost_function
	 * 1:  e*d
	 * 0:  d
	 * -1: e
	 * n:  e^n*d
	 * -n: e*d^n
	 */
	int cf_param = 1;

	// Rounds of SA = urounds * #layers
	int urounds = 100;

#ifndef NOT_GEN_IR
	// Whether generate IR or not.
	bool gen_IR = true;
#endif

	// Read from file / args
	{
		std::string config_file;
		if(argc > 1){
			config_file = argv[1];
			if(config_file == "--args"){
				config_file.clear();
#ifndef NOT_GEN_IR
				constexpr int arg_num = 11;
#else
				constexpr int arg_num = 10;
#endif
				if(argc != arg_num + 2){
					std::cout << "Should have " << arg_num << " args!" << std::endl;
					return 0;
				}
				int i = 1;
				exp_name = argv[++i];
				if(exp_name == "None") exp_name = "";
				net_name = argv[++i];
				tot_batch = std::stoi(argv[++i]);
				core_type = argv[++i];
				x_len = std::stoi(argv[++i]);
				y_len = std::stoi(argv[++i]);
				stride = std::stoi(argv[++i]);
				noc_bw = std::stoi(argv[++i]);
				cf_param = std::stoi(argv[++i]);
				urounds = std::stoi(argv[++i]);
#ifndef NOT_GEN_IR
				gen_IR = (std::stoi(argv[++i]) != 0);
#endif
			}
		}

		if(!config_file.empty()){
			std::ifstream in(config_file);
			if(!in){
				throw std::invalid_argument("Cannot read from config file!");
			}
			while(true){
				std::string config_name;
				in >> config_name;
				if(in.eof()) break;

				if(config_name == "exp"){
					in >> exp_name;
					if(exp_name == "None") exp_name = "";
				}else if(config_name == "net"){
					in >> net_name;
				}else if(config_name == "batch"){
					in >> tot_batch;
				}else if(config_name == "core"){
					in >> core_type;
				}else if(config_name == "x_len"){
					in >> x_len;
				}else if(config_name == "y_len"){
					in >> y_len;
				}else if(config_name == "stride"){
					in >> stride;
				}else if(config_name == "noc_bw"){
					in >> noc_bw;
				}else if(config_name == "cost_func"){
					in >> cf_param;
				}else if(config_name == "round"){
					in >> urounds;
#ifndef NOT_GEN_IR
				}else if(config_name == "IR"){
					in >> gen_IR;
#endif
				}else{
					throw std::invalid_argument("Config name \"" + config_name + "\" not recognized!");
				}

				if(!in){
					throw std::invalid_argument("Config file format not recognized!");
				}
			}
		}
	}
	if(!exp_name.empty()) exp_name += "_";

	// Other parameters

	Cluster::min_util = 0.75;
	ofm_ubuf_vol = 10 KB;
	// 0.5 (GB/s)/TOPS
	double rel_dram_bw = 0.5;

	// NoC and DRAM energy
	NoC::DRAM_acc_cost = 7.5 * 8;
	NoC::hop_cost = 0.7 * 8;

	// Sets DRAM ports
	NoC::dram_list.resize(2 * y_len);
	for(mlen_t y=0; y<y_len; ++y){
		NoC::dram_list[y] = {0, y};
		NoC::dram_list[y_len + y] = {static_cast<mlen_t>(x_len-1), y};
	}

	// Core/LayerEngine initialization
	Core* core;
	CoreMapper* cMapper;
	init_core(core_type, core, cMapper);
	StdLayerEngine engine(cMapper);
	SchNode::layerMapper = &engine;

	// Cluster initialization
	Cluster::xlen = x_len;
	Cluster::ylen = y_len;
	Cluster::stride = stride;
	Cluster c(0, Cluster::xlen * Cluster::ylen);

	// TOPS
	double tops = 2.0 * cMapper->core().mac_num * Cluster::xlen * Cluster::ylen;
	tops /= 1024;
	// Sets NoC and DRAM bandwidth
	NoC::DRAM_bw = rel_dram_bw * tops; // rel_dram_bw (GB/s)/TOPS
	NoC::NoC_bw = noc_bw;

	if(NoC::DRAM_bw <= 0 || NoC::NoC_bw <= 0){
		delete core;
		delete cMapper;
		throw std::invalid_argument("Bandwidth must be positive, got "
									+ std::to_string(NoC::DRAM_bw)
									+ " and "
									+ std::to_string(NoC::NoC_bw));
	}

	// Sets networks
	{
		auto it = All_Networks.find(net_name);
		if(it == All_Networks.end()){
			throw std::invalid_argument("Network \"" + net_name + "\" not found!");
		}
		network = it->second;
	}

	// Sets cost function
	switch (cf_param){
		case 1:
			// This is the default cost_func.
			//cost_func = [](energy_t e, cycle_t t){return e*t;};
			break;
		case 0:
			cost_func = [](energy_t, cycle_t t){return t;};
			break;
		case -1:
			cost_func = [](energy_t e, cycle_t){return e;};
			break;
		default:
			if(cf_param > 0){
				cost_func = [=](energy_t e, cycle_t t)->cost_t{return std::pow(e,cf_param)*t;};
			}else{
				cost_func = [=](energy_t e, cycle_t t)->cost_t{return e*std::pow(t,-cf_param);};
			}
	}

	// Sets total batch size
	SchNode::tot_batch = tot_batch;

	// Sets NPT
	network->set_utime(*cMapper);

	// Sets SA rounds
	lid_t num_layer = network->len();
	SAEngine::nrounds = urounds * num_layer;

	std::cout << "Seed: " << seed << std::endl;
	std::cout << "Core " << core_type;
	std::cout << " Network " << net_name;
	std::cout << " Mesh " << static_cast<int>(Cluster::xlen) << '*' << static_cast<int>(Cluster::ylen);
	std::cout << " Batch " << tot_batch << std::endl;

	/* ########## Search functions ########## */

	// Initial RA Tree
	LTreeNode* init_tree = nullptr;
	SchNode* init_res = nullptr;
	for(len_t lb = tot_batch; lb>0; lb/=2){
		init_tree = new LTreeNode(Bitset(), tot_batch, nullptr, LTreeNode::NodeType::T);
		for(lid_t i = 0; i < num_layer; ++i){
			(void)new LTreeNode(i, lb, init_tree);
		}
		init_tree->init_root();
		init_res = SchNode::newNode(init_tree, c, nullptr);
		if(init_res->is_valid()){
			init_tree->confirm();
			break;
		}
		delete init_tree;
		delete init_res;
		init_tree = nullptr;
	}
	if(init_tree){
		std::cout << exp_name << "init: " << init_res << std::endl;
		if(print_summary){
			std::ofstream out(exp_name + "init_summary.txt");
			init_res->print_summary(out);
		}
		if(print_scheme){
			std::ofstream out(exp_name + "init_scheme.txt");
			init_res->print_scheme("", out);
		}
		if(print_tree){
			std::ofstream out(exp_name + "init_tree.txt");
			init_res->print_tree("", out);
		}
	}else{
		std::cout << exp_name + "init finds no valid solution." << std::endl;
		return 0;
	}

	WholeSch init_sch = WholeSch(init_tree, init_res);
	init_tree = nullptr;
	init_res = nullptr;

	WholeSch min_sch = init_sch.copy();
	// bool SA_only = true;

	SAEngine* searchEngine[tries];
	for(int i = 0; i < tries; ++i){
		searchEngine[i] = new SAEngine(seed+i, i==0);
	}

	auto search = [&](const char* method, bool has_S, bool has_T){
		WholeSch cur_sch;
		int SA_type = has_S?(has_T?0:1):2;
		// SA
		std::thread* thr[tries];
		WholeSch try_sch[tries];
		for(int i = 0; i < tries; ++i){
			try_sch[i] = init_sch.copy();
			thr[i] = new std::thread(&SAEngine::SA_search, searchEngine[i], std::ref(try_sch[i]), std::ref(c), 2, SA_type);
		}
		for(int i = 0; i < tries; ++i){
			thr[i]->join();
			delete thr[i];
			if(i != 0)
				searchEngine[i]->flushBuf();
			cur_sch.min(try_sch[i]);
		}
		if(cur_sch){
			std::cout << exp_name << method << ": " << cur_sch.sch << std::endl;
			if(print_summary){
				std::ofstream out(exp_name + method + "_summary.txt");
				cur_sch.sch->print_summary(out);
			}
			if(print_scheme){
				std::ofstream out(exp_name + method + "_scheme.txt");
				cur_sch.sch->print_scheme("", out);
			}
			if(print_tree){
				std::ofstream out(exp_name + method + "_tree.txt");
				cur_sch.sch->print_tree("", out);
			}

#ifndef NOT_GEN_IR
			if(gen_IR){
				auto IR = cur_sch.sch->IR_gen();
				Json::StyledWriter swriter;
				std::string curIRName = exp_name + method + "_IR.json";
				std::ofstream IRfile(curIRName);
				IRfile << swriter.write(IR);
				IRfile.close();
			}
#endif
			min_sch.min(cur_sch);
		}else{
			std::cout << method << " finds no valid solution." << std::endl;
		}
	};

	// LP
	search("LP", true, false);

	//LS
	search("LS", false, true);

	//LSP
	//search("LSP", true, true);

	auto our_search = [&](const char* method, const WholeSch& init_sch) -> WholeSch {
		if(!init_sch){
			std::cout << method << " has no starting point!" << std::endl;
			return init_sch;
		}

		WholeSch SA_sch;

		std::thread* thr[tries];
		WholeSch try_sch[tries];
		for(int i = 0; i < tries; ++i){
			try_sch[i] = init_sch.copy();
			thr[i] = new std::thread(&SAEngine::SA_search, searchEngine[i], std::ref(try_sch[i]), std::ref(c), 0, 0);
		}
		for(int i = 0; i < tries; ++i){
			thr[i]->join();
			delete thr[i];
			if(i != 0)
				searchEngine[i]->flushBuf();
			SA_sch.min(try_sch[i]);
		}
		if(SA_sch){
			std::cout << exp_name << method << ": " << SA_sch.sch << std::endl;
			if(print_summary){
				std::ofstream out(exp_name + method + "_summary.txt");
				SA_sch.sch->print_summary(out);
			}
			if(print_scheme){
				std::ofstream out(exp_name + method + "_scheme.txt");
				SA_sch.sch->print_scheme("", out);
			}
			if(print_tree){
				std::ofstream out(exp_name + method + "_tree.txt");
				SA_sch.sch->print_tree("", out);
			}

#ifndef NOT_GEN_IR
			if(gen_IR){
				auto IR = SA_sch.sch->IR_gen();
				Json::StyledWriter swriter;
				std::string curIRName = exp_name + method + "_IR.json";
				std::ofstream IRfile(curIRName);
				IRfile << swriter.write(IR);
				IRfile.close();
			}
#endif
		}else{
			std::cout << method << " finds no valid solution." << std::endl;
		}
		return SA_sch;
	};

	our_search("SET", init_sch).del();
	//our_search("SET-min", min_sch).del();

	init_sch.del();
	min_sch.del();

	for(int i=0; i<tries; ++i){
		delete searchEngine[i];
	}

	delete cMapper;
	delete core;

	return 0;
}

static void init_core(const std::string& core_type, Core*& core, CoreMapper*& cMapper){
	Core::numMac_t LR_mac_num = 64;
	energy_t LR_mac_cost = 0.0873; //IEEE FP16

	if(core_type == "polar"){
		PolarCore::PESetting pPE(8,8,0.018);
		PolarCore::Bus pBus(4,4,0.018,16);

		PolarCore::Buffers pBuf;

		pBuf.al1.Size = 8 KB; //2 64bit-IO 4KB 1-port SBRAM
		pBuf.ol1.Size = 2 KB; //2 16bit-IO 1KB 2-port REGF
		pBuf.wl1.Size = 4 KB; //2 64bit-IO 2KB 1-port SBRAM
		pBuf.ol2.Size = 28 KB; // 2 128bit-IO 14KB 1-port MMBSRAM (concat)
		pBuf.wl2.Size = 0;//256 KB;
		pBuf.ul3.Size = 1024 KB; //16 64-bit IO 64KB 1-port MBSRAM
		pBuf.al2.Size = 0;

		pBuf.al1.RCost = 0.0485625 * 8;
		pBuf.al1.WCost = 0.0411625 * 8;
		pBuf.wl1.RCost = 0.0381625 * 8; // emlpoy 2 4KB
		pBuf.wl1.WCost = 0.0308875 * 8;

		pBuf.ol1.RCost = 0.0802 * 8;
		pBuf.ol1.WCost = 0.0709 * 8;
		pBuf.ol2.RCost = 0.07648125 * 8;
		pBuf.ol2.WCost = 0.0989875 * 8;
		pBuf.ul3.RCost = 0.1317125 * 8;
		pBuf.ul3.WCost = 0.234025 * 8;

		pBuf.al2.RCost = pBuf.al2.WCost = 0;
		pBuf.wl2.RCost = pBuf.wl2.WCost = 0;

		PolarCore* p_core = new PolarCore(pPE, LR_mac_num, LR_mac_cost, pBus, pBuf);
		core = p_core;
		cMapper = new PolarMapper(*p_core);
	}else if(core_type == "eyeriss"){
		EyerissCore::PESetting s2(32, 32, 0.018);
		EyerissCore::Bus ibus(0.018, 64);
		EyerissCore::Bus wbus(0.018, 64);
		EyerissCore::Bus pbus(0.018, 64); // ifmap RC, weight RCK, psum RK
		EyerissCore::Buses eBus{ibus, wbus, pbus};

		EyerissCore::Buffers eBuf;

		eBuf.al1.Size = 32;
		eBuf.pl1.Size = 1;
		eBuf.wl1.Size = 128;
		eBuf.ul2.Size = 1024 KB;

		eBuf.al1.RCost = 0.0509 * 8; //8bit IO single port
		eBuf.al1.WCost = 0.0506 * 8;//0.045;
		eBuf.wl1.RCost = 0.0545 * 8; //Using 2 banks of 64
		eBuf.wl1.WCost = 0.054 * 8;//0.090;
		eBuf.pl1.RCost = eBuf.pl1.WCost = 0.0;
		eBuf.ul2.RCost = 0.1317125 * 8;
		eBuf.ul2.WCost = 0.234025 * 8;

		EyerissCore* e_core = new EyerissCore(s2, LR_mac_num, LR_mac_cost, eBus, eBuf);
		core = e_core;
		cMapper = new EyerissMapper(*e_core);
	}else{
		throw std::invalid_argument("Core type \"" + core_type + "\" not recognized!");
	}
}
