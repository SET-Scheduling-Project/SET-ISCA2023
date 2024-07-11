#include "json/json.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cassert>
#include <random>
#include <filesystem>
#define cmin(a,b) (a>(b)?a=(b),1:0)
#define cmax(a,b) (a<(b)?a=(b),1:0)

Json::Value input;
std::string coreid;

struct datablock{
	int no;
	int l,r;
	int size;
	enum datatype{
		weight, ifmap, ofmap
	} type;
	bool operator <(const datablock &other) const{
		return no<other.no;
	}
};
int w;
int top_batch_cut;
std::vector<datablock> data, data2, original;
int lwb;

std::string dirname;

bool cmp1(const datablock &x, const datablock &y){
	/*if((x.type == datablock::datatype::weight) != (y.type == datablock::datatype::weight)){
		return (x.type == datablock::datatype::weight) > (y.type == datablock::datatype::weight);
	}*/
	if(x.l != y.l){
		return x.l < y.l;
	}
	return x.r > y.r;
}

bool cmp2(const datablock &x, const datablock &y){
	return x.size > y.size;
}

bool cmp3(const datablock &x, const datablock &y){
	if(x.r!=y.r)return x.r < y.r;
	return x.l<y.l;
}

bool cmp4(const datablock &x, const datablock &y){
	return (long long)(x.r-x.l+1)*(x.size) > (long long)(y.r-y.l+1)*(y.size);
}

struct sol_perm{
	int wlist_left, wlist_right;
	int lower, upper;
	std::vector<int> p;
};

typedef std::vector<sol_perm> compl_sol_t;

typedef std::pair<int, std::vector<int> > Solution_t; 

struct st{
	const static int inf = 0x3f3f3f3f;
private:
	const int l,r;
	int set;
	int ret;
	st *lc, *rc;
public:
	st(int _l,int _r,int ini = 0)
		:l(_l), r(_r), set(-1), ret(ini){
		if(l == r)return;
		const int mid = l+r>>1;
		lc = new st(l,mid,ini);
		rc = new st(mid+1,r,ini);
	}
	~st(){
		if(l!=r){
			delete lc;
			delete rc;
		}
	}
	void pd(){
		if(set != -1){
			ret = set;
			set = -1;
			if(l != r){
				lc->set = ret;
				rc->set = ret;
			}
		}
	}
	void MT(){//do not call on leaves
		lc->pd();
		rc->pd();
		ret = std::max(lc->ret,rc->ret);
	}
	int query(int _l, int _r){
		if(r<_l || l>_r){
			return 0;
		}
		pd();
		if(_l <= l && r <= _r){
			return ret;
		}
		return std::max(lc->query(_l, _r), rc->query(_l, _r));
	}
	void paint(int _l, int _r, int val){
		pd();
		if(_l <= l && r <= _r){
			set = val;
			return;
		}
		const int mid = l+r>>1;
		if(_l<=mid) lc->paint(_l, _r, val);
		if(_r>mid) rc->paint(_l, _r, val);
		MT();
	}
	void checkmin(int _l, int _r, int val){
		if(ret<=val)return;
		if(_l <= l && r <= _r){
			if(l == r){
				cmin(ret, val);
				return;
			}
			lc->checkmin(_l,_r,val);
			rc->checkmin(_l,_r,val);
			MT();
			return;
		}
		const int mid = l+r>>1;
		if(_l<=mid) lc->checkmin(_l,_r,val);
		if(_r>mid) rc->checkmin(_l,_r,val);
		MT();
	}
};

Solution_t fcfs_sol;
std::vector<int> bottleneck;
Solution_t greedy(std::vector<datablock> &data = ::data, int left=1, int right=w, int last = 0, int* retlast = NULL, int* lower = NULL, int* upper = NULL){
	st* t = new st(left,right);
	st* tmin = new st(left,right,st::inf);
	std::vector<int> sol;
	sol.resize(data.size());
	for(auto block:data){
		int max = t->query(block.l,block.r);
		tmin->checkmin(block.l,block.r,max);
		t->paint(block.l,block.r,max+block.size);
		if(block.no<sol.size()){
			sol[block.no] = max;
		}
	}
	int ans=0;
	for(int i=left;i<=right;++i){
		int tmp = t->query(i,i) - tmin->query(i,i) + (i==left?last:0);
		cmax(ans,tmp);
	}
	bottleneck.resize(0);
	for(int i=left;i<=right;++i){
		int tmp = t->query(i,i) - tmin->query(i,i) + (i==left?last:0);
		if(tmp == ans){
			bottleneck.push_back(i);
		}
	}
	if(lower){
		*lower = tmin->query(left,left);
	}
	if(upper){
		*upper = t->query(right,right);
	}
	if(retlast){
		*retlast = t->query(right,right) - tmin->query(right,right);
	}
	delete t;
	delete tmin;
	return std::make_pair(ans,sol);
}

