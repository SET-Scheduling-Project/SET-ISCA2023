#include "sa.h"

#include <algorithm>	// std::swap
#include <cassert>		// assert
#include <cmath>		// std::exp
#include <cstdint>		// std::uint64_t
#include <cstring>		// std::size_t, (std::memset)
#include <ctime>		// std::time
#include <iostream>		// std::cout, std::flush, std::endl
#include <map>			// std::map
#include <stdexcept>	// std::invalid_argument

#include "bitset.h"		// Bitset

/*
#include <chrono>		// std::chrono
#include <mutex>		// std::mutex, std::unique_lock
#include <thread>		// std::thread, std::this_thread
*/

WholeSch::WholeSch(): tree(nullptr), sch(nullptr){}

WholeSch::WholeSch(LTreeNode* _tree, SchNode* _sch): tree(_tree), sch(_sch){}

WholeSch::operator bool() const{
	return tree;
}

WholeSch WholeSch::copy() const{
	if(!tree) return {nullptr, nullptr};
	LTreeNode* new_tree = tree->copy();
	SchNode* new_sch = sch->copy();
	return WholeSch(new_tree, new_sch);
}
void WholeSch::del(){
	if(tree){
		delete tree;
		delete sch;
		tree = nullptr;
		sch = nullptr;
	}
}
void WholeSch::min(WholeSch& w_sch){
	if(!tree){
		tree = w_sch.tree;
		sch = w_sch.sch;
	}else if(sch->get_cost().cost() > w_sch.sch->get_cost().cost()){
		del();
		tree = w_sch.tree;
		sch = w_sch.sch;
	}else{
		w_sch.del();
	}
	w_sch.tree = nullptr;
	w_sch.sch = nullptr;
}

static std::size_t find(const LTreeNode::node_vec& vec, LTreeNode* node){
	for(std::size_t i=0; i<vec.size(); ++i){
		if(vec[i] == node) return i;
	}
	assert(false);
	return vec.size();
}


int SAEngine::nrounds;

void SAEngine::halv_bat(LTreeNode* node){
	if(!node->children.empty() && node->children.front()->num_batch == node->num_batch){
		for(auto x : node->children){
			halv_bat(x);
		}
	}
	node->num_batch /= 2;
}

void SAEngine::flat_bat(LTreeNode* node, len_t n_batch){
	if(node->num_batch <= n_batch) return;
	for(auto x : node->children){
		flat_bat(x, n_batch);
	}
	node->num_batch = n_batch;
}

int SAEngine::randInt(int to){
	return std::uniform_int_distribution(0, to-1)(generator);
}

bool SAEngine::withProb(double prob){
	return std::uniform_real_distribution(0.0, 1.0)(generator) < prob;
}

SAEngine::SAEngine(std::uint32_t seed, bool directCout):generator(seed), out(directCout ? std::cout : strStream){
	strStream.precision(4);
}

void SAEngine::flushBuf(){
	std::cout << strStream.str() << std::flush;
	strStream.clear();
	strStream.str("");
}

