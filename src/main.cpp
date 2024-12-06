#include "cluster.h"
#include "layerengine.h"
#include "ltreenode.h"
#include "noc.h"
#include "schnode.h"
#include "util.h"
#include "nns/nns.h"

#include "sa.h"			// Library for SA

#include "json/json.h"	// Json::StyledWriter
#include <cassert>		// assert
#include <cmath>		// std::pow
#include <cstdlib>		// std::srand
#include <ctime>		// std::time
#include <fstream>		// std::ifstream, std::ofstream
#include <functional>	// std::ref
#include <iostream>		// std::cin, std::cout, std::endl
#include <string>		// std::string
#include <thread>		// std::thread

#define KB *1024


int main(int argc, char** argv){
#ifdef DEBUG
	unsigned seed = 213;
#else
	unsigned seed = std::time(nullptr);
#endif
	std::srand(seed);
	std::cout.precision(4);

	NoC::hop_cost=0.7 * 8;
	NoC::DRAM_acc_cost=7.5 * 8;
	// NoC::DRAM_bw=64;
	// NoC::NoC_bw=32;
	Core::numMac_t LR_mac_num = 64;
	energy_t LR_mac_cost=0.0873; //IEEE FP16

	PolarCore::Buffers pBuf;
	PolarCore::PESetting pPE(8,8,0.018);
	PolarCore::Bus pBus(4,4,0.018,16);
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

	PolarCore core(pPE, LR_mac_num, LR_mac_cost, pBus, pBuf);
	PolarMapper mapper(core);

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

	EyerissCore core2(s2, LR_mac_num, LR_mac_cost, eBus, eBuf);
	EyerissMapper mapper2(core2);
	int mm, nn, xx, yy, ss, bb, rr, ff;
	bw_t bw;
	std::string IR_name = "";

	/*
	//std::ifstream in("params.in");
	if(!(std::cin>>mm>>nn>>bb>>xx>>yy>>ss>>rr>>ff>>bw)){
		assert(false);
		std::cout<<"Warning: No input file detected, use default settings:"<<std::endl;
		mm=0;nn=2;bb=64;xx=8;yy=8;ss=4;rr=100;ff=1;bw=24;
		std::cout<<mm<<' '<<nn<<' '<<bb<<' '<<xx<<' '<<yy<<' '<<ss<<' '<<rr<<' '<<ff<<' '<<bw<<std::endl;
	}else{
		//in.close();
	}*/

	std::cin >> mm >> nn >> bb >> xx >> yy >> ss >> rr >> ff >> bw;
	/*
		mm: 0: polarcore 1: eyeriss core
		nn: network options
		bb: batch size
		xx*yy: core number
		ss: zig-zag width
		rr: number of iterations / number of layers
		ff: evaluation function
		bw: bandwidth (in ??) 
	*/
	// mm=0;nn=2;bb=64;xx=8;yy=8;ss=4;rr=100;ff=1;bw=24;
	// 0 13 2 4 4 2 50 1 24

	if(argc > 1) IR_name = std::string("results/json/") + argv[1];

	std::cout << "Seed: " << seed << std::endl;

	CoreMapper* cMapper;
	if(mm == 0){
		cMapper = &mapper;
	}else{
		cMapper = &mapper2;
	}

	// Part into 6 parts:
	cMapper->set_part(6);

	// Part into <=10KB parts:
	// Note: do not use this !!!
	//  1. will enconter large primes (e.g. 20071)
	//  2. the part size will overflow (>65536)
	// cMapper->set_part(-10 KB);

	StdLayerEngine engine(cMapper);
	SchNode::layerMapper = &engine;

	std::string net_name;
	switch (nn) {
	case 0:
		network = &darknet19;
		net_name="darknet19";
		break;
	case 1:
		network = &vgg19;
		net_name="vgg";
		break;
	case 2:
		network = &resnet50;
		net_name="resnet";
		break;
	case 3:
		network = &googlenet;
		net_name="goog";
		break;
	case 4:
		network = &resnet101;
		net_name="resnet101";
		break;
	case 5:
		network = &densenet;
		net_name="densenet";
		break;
	case 6:
		network = &inception_resnet_v1;
		net_name="ires";
		break;
	case 7:
		network = &gnmt;
		net_name="gnmt";
		break;
	case 8:
		network = &lstm;
		net_name="lstm";
		break;
	case 9:
		network = &zfnet;
		net_name="zfnet";
		break;
	case 10:
		network = &transformer;
		net_name="trans";
		break;
	case 11:
		network = &transformer_cell;
		net_name="trans_cell";
		break;
	case 12:
		network = &PNASNet;
		net_name="pnas";
		break;
	case 13:
		network = &inception_resnet_block;
		net_name="ires_block";
		break;
	default:
		assert(false);
		break;
	}

	Cluster::xlen = xx;
	Cluster::ylen = yy;
	len_t tot_batch = bb;
	SchNode::tot_batch = tot_batch;

	Cluster::stride = ss;
	Cluster::min_util = 0.75;

	//SchNode::tot_batch = 64;
	lid_t num_layer = network->len();
	SAEngine::nrounds = rr * num_layer;
	network->set_utime(*cMapper);
	Cluster c(0, Cluster::xlen * Cluster::ylen);


	// Set NoC properties:
	// Only use unicast in NoC
	NoC::unicast_only = true;
	{
		std::vector<std::vector<pos_t>> dram_lists(4);
		assert(Cluster::ylen >= 2);
		mlen_t h_len = Cluster::ylen / 2;
		dram_lists[0].resize(h_len);
		dram_lists[1].resize(h_len);
		dram_lists[2].resize(h_len);
		dram_lists[3].resize(h_len);
		mlen_t x_max = Cluster::xlen-1;
		mlen_t y_max = Cluster::ylen-1;
		for(mlen_t y=0; y<h_len; ++y){
			dram_lists[0][y] = {0, y};
			dram_lists[1][y] = {0, static_cast<mlen_t>(y_max-y)};
			dram_lists[2][y] = {x_max, y};
			dram_lists[3][y] = {x_max, static_cast<mlen_t>(y_max-y)};
		}
		NoC::set_DRAMs(dram_lists);

		// The way to set group interleaving (default is full-interleaving):
		std::vector<std::vector<std::pair<didx_t, didx_t>>> port_groups(4);
		for(mlen_t i=0; i<4; ++i){
			port_groups[i].resize(h_len);
			for(mlen_t j=0; j<h_len; ++j){
				port_groups[i][j] = {i, j};
			}
		}
		NoC::set_interleave(port_groups, 2);
	}

	switch (ff){
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
			if(ff > 0){
				cost_func = [=](energy_t e, cycle_t t)->cost_t{return std::pow(e,ff)*t;};
			}else{
				cost_func = [=](energy_t e, cycle_t t)->cost_t{return e*std::pow(t,-ff);};
			}
	}

	std::cout << "Mapper " << ((cMapper == &mapper)?"polar":"eyeriss");
	std::cout << " Network " << net_name;
	std::cout << " Mesh " << static_cast<int>(Cluster::xlen) << '*' << static_cast<int>(Cluster::ylen);
	std::cout << " Batch " << tot_batch << std::endl;

	// TOPS
	double tops = cMapper->core().mac_num * Cluster::xlen * Cluster::ylen * 2;
	tops /= 1024;

	// 0.5 GB/TOPS
	NoC::DRAM_bw = 0.5 * (tops/4);
	NoC::NoC_bw = bw;

	// LS
	LTreeNode* LS_tree = nullptr;
	SchNode* LS_res = nullptr;
	for(len_t lb = tot_batch; lb>0; lb/=2){
		LS_tree = new LTreeNode(Bitset(), tot_batch, nullptr, LTreeNode::NodeType::T);
		for(lid_t i = 0; i < num_layer; ++i){
			(void)new LTreeNode(i, lb, LS_tree);
		}
		LS_tree->init_root();
		LS_res = SchNode::newNode(LS_tree, c, nullptr);
		if(LS_res->is_valid()){
			LS_tree->confirm();
			break;
		}
		delete LS_tree;
		delete LS_res;
		LS_tree = nullptr;
	}
	if(LS_tree){
		std::cout << "LS: " << LS_res << std::endl;
		std::cout << "Struct:" << std::endl;
		LS_res->print_struct("\t");
		std::ofstream out("tree_LS.txt");
		LS_res->print_tree("", out);
	}else{
		std::cout << "LS finds no valid solution." << std::endl;
		return 0;
	}

	WholeSch LS_sch = WholeSch(LS_tree, LS_res);
	LS_tree = nullptr;
	LS_res = nullptr;

	WholeSch min_sch = LS_sch.copy();
	// bool SA_only = true;
	constexpr int tries = 4;

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
			try_sch[i] = LS_sch.copy();
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
			std::cout << method << "-SA: " << cur_sch.sch << std::endl;
			std::cout << "Struct:" << std::endl;
			cur_sch.sch->print_struct("\t");
			std::ofstream out(std::string("tree_") + method + "-SA.txt");
			cur_sch.sch->print_tree("", out);
			if(!IR_name.empty()){
				auto IR = cur_sch.sch->IR_gen();
				Json::StyledWriter swriter;
				std::string curIRName = IR_name + '_' + method + "-SA.json";
				std::ofstream IRfile(curIRName);
				IRfile << swriter.write(IR);
				IRfile.close();
			}
			min_sch.min(cur_sch);
		}else{
			std::cout << method << " finds no valid solution." << std::endl;
		}
	};

	// LP
	search("LP", true, false);

	std::cerr << "LP" << std::endl;

	//LS-opt
	search("LS-opt", false, true);

	std::cerr << "LS-opt" << std::endl;

	//LSP
	//search("LSP", true, true);

	auto our_search = [&](const char* method, const WholeSch& init_sch) -> WholeSch {
		if(!init_sch){
			std::cout << method << " has no starting point!" << std::endl;
			return init_sch;
		}

		WholeSch SA_sch;

		//nrounds = rr;
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
			std::cout << method << ": " << SA_sch.sch << std::endl;
			std::cout << "Struct: " << std::endl;
			SA_sch.sch->print_struct("\t");
			std::ofstream out(std::string("tree_") + method + ".txt");
			SA_sch.sch->print_tree("", out);
			if(!IR_name.empty()){
				auto IR = SA_sch.sch->IR_gen();
				Json::StyledWriter swriter;
				std::string curIRName = IR_name + '_' + method + ".json";
				std::ofstream IRfile(curIRName);
				IRfile << swriter.write(IR);
				IRfile.close();
			}
		}else{
			std::cout << method << " finds no valid solution." << std::endl;
		}
		return SA_sch;
	};

	our_search("SA-LS", LS_sch).del();
	//our_search("SA-min", min_sch).del();

	LS_sch.del();
	min_sch.del();

	for(int i=0; i<tries; ++i){
		delete searchEngine[i];
	}

	return 0;
}