Solution_t greedy_opt1(const std::vector<int> &p){
	st* t = new st(1,w);
	st* tmin = new st(1,w,st::inf);
	std::vector<int> sol;
	int n = original.size();
	sol.resize(n);
	int floor = 50000000;
	for(int part = 0; part<top_batch_cut; ++part){
		int minl = w+1, maxr = 0;
		int lower = 0x3f3f3f3f;
		int upper = floor;
		for(auto a:p){
			auto block = original[a+n/top_batch_cut*part];
			int max = std::max(floor, t->query(block.l,block.r));
			tmin->checkmin(block.l,block.r,max);
			t->paint(block.l,block.r,max+block.size);
			sol[block.no] = max;
			if(cmin(minl, block.l)){
				lower = max;
			}
			if(cmax(maxr, block.r)){
				upper = max+block.size;
			}
		}
		floor += upper - lower;
	}
	
	int ans=0;
	for(int i=1;i<=w;++i){
		int tmp = t->query(i,i) - tmin->query(i,i);
		cmax(ans,tmp);
	}
	delete t;
	delete tmin;
	return std::make_pair(ans,sol);
}

Solution_t greedy_opt2(const compl_sol_t &sol){
	st* t = new st(1,w);
	st* tmin = new st(1,w,st::inf);
	std::vector<int> ret;
	ret.resize(original.size());
	int floor = 50000000;
	for(int _perm=0; _perm<sol.size(); ++_perm){
		auto perm = sol[_perm];
		for(int i=0; i<perm.p.size(); ++i){
			auto block = original[perm.p[i]];
			int max = std::max(floor, t->query(block.l,block.r));
			tmin->checkmin(block.l,block.r,max);
			t->paint(block.l,block.r,max+block.size);
			ret[block.no] = max;
		}
		if(_perm!=sol.size()-1){
			floor += perm.upper - sol[_perm+1].lower;
		}
	}
	int ans=0;
	for(int i=1;i<=w;++i){
		int tmp = t->query(i,i) - tmin->query(i,i);
		cmax(ans,tmp);
	}
	delete t;
	delete tmin;
	return std::make_pair(ans,ret);
}

bool is_valid(Solution_t ans){
	data = data2;
	for(int i=0;i<data.size();++i){
		if(ans.first<data[i].size){
			std::cerr << "A\n";
			return 0;
		}
	}
	for(int i=0;i<data.size();++i){
		for(int j=i+1;j<data.size();++j){
			if(std::max(data[i].l,data[j].l)<=std::min(data[i].r,data[j].r)){
				int bi = ans.second[i]%ans.first, bj = ans.second[j]%ans.first;
				if((bj-bi+ans.first)%ans.first<data[i].size){
					std::cerr << i << " " << j << " " << data[i].no << " " << data[j].no << " " << bi << " " << bj << " " << data.size() << " " << ans.second.size() << " " << "B\n";
					return 0;
				}
				if((bi-bj+ans.first)%ans.first<data[j].size){
					std::cerr << i << " " << j << " " << "C\n";
					return 0;
				}
			}
		}
	}
	return 1;
}