// sa_type: 0 -> arbitrary. 1 -> only s under top t. 2 -> only t under top t.
LTreeNode* SAEngine::sa_change(LTreeNode* root, bool* valid_op, lid_t max_depth, int sa_type, int* op_type){
	root = root->copy();
	lid_t lnum = root->layers().count();
	if(max_depth == 0) max_depth = lnum;
	if(root->height > max_depth){
		throw std::invalid_argument("The root of SA is deeper than max_depth!");
	}

	int prob[NUM_OP]={10,10,20,20,20,20,40};
	for(int i=1; i<NUM_OP; ++i){
		prob[i] += prob[i-1];
	}
	//bool x[NUM_OP+1];
	//std::memset(x,0,NUM_OP);
	int t=NUM_OP;
	std::uint64_t print_tries = 30;
	cur_tries = 0;
	bool ok;
	// For convenience, we research the whole seg now.
	assert(root->get_type() == LTreeNode::NodeType::T);
	do{
		//x[t] = true;
		lid_t l = randInt(lnum);
		LTreeNode* lnode=root;
		lid_t depth = 0;
		while(lnode->t != LTreeNode::NodeType::L){
			for(auto child: lnode->children){
				if(child->layers().contains(l)){
					lnode = child;
					++depth;
					break;
				}
			}
		}
		assert(depth > 0);
		ok=false;
		do{
			int pr = randInt(prob[NUM_OP-1]);
			t=0;
			while(pr >= prob[t]) ++t;
		}while(!valid_op[t]);
		//if(x[t]) continue;
		switch (t) {
		case 0:{
			// Change with front.
			LTreeNode* front = lnode->parent;
			LTreeNode* c = lnode;
			while (front && front->children.front() == c) {
				c = front;
				front = front->parent;
			}
			if(!front) break;
			LTreeNode* lcl = c;
			auto k = find(front->children, c);
			// std::cout << k << " is " << std::endl;
			c = front->children[k-1];
			LTreeNode* lcc = c;
			while (!c->children.empty()) {
				c = c->children.back();
			}
			lid_t x = c->layer_set.first();
			if(network->getNode(l).getPrevs().contains(x)) break;
			// Found valid front, change!
			front->stage.clear();
			// Reset path to lcl/lcc.
			while(lcl!=lnode){
				lcl->layer_set.reset(l);
				lcl->layer_set.set(x);
				lcl->stage.clear();
				for (auto z:lcl->children) {
					if(z->layer_set.contains(l)){
						lcl = z;
						break;
					}
				}
			}
			while(lcc!=c){
				lcc->layer_set.reset(x);
				lcc->layer_set.set(l);
				lcc->stage.clear();
				lcc = lcc->children.back();
			}
			auto i = find(c->parent->children, c);
			auto j = find(lnode->parent->children, lnode);
			c->parent->children[i] = lnode;
			lnode->parent->children[j] = c;
			std::swap(lnode->parent,c->parent);
			std::swap(lnode->num_batch,c->num_batch);
			// std::cout << (int)x << ' ' << (int)l <<std::endl;
			// Now we'll reset the whole seg.
			if(front == root){
				front->children[k]->isNewNode = true;
				front->children[k-1]->isNewNode = true;
			}else{
				while(front->parent->parent) front = front->parent;
				front->isNewNode = true;
			}
			ok=true;
		}break;
		case 1:{
			// Change with back.
			LTreeNode* back = lnode->parent;
			LTreeNode* c = lnode;
			while (back && back->children.back() == c) {
				c = back;
				back = back->parent;
			}
			if(!back) break;
			LTreeNode* lcl = c;
			auto k = find(back->children, c);
			// std::cout << k << " is " << std::endl;
			c = back->children[k+1];
			LTreeNode* lcc = c;
			while (!c->children.empty()) {
				c = c->children.front();
			}
			lid_t x = c->layer_set.first();
			if(network->getNode(x).getPrevs().contains(l)) break;
			// Found valid back, change!
			back->stage.clear();
			// Reset path to lcl/lcc.
			while(lcl!=lnode){
				lcl->layer_set.reset(l);
				lcl->layer_set.set(x);
				lcl->stage.clear();
				for (auto z:lcl->children) {
					if(z->layer_set.contains(l)){
						lcl = z;
						break;
					}
				}
			}
			while(lcc!=c){
				lcc->layer_set.reset(x);
				lcc->layer_set.set(l);
				lcc->stage.clear();
				lcc = lcc->children.front();
			}
			auto i = find(c->parent->children, c);
			auto j = find(lnode->parent->children, lnode);
			c->parent->children[i] = lnode;
			lnode->parent->children[j] = c;
			std::swap(lnode->parent,c->parent);
			std::swap(lnode->num_batch,c->num_batch);
			//std::cout << (int)x << ' ' << (int)l <<std::endl;
			// Now we'll reset the whole seg.
			if(back == root){
				back->children[k]->isNewNode = true;
				back->children[k+1]->isNewNode = true;
			}else{
				while(back->parent->parent) back = back->parent;
				back->isNewNode = true;
			}
			ok=true;
		}break;
		case 2:{
			// Delete parent, merge to grandma.
			LTreeNode* par = lnode->parent;
			if(par == nullptr) break;
			LTreeNode* grandma = par->parent;
			if(grandma == nullptr) break;
			auto i = find(grandma->children, par);
			grandma->children.erase(grandma->children.begin()+i);
			grandma->children.insert(grandma->children.begin()+i, par->children.begin(), par->children.end());
			grandma->stage.clear();
			for(auto x : par->children){
				x->parent = grandma;
				x->t = LTreeNode::NodeType::L;
				x->num_batch = par->num_batch;
			}
			// Now we'll reset the whole seg.
			if(grandma == root){
				for(auto x : par->children){
					x->isNewNode = true;
				}
			}else{
				while(grandma->parent->parent) grandma = grandma->parent;
				grandma->isNewNode = true;
			}
			par->children.clear();
			delete par;
			ok=true;
		}break;
		case 3:{
			// Add new parent, select a range of childs.
			LTreeNode* par = lnode->parent;
			if(par == nullptr || par->children.size() <= 2) break;
			auto i = find(par->children, lnode);
			auto x = par->children.size()-1;
			std::size_t j;
			do{
				j = randInt(x);
				if(j>=i) ++j;
			}while(i*j==0&&i+j==x);
			x = MIN(i,j);
			j = MAX(i,j)+1;
			i = x;
			// Check depth limit
			if(par->height + depth > max_depth){
				bool p = false;
				for(std::size_t x=i; x<j; ++x){
					if(par->children[x]->height + depth >= max_depth){
						p=true;
						break;
					}
				}
				if(p) break;
			}
			par->stage.clear();
			LTreeNode* new_par;
			bool T_under_T = false;
			if((par->parent == nullptr) && (par->t == LTreeNode::NodeType::T)){
				switch(sa_type){
					case 0: T_under_T = withProb(0.5); break;
					case 1: break;
					case 2: T_under_T = true; break;
					default: assert(false);
				}
			}
			new_par=new LTreeNode(Bitset(),lnode->num_batch,nullptr, T_under_T ? LTreeNode::NodeType::T : LTreeNode::NodeType::L);
			new_par->children.insert(new_par->children.begin(), par->children.begin()+i, par->children.begin()+j);
			new_par->parent = par;
			par->children.erase(par->children.begin()+i, par->children.begin()+j);
			par->children.insert(par->children.begin()+i, new_par);
			for(auto x : new_par->children){
				x->parent = new_par;
				if(T_under_T) x->t = LTreeNode::NodeType::L;
				else if(par->t == LTreeNode::NodeType::T) flat_bat(x);
			}
			// Now we'll reset the whole seg.
			while(new_par->parent->parent) new_par = new_par->parent;
			new_par->isNewNode = true;
			ok=true;
		}break;
		case 4:{
			// Put batch down
			LTreeNode* cur = lnode->parent;
			int d = randInt(depth);
			while(d-->0){
				cur = cur->parent;
			}
			if(cur->children.front()->num_batch == cur->num_batch){
				break;
			}
			if(cur == root && !withProb(0.1 * depth)){
				break;
			}
			for(auto x:cur->children){
				x->num_batch *= 2;
			}
			// Now we'll reset the whole seg.
			if(cur == root){
				cur->isNewNode = true;
			}else{
				while(cur->parent->parent) cur = cur->parent;
				cur->isNewNode = true;
			}
			ok=true;
		}break;
		case 5:{
			// Put batch up
			LTreeNode* cur = lnode->parent;
			int d = randInt(depth);
			while(d-->0){
				cur = cur->parent;
			}
			if(cur->children.front()->num_batch == 1){
				break;
			}
			if(cur == root && !withProb(0.1 * depth)){
				break;
			}
			for(auto x:cur->children){
				halv_bat(x);
			}
			// Now we'll reset the whole seg.
			if(cur == root){
				cur->isNewNode = true;
			}else{
				while(cur->parent->parent) cur = cur->parent;
				cur->isNewNode = true;
			}
			ok=true;
		}break;
		case 6:{
			// Put lnode into the cut before/after it (or out of its parent).
			LTreeNode* par = lnode->parent;
			if(par->children.size() <= 2) break;
			auto node_pos = find(par->children, lnode);
			bool put_before = (randInt(2) == 0);
			if(put_before?(node_pos > 0):(node_pos < par->children.size()-1)){
				auto next_pos = node_pos + (put_before ? -1 : 1);
				LTreeNode* cut = par->children[next_pos];
				if(cut->t == LTreeNode::NodeType::L) break;
				// Put lnode under cut.
				par->stage.clear();
				cut->stage.clear();
				par->children.erase(par->children.begin()+node_pos);
				if(put_before){
					cut->children.push_back(lnode);
				}else{
					cut->children.insert(cut->children.begin(), lnode);
				}
				lnode->parent = cut;

				lnode->num_batch = cut->get_bgrp_size();
				cut->layer_set.set(l);
				// Now we'll reset the whole seg.
				assert(cut != root);
				while(cut->parent->parent) cut = cut->parent;
				cut->isNewNode = true;
			}else{
				LTreeNode* grandma = par->parent;
				if(grandma == nullptr) break;
				// Put lnode under par->parent.
				auto par_pos = find(grandma->children, par);
				auto insert_pos = par_pos + (put_before ? 0 : 1);
				par->stage.clear();
				grandma->stage.clear();
				par->children.erase(par->children.begin()+node_pos);
				grandma->children.insert(grandma->children.begin()+insert_pos, lnode);
				lnode->parent = grandma;

				lnode->num_batch = grandma->get_bgrp_size();
				par->layer_set.reset(l);
				// Now we'll reset the whole seg.
				if(grandma == root){
					lnode->isNewNode = true;
					par->isNewNode = true;
				}else{
					while(grandma->parent->parent) grandma = grandma->parent;
					grandma->isNewNode = true;
				}
			}
			ok=true;
		}break;
		default:
			break;
		}
		++cur_tries;
		++num_tries;
		if(cur_tries >= print_tries){
			out << "[Warning] After " << cur_tries << " tries." << std::endl;
			print_tries *= 2;
		}
	}while(!ok);
	//std::cout << t << std::endl;
	if(op_type) *op_type = t;
	root->init_root();
	return root;
}