void draw(Solution_t sol, std::string filename = "rect.json"){
	Json::Value rects;
	for(int i=0;i<data2.size();++i){
		Json::Value rect;
		rect.append(data2[i].l-1);
		rect.append(data2[i].r-1);
		rect.append(sol.second[i]%sol.first);
		rect.append((sol.second[i]+data2[i].size-1)%sol.first);
		rects.append(rect);
	}
	Json::Value color;
	for(int i=0;i<sol.second.size();++i){
		if(data2[i].type == datablock::datatype::ifmap){
			color.append("blue");
		}
		if(data2[i].type == datablock::datatype::ofmap){
			color.append("red");
		}
		if(data2[i].type == datablock::datatype::weight){
			color.append("green");
		}
	}
	Json::Value output;
	output["w"] = w;
	output["h"] = sol.first;
	output["rects"] = rects;
	output["color"] = color;
	Json::FastWriter writer;
	std::ofstream outputfile(filename);
	outputfile << writer.write(output);
}

Solution_t drop(){
	std::vector<std::vector<bool> > buf;
	buf.resize(w+1);
	Solution_t sol;
	sol.second.resize(data.size());
	static std::vector<int> lbound, ubound;
	lbound.resize(w+1);
	ubound.resize(w+1);
	for(int &l: lbound){
		l = 0x3f3f3f3f;
	}
	for(int &u: ubound){
		u = 0;
	}
	for(datablock &block: data){
		std::vector<int> now(block.r-block.l+1);
		int pl;
		for(pl=0;;++pl){
			int min=block.size;
			for(int i=block.l;i<=block.r;++i){
				if(buf[i].size()>pl && buf[i][pl]){
					now[i-block.l]=0;
				}
				else{
					now[i-block.l]++;
				}
				cmin(min,now[i-block.l]);
			}
			if(min == block.size){
				break;
			}
		}
		for(int i=block.l;i<=block.r;++i){
			if(buf[i].size()<=pl){
				buf[i].resize(pl+1);
			}
			for(int j=pl;j>pl-block.size;--j){
				assert(buf[i][j]==0);
				buf[i][j]=1;
			}
			cmin(lbound[i], pl-block.size+1);
			cmax(ubound[i], pl);
		}
		sol.second[block.no] = pl - block.size + 1;
	}
	for(int i=1;i<=w;++i){
		cmax(sol.first, ubound[i]-lbound[i]+1);
	}
	buf.clear();
	lbound.clear();
	ubound.clear();
	return sol;
}

void greedy1(){
	std::sort(data.begin(),data.end(),cmp1);
	auto ans = drop();
	assert(is_valid(ans));
	draw(ans,dirname+"wlist/"+coreid+"/first_come_first_serve.json");
	printf("first_come_first_serve: %d\n",ans.first);
	fcfs_sol = ans;
}

void greedy2(){
	std::sort(data.begin(),data.end(),cmp2);
	auto ans = greedy();
	assert(is_valid(ans));
	draw(ans,dirname+"wlist/"+coreid+"/biggest_size_first.json");
	printf("biggest_size_first: %d\n",ans.first);
}

void greedy3(){
	std::sort(data.begin(),data.end(),cmp3);
	auto ans = drop();
	assert(is_valid(ans));
	draw(ans,dirname+"wlist/"+coreid+"/deadline_earliest_first.json");
	printf("deadline_earliest_first: %d\n",ans.first);
}

void greedy4(){
	std::sort(data.begin(),data.end(),cmp4);
	auto ans = greedy();
	assert(is_valid(ans));
	draw(ans,dirname+"wlist/"+coreid+"/large_area_first.json");
	printf("large_area_first: %d\n",ans.first);
}

void output(Solution_t sol){
	Json::Value ans;
	for(auto addr:sol.second){
		ans.append(addr%sol.first);
	}
	Json::FastWriter writer;
	std::ofstream ofile(dirname+"wlist/"+coreid+"/addr.json");
	ofile << writer.write(ans);
}

bool sa_accept(std::pair<int,int> best,std::pair<int,int> now,double T){
	static std::default_random_engine generator;
	static std::uniform_real_distribution<double> distribution(0.0,1.0);
	cmax(best.first,lwb);
	cmax(now.first,lwb);
	if(now<=best){
		return true;
	}
	if(now.first == best.first){
		double prob = exp((best.second-now.second)/T);
		return distribution(generator) <= prob;
	}
	double prob = exp((best.first-now.first)/T);
	return distribution(generator) <= prob;
}

std::vector<int> sa_change(const std::vector<int> &p, int prob_deal_with_bottleneck = 0, int prob_deal_with_last = 0){
	if(p.size() == 1){
		return p;
	}
	int l,r;
	std::vector<int> ret = p;
	int x = rand()%100;
	if(x < prob_deal_with_bottleneck){
		std::vector<int> in_bottleneck;
		for(int i=0; i<p.size(); ++i){
			if(std::lower_bound(bottleneck.begin(),bottleneck.end(),original[p[i]].l-7) != std::upper_bound(bottleneck.begin(),bottleneck.end(),original[p[i]].r+7)){
				in_bottleneck.push_back(i);
			}
		}
		l=in_bottleneck[rand()%in_bottleneck.size()];
		do{
			r=rand()%p.size();
		} while (l==r);
	}
	else if(x < prob_deal_with_bottleneck + prob_deal_with_last){
		int maxr=0;
		for(int i=0; i<p.size(); ++i){
			cmax(maxr, original[p[i]].r);
		}
		std::vector<int> in_bottleneck;
		for(int i=0; i<p.size(); ++i){
			if(original[p[i]].r >= maxr - 7){
				in_bottleneck.push_back(i);
			}
		}
		l=in_bottleneck[rand()%in_bottleneck.size()];
		do{
			r=rand()%p.size();
		} while (l==r);
	}
	else{
		do{
			l=rand()%p.size();
			r=rand()%p.size();
		} while (l==r);
	}
	if(l<r){
		for(int i=l;i<r;++i){
			std::swap(ret[i],ret[i+1]);
		}
	}
	else{
		for(int i=l;i>r;--i){
			std::swap(ret[i],ret[i-1]);
		}
	}
	return ret;
}

Solution_t sa_evaluate(const std::vector<int> &p,int left = 1, int right = w, int last = 0, int* retlast = NULL, int* lower = NULL, int* upper = NULL){
	/*for(int i=0;i<data.size();++i){
		data[i] = data2[p[i]];
	}*/
	data.resize(p.size());
	for(int i=0;i<p.size();++i){
		data[i] = original[p[i]];
	}
	return greedy(data,left,right,last,retlast,lower,upper);
}

void sa(){
	std::vector<int> now,best,iter;
	Solution_t best_sol, iter_sol;
	srand(time(0));
	best_sol.first = 0x3f3f3f3f;
	int n=data.size();
	now.resize(n);
	for(int i=0;i<n;++i){
		now[i]=i;
	}
	std::random_shuffle(now.begin(),now.end());
	best = now;
	iter = now;
	best_sol = sa_evaluate(now);
	iter_sol = best_sol;
	double T = 10;
	int rnd = 0;
	int rnds = log(1/T)/log(0.99);
	while(T>1 && best_sol.first > lwb){
		now = sa_change(iter);
		auto sol = sa_evaluate(now);
		if(sa_accept({iter_sol.first, 0}, {sol.first, 0}, T)){
			iter = now;
			iter_sol = sol;
		}
		if(sol.first < best_sol.first){
			best_sol = sol;
		}
		if(++rnd == std::min(3*n, 100000/rnds)){
			fprintf(stderr,"T = %lf, best = %d, iter = %d\n",T,best_sol.first,iter_sol.first);
			T *= 0.99;
			rnd = 0;
		}
	}
	assert(is_valid(best_sol));
}

void sa_opt1(){
	std::vector<int> now,best,iter;
	Solution_t best_sol, iter_sol;
	srand(time(0));
	best_sol.first = 0x3f3f3f3f;
	int n=data.size();
	now.resize(n/top_batch_cut);
	for(int i=0;i<n/top_batch_cut;++i){
		now[i]=i;
	}
	std::random_shuffle(now.begin(),now.end());
	best = now;
	iter = now;
	best_sol = sa_evaluate(now);
	iter_sol = best_sol;
	double T = 10;
	int rnd = 0;
	int rnds = log(1/T)/log(0.99);
	while(T>1){
		now = sa_change(iter);
		auto sol = sa_evaluate(now);
		if(sa_accept({iter_sol.first, 0}, {sol.first, 0}, T)){
			iter = now;
			iter_sol = sol;
		}
		if(sol.first < best_sol.first){
			best_sol = sol;
			best = now;
		}
		if(++rnd == std::min(3*n,100000/rnds)){
			fprintf(stderr,"T = %lf, best = %d, iter = %d\n",T,best_sol.first,iter_sol.first);
			T *= 0.99;
			rnd = 0;
			if(greedy_opt1(best).first == lwb){
				break;
			}
		}
	}
	data.resize(n);
	best_sol = greedy_opt1(best);
	assert(is_valid(best_sol));
	draw(best_sol,dirname+"wlist/"+coreid+"/sa.json");
	printf("sa: %d\n",best_sol.first);
	Json::Value ans;
	for(auto addr:best_sol.second){
		ans.append(addr%best_sol.first);
	}
	Json::FastWriter writer;
	std::ofstream ofile(dirname+"wlist/"+coreid+"/addr.json");
	ofile << writer.write(ans);
}