bool SAEngine::sa_accept(cost_t cur_cost, cost_t new_cost, int round){
	if(new_cost <= cur_cost) return true;
	/*
	 * T(x) = a+c/(b+x)
	 * a + c/b = 0.1
	 * a + c/(b+0.5) = 0.01
	 * a + c/(b+1) = 0
	 * a = -1 / 80
	 * b = 0.01*0.5/(0.1*0.5-0.01) = 0.125
	 * c = 9 / 640
	 * T(x) = 1/10 * (1-x)/(1+8x)
	 */
	double x = round;
	x /= nrounds;
	// Since only 1/100 are good, multiply T by 0.7:
	double T = 0.07 * (1-x)/(1+8*x);
	double prob = std::exp(-((new_cost - cur_cost)/cur_cost)/T);
	// std::cout << x << ' ' << T << ' ' << prob << std::endl;
	return withProb(prob);
}

/*
static std::mutex m;
void SAEngine::ping_func(volatile bool& stop) const{
	while(true){
		for(int i=0; i<120; ++i){
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if(stop) return;
		}
		std::unique_lock<std::mutex> l(m);
		std::cout << "[Ping] " << cur_round << ' ' << cur_tries << ' ' << num_tries << std::endl;
		l.unlock();
	}
}
*/

void SAEngine::SA_search(WholeSch& w_sch, const Cluster& c, lid_t max_depth, int sa_type){
	time_t start_time = std::time(nullptr);
	int nvalid = 0, naccept = 0;
	LTreeNode*& min_node = w_sch.tree;
	SchNode*& min_res = w_sch.sch;
	LTreeNode* cur_node = w_sch.tree;
	SchNode* cur_res = w_sch.sch;
	int op_type;
	std::map<int, int> accept_num;
	std::map<int, int> valid_num;
	for(int i=0;i<NUM_OP;++i){
		accept_num[i] = 0;
		valid_num[i] = 0;
	}
	bool using_best = false;

	bool valid_op[NUM_OP];
	for(int i=0; i<NUM_OP; ++i) valid_op[i] = true;
	if(network->is_chain()) valid_op[0] = valid_op[1] = false;
	if(cur_node->get_tot_batch() == 1) valid_op[4] = valid_op[5] = false;
	num_tries = 0;
	cur_tries = 0;
	int print_intv = nrounds/30;
	cur_round=0;
	// bool stop_ping = false;
	// std::thread ping(ping_func, ref(stop_ping));
	for(; cur_round<nrounds; ++cur_round){
		if((cur_round+1) % print_intv == 0){
			// std::unique_lock<std::mutex> l(m);
			out << cur_round << ' ' << cur_res->get_cost().cost() << ' ' << (num_tries * 1.0) /print_intv << std::endl;
			// l.unlock();
			num_tries = 0;
		}
		if(cur_round >= 0.90*nrounds && !using_best){
			using_best = true;
			if(cur_node != min_node){
				// std::unique_lock<std::mutex> l(m);
				out << "Switch to best solution." << std::endl;
				// l.unlock();
				delete cur_node;
				delete cur_res;
				cur_node = min_node;
				cur_res = min_res;
			}
		}
		LTreeNode* new_tree = sa_change(cur_node, valid_op, max_depth, sa_type, &op_type);
		SchNode* new_res;
		if(new_tree->isNew()){
			new_res = Cut::newNode(new_tree, c, nullptr);
		}else{
			new_res = cur_res->copy();
			assert(new_tree->isModified());
			new_res->searchInc(new_tree);
		}
		if(!new_res->is_valid()){
			delete new_tree;
			delete new_res;
			continue;
		}
		new_tree->confirm();
		++nvalid;
		++valid_num[op_type];
		cost_t new_cost = new_res->get_cost().cost();
		if(new_cost < min_res->get_cost().cost()){
			if(cur_node != min_node){
				delete min_node;
				delete min_res;
			}
			min_node = new_tree;
			min_res = new_res;
		}

		if(sa_accept(cur_res->get_cost().cost(), new_cost, cur_round)){
			// std::cout << "accept!" << std::endl;
			if(cur_node != min_node){
				delete cur_node;
				delete cur_res;
			}
			cur_node = new_tree;
			cur_res = new_res;
			++naccept;
			++accept_num[op_type];
		}else{
			if(new_tree != min_node){
				delete new_tree;
				delete new_res;
			}
		}
	}
	if(cur_node != min_node){
		delete cur_node;
		delete cur_res;
	}
	time_t end_time = std::time(nullptr);
	// stop_ping = true;
	// ping.join();
	out << "Elapsed: " << end_time - start_time << "s ";
	out << "Valid: " << nvalid << " (" << (nvalid*100.0)/nrounds << "%) ";
	out << "Accept: " << naccept << " (" << (naccept*100.0)/nrounds << "%)" << std::endl;
	out << "Per OP: ";
	for(int i=0;i<NUM_OP;++i){
		if(i>0) out << ", ";
		out << accept_num[i] << '/' << valid_num[i];
	}
	out << std::endl;
}