void sa_opt2(){
	srand(time(0));
	int n=original.size();
	
	std::vector<int> lmaxr, rminl;
	lmaxr.resize(n);
	rminl.resize(n);
	for(int i=0; i<n; ++i){
		lmaxr[i] = original[i].r;
		rminl[i] = original[i].l;
	}
	for(int i=1;i<n;++i){
		cmax(lmaxr[i],lmaxr[i-1]);
	}
	for(int i=n-2;i>=0;--i){
		cmin(rminl[i],rminl[i+1]);
	}
	int last_left = 0;
	compl_sol_t solution;
	int sol_size;
	std::vector<int> fcfs_top(w+1);
	std::vector<int> height(w+1);
	for(int i=0;i<n && i<n/top_batch_cut*2;++i){
		if(i==n-1 || lmaxr[i]<=rminl[i+1]){
			int len = i-last_left+1;
			std::vector<int> now, iter, best;
			Solution_t iter_sol, best_sol;
			int iter_lastcol, best_lastcol;
			int r_lower_bound = 0;
			for(int j=last_left; j<=i; ++j){
				iter.push_back(j);
				if(original[j].r == lmaxr[i]){
					r_lower_bound += original[j].size;
				}
			}
			//std::random_shuffle(iter.begin(),iter.end());
			//std::sort(iter.begin(), iter.end(), [&](int a,int b){return (original[a].r-original[a].l) > (original[b].r-original[b].l);});
			int last_top = 0;
			if(last_left && rminl[last_left] == lmaxr[last_left-1]){
				last_top = fcfs_top[rminl[last_left]];
			}
			std::sort(iter.begin(), iter.end(), [&](int a,int b){return (fcfs_sol.second[original[a].no]-last_top+fcfs_sol.first)%fcfs_sol.first < (fcfs_sol.second[original[b].no]-last_top+fcfs_sol.first)%fcfs_sol.first;});
			iter_sol = sa_evaluate(iter, rminl[last_left], lmaxr[i], height[rminl[last_left]], &iter_lastcol);
			best_sol = iter_sol;
			best = iter;
			best_lastcol = iter_lastcol;
			double T = 5;
			int rnd = 0;
			int rnds = log(2/T)/log(0.99);
			int is_output = 0;
			while(T>.005){
				now = sa_change(iter, 40, 20);
				int sol_lastcol;
				auto sol = sa_evaluate(now, rminl[last_left], lmaxr[i], height[rminl[last_left]], &sol_lastcol);
				if(sa_accept(std::make_pair(iter_sol.first, iter_lastcol), std::make_pair(sol.first, sol_lastcol), T)){
					iter = now;
					iter_sol = sol;
					iter_lastcol = sol_lastcol;
				}
				if(sa_accept(std::make_pair(best_sol.first, best_lastcol), std::make_pair(sol.first, sol_lastcol), 1e-20)){
					best_sol = sol;
					best = now;
					best_lastcol = sol_lastcol;
				}
				if(++rnd == std::min(3*n,100000/rnds)){
					if(++is_output == 10){
						fprintf(stderr,"T = %lf, best = (%d,%d), iter = (%d,%d)\n",T,best_sol.first,best_lastcol,iter_sol.first,iter_lastcol);
						is_output = 0;
					} 
					if(T>0.5 && T*0.99<=0.5){
						iter = best;
						iter_lastcol = best_lastcol;
						iter_sol = best_sol;
						T = 0.03 / 0.99;
					}
					T *= 0.99;
					rnd = 0;
				}
				if(best_sol.first <= lwb && best_lastcol <= r_lower_bound){
					break;
				}
			}
			int lower,upper;
			int last_height = 0;
			auto ans = sa_evaluate(best, rminl[last_left], lmaxr[i], height[rminl[last_left]], &last_height, &lower, &upper);
			height[lmaxr[i]] += last_height;
			sol_perm item = (sol_perm){last_left, i, lower, upper, best};
			solution.push_back(item);
			for(int j=last_left; j<=i; ++j){
				cmax(fcfs_top[original[j].r], fcfs_sol.second[original[j].no]+original[j].size);
			}
			last_left = i+1;
		}
		if(i==n/top_batch_cut-1){
			sol_size = solution.size();
			assert(last_left == i+1);
		}
	}
	
	int sec_size = solution.size() - sol_size;
	for(int i=solution.size(); i<sol_size+sec_size*(top_batch_cut-1); ++i){
		sol_perm item = solution[solution.size()-sec_size];
		for(int &p: item.p){
			p += n/top_batch_cut;
		}
		item.wlist_left = w;
		item.wlist_right = 1;
		for(int p: item.p){
			cmin(item.wlist_left, original[p].l);
			cmax(item.wlist_right, original[p].r);
		}
		sa_evaluate(item.p, item.wlist_left, item.wlist_right, 0, NULL, &(item.lower), &(item.upper));
		solution.push_back(item);
	}

	auto best_sol = greedy_opt2(solution);

	assert(is_valid(best_sol));
	draw(best_sol,dirname+"wlist/"+coreid+"/sa.json");
	printf("sa: %d\n",best_sol.first);
	output(best_sol);
}

void randomperm(long timelimit){
	auto min = std::make_pair(0x3f3f3f3f,std::vector<int>{});
	srand((unsigned)time(0));
	long start=clock();
	long long cnt=0;
	data = data2;
	while(clock()-start<=timelimit){
		++cnt;
		std::random_shuffle(data.begin(),data.end());
		auto ans = greedy();
		if(ans.first < min.first){
			min = ans;
		}
	}
	assert(is_valid(min));
	draw(min, dirname+"wlist/"+coreid+"/random.json");
	printf("random: %d (%lld permutations computed)\n",min.first,cnt);
}

int lower_bound(const std::vector<datablock> &data = ::data,int print = 1){
	int max=0;
	for(int i=1;i<=w;++i){
		int now=0;
		for(auto j:data){
			if(j.l<=i&&j.r>=i){
				now+=j.size;
			}
		}
		cmax(max,now);
	}
	if(print)
		printf("lower_bound: %d\n",max);
	return max;
}


void init(){
	top_batch_cut = input["top_batch_cut"].asInt();
	for(Json::Value block: input["rects"]){
		datablock dat;
		dat.no = block["no"].asInt();
		dat.l = block["left"].asInt();
		dat.r = block["right"].asInt();
		dat.size = block["size"].asInt();
		if(block["type"].asString() == "ifmap"){
			dat.type = datablock::datatype::ifmap;
		}
		else if(block["type"].asString() == "ofmap"){
			dat.type = datablock::datatype::ofmap;
		}
		else{
			dat.type = datablock::datatype::weight;
		}
		cmax(w,dat.r);
		assert(dat.l<=dat.r);
		data.push_back(dat);
	}
	original = data;
	std::sort(data.begin(),data.end());
	data2 = data;
}

int main(int argc, char** argv){
	if(argc < 2){
		std::cerr << "format: ./allocator <coreid> [<dirname>]\n";
		return 1;
	}
	coreid = argv[1];
	if(argc < 3){
		dirname = "";
	}
	else{
		dirname = std::string(argv[2]) + "/";
	}
	std::ifstream inputfile(dirname + "wlist/" + argv[1] + ".json");
	Json::Reader reader;
	reader.parse(inputfile, input);
	std::filesystem::create_directories(dirname+"wlist/"+coreid+"/");
	const int buffersize = 1024;
	init();
	input.clear();
	lwb = lower_bound();
	greedy1();
	if(fcfs_sol.first<=buffersize){
		output(fcfs_sol);
		return 0;
	}
	greedy2();
	greedy3();
	greedy4();
	long start = clock();
	sa_opt2();
	long end = clock();
	randomperm(end-start);
	return 0;
}