void LP_search(lid_t num_layer, len_t tot_batch, Cluster& c, WholeSch& w_sch, bool has_S, bool has_T){
	if(!has_S && !has_T){
		throw std::invalid_argument("Either has_S or has_T must be true.");
	}
	LTreeNode*& tree_node = w_sch.tree;
	SchNode*& sch_res = w_sch.sch;
	SchNode::SchCost LP_cost;
	LTreeNode** LP_DP = new LTreeNode*[num_layer];
	SchNode* tmp = nullptr;
	SchNode* last = nullptr;
	LTreeNode* root_Node;
	for(int i=0;i<num_layer;++i) {
		LP_DP[i]=nullptr;
		LP_cost.energy = energy_inf;
		// std::cout << "\tStart " << network->getNode(i).name() << std::endl;
		for(int j=-1;j<i;++j){
			if(j != -1 && LP_DP[j] == nullptr) continue;
			for(len_t l_bat=1;l_bat<=4&&l_bat<=tot_batch;l_bat*=2){
				for(int last_T = has_S?0:1; last_T < (has_T?2:1); ++last_T){
					if(j==i-1 && has_S && has_T && last_T > 0) break;
					if(j==-1){
						// Create top T-cut.
						root_Node = new LTreeNode(Bitset(), tot_batch, nullptr, LTreeNode::NodeType::T);
					}else {
						// Use top T-cut from DP.
						root_Node = LP_DP[j]->copy();
						root_Node->reset_lset();
					}
					if(j == i-1){
						// New LNode: last layer.
						(void) new LTreeNode(i, tot_batch, root_Node);
					}else{
						LTreeNode* cur_Cut = new LTreeNode(Bitset(), tot_batch, root_Node, (last_T == 0) ? LTreeNode::NodeType::S : LTreeNode::NodeType::T);
						for(int k=j+1;k<=i;++k){
							(void) new LTreeNode(k, l_bat, cur_Cut);
						}
					}
					root_Node->init_root();
					// TODO: change to inc. search
					tmp = Cut::newNode(root_Node, c, nullptr);
					if(!tmp->is_valid()){
						delete tmp;
						delete root_Node;
						continue;
					}
					root_Node->confirm();
					if(tmp->get_cost().cost() >= LP_cost.cost()){
						delete tmp;
						delete root_Node;
						continue;
					}
					if(LP_DP[i]) delete LP_DP[i];
					LP_DP[i] = root_Node;
					LP_cost = tmp->get_cost();
					if(last) delete last;
					last = tmp;
				}
			}
		}
	}
	tree_node = LP_DP[num_layer-1];
	if(tree_node == nullptr){
		std::cout << "Warning: no scheme found in LP_search!" << std::endl;
		sch_res = nullptr;
		if(last) delete last;
	}else{
		sch_res = last;
	}
	for(int i=0; i<num_layer-1; ++i) if(LP_DP[i]) delete LP_DP[i];
	delete[] LP_DP;
	return;
}
