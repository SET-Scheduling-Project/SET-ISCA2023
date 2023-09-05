#include "layer.h"

#include <cassert>
#include <cstring>

Layer::Layer(const std::string& _name, const fmap_shape& _ifm_shape, const fmap_shape& _ofm_shape, const fmap_shape& _wgt_shape)
	:name(_name), ifm_shape(_ifm_shape), ofm_shape(_ofm_shape), wgt_shape(_wgt_shape), bitwidth(8){}

Layer::Layer(const std::string& _name)
	:name(_name), bitwidth(8){}

bwidth_t Layer::get_bitwidth() const{
	return bitwidth;
}

void Layer::set_bitwidth(bwidth_t width){
	bitwidth = width;
}

const std::string& Layer::get_name() const{
	return name;
}

utime_t Layer::get_utime() const{
	return unit_time;
}

void Layer::set_utime(utime_t time){
	unit_time = time;
}

const fmap_shape& Layer::tot_ifmap_shape() const{
	return ifm_shape;
}

const fmap_shape& Layer::ofmap_shape() const{
	return ofm_shape;
}

const fmap_shape& Layer::weight_shape() const{
	return wgt_shape;
}

void ConvLayer::Workload::init(){
	K = (K == 0)?C:K;
	W = (W == 0)?H:W;
	S = (S == 0)?R:S;
	sW = (sW == 0)?sH:sW;
	update_op();
	// This is wrong! What?
	// See resnet50 (conv3_0_br)
	//assert(sH<=R && sW<=S);
	assert(tot_op>0 && sH>0 && sW>0);
}

vol_t ConvLayer::Workload::ifm_size(len_t batch_size) const{
	return ((H-1)*sH + R) * ((W-1)*sW + S) * C * batch_size;
}

vol_t ConvLayer::Workload::fil_size() const{
	return R*S*C*K;
}

vol_t ConvLayer::Workload::ofm_size(len_t batch_size) const{
	return H*W*K*batch_size;
}

void ConvLayer::Workload::update_op(){
	tot_op = C*K*R*S*H*W;
}

access_t ConvLayer::Workload::calc_op(len_t batch_size) const{
	return tot_op*batch_size;
}

ConvLayer::ConvLayer(const std::string& _name, const ConvLayer::Workload& _wl)
	:Layer(_name), wl(_wl){
	wl.init();
	ifm_shape = fmap_shape(wl.C, (wl.H - 1) * wl.sH + wl.R, (wl.W - 1) * wl.sW + wl.S);
	ofm_shape = fmap_shape(wl.K, wl.H, wl.W);
	wgt_shape = fmap_shape(wl.K, wl.C, wl.R*wl.S);
	wgt_size = wl.fil_size();
}

const fmap_shape& ConvLayer::real_ifmap_shape() const{
	return padded_ifm_shape;
}

vol_t ConvLayer::weight_size() const{
	return wgt_size;
}

const ConvLayer::Workload& ConvLayer::get_workload() const{
	return wl;
}

bool ConvLayer::set_padded_ifm(const fmap_shape& padded_shape){
	if(padded_shape.c != ifm_shape.c) return false;
	padded_ifm_shape.c = ifm_shape.c;
	if(padded_shape.h > ifm_shape.h){
		if(padded_shape.h > wl.H * wl.sH) return false;
		pad_h = 0;
		padded_ifm_shape.h = ifm_shape.h;
	}else{
		len_t tot_ph = ifm_shape.h - padded_shape.h;
		if(tot_ph > 2*(wl.R - 1)) return false;
		pad_h = tot_ph/2;
		padded_ifm_shape.h = padded_shape.h;
	}
	if(padded_shape.w > ifm_shape.w){
		if(padded_shape.w > wl.W * wl.sW) return false;
		pad_w = 0;
		padded_ifm_shape.w = ifm_shape.w;
	}else{
		len_t tot_pw = ifm_shape.w - padded_shape.w;
		if(tot_pw > 2*(wl.S - 1)) return false;
		pad_w = tot_pw/2;
		padded_ifm_shape.w = padded_shape.w;
	}
	padded_ifm_shape.update_size();
	return true;
}

access_t ConvLayer::get_num_op(len_t batch_size) const{
	return wl.calc_op(batch_size);
}

void ConvLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	ofm_range.c = {0, wl.C};
	ofm_range.h.from = ofm_range.h.from * wl.sH;
	ofm_range.h.from = (ofm_range.h.from > pad_h)?(ofm_range.h.from - pad_h):0;
	ofm_range.w.from = ofm_range.w.from * wl.sW;
	ofm_range.w.from = (ofm_range.w.from > pad_w)?(ofm_range.w.from - pad_w):0;
	ofm_range.h.to = (ofm_range.h.to-1) * wl.sH + wl.R - pad_h;
	ofm_range.w.to = (ofm_range.w.to-1) * wl.sW + wl.S - pad_w;
	ofm_range.h.to = MIN(ofm_range.h.to, padded_ifm_shape.h);
	ofm_range.w.to = MIN(ofm_range.w.to, padded_ifm_shape.w);
}

void ConvLayer::ofm_to_wgt(fmap_range& ofm_range) const{
	// B = B(1)
	// ofm_range.b = {0, 1};
	// C = K, H = C, W = R*S
	// ofm_range.c = ofm_range.c;
	ofm_range.h = {0, wl.C};
	ofm_range.w = {0, wl.R*wl.S};
}

bool ConvLayer::fmap_channel_rel() const{
	return false;
}

void GroupConvLayer::Workload::init(){
	ConvLayer::Workload::init();
	assert(G >= 1);
	assert(C%G == 0);
	assert(K%G == 0);
	GC = C/G;
	GK = K/G;
	tot_op /= G;
}

vol_t GroupConvLayer::Workload::fil_size() const{
	return R*S*GC*K;
}

GroupConvLayer::GroupConvLayer(const std::string& _name, const Workload& _wl)
	:ConvLayer(_name, _wl), wl(_wl){
	wl.init();
	ConvLayer::wl = wl;
	ConvLayer::wgt_size = wl.fil_size();
	wgt_shape = fmap_shape(wl.K, wl.GC, wl.R*wl.S);
}

const GroupConvLayer::Workload& GroupConvLayer::get_workload() const{
	return wl;
}

void GroupConvLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	len_t group_Klen = wl.GK;
	len_t group_Clen = wl.GC;
	len_t from_id = ofm_range.c.from / group_Klen;
	len_t to_id = DIVCEIL(ofm_range.c.to, group_Klen);
	ConvLayer::ofm_to_ifm(ofm_range);
	ofm_range.c.from = from_id * group_Clen;
	ofm_range.c.to = to_id * group_Clen;
}

void GroupConvLayer::ofm_to_wgt(fmap_range& ofm_range) const{
	// B = B(1)
	// ofm_range.b = {0, 1};
	// C = K, H = C, W = R*S
	// ofm_range.c = ofm_range.c;
	ofm_range.h = {0, wl.GC};
	ofm_range.w = {0, wl.R*wl.S};
}

bool GroupConvLayer::fmap_channel_rel() const{
	return true;
}

FCLayer::FCLayer(const std::string& _name, const Workload& wl)
	:ConvLayer (_name, [&]{ConvLayer::Workload cwl;
							cwl.C = wl.C; cwl.K=wl.K; cwl.R=wl.IH; cwl.S=wl.IW; cwl.H=1;
							return cwl;}()){
}

void FCLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	assert(ofm_range.h.from == 0 && ofm_range.w.from == 0 &&
		   ofm_range.h.to == 1 && ofm_range.w.to == 1);
	ofm_range.c = {0, wl.C};
	ofm_range.h.to = wl.R - pad_h;
	ofm_range.w.to = wl.S - pad_w;
}

void LRLayer::Workload::init(){
	W = (W == 0)?H:W;
	S = (S == 0)?R:S;
	sK = (sK == 0)?N:sK;
	sH = (sH == 0)?R:sH;
	sW = (sW == 0)?sH:sW;
	update_op();
	assert(sK<=N && 3 && sW<=S);
	assert(tot_op>0 && sK>0 && sH>0 && sW>0);
}

void LRLayer::Workload::update_op(){
	tot_op = K*H*W*N*R*S;
}

access_t LRLayer::Workload::calc_op(len_t batch_size) const{
	return tot_op*batch_size;
}

LRLayer::LRLayer(const std::string& _name, const Workload& _wl):
	Layer(_name), wl(_wl){
	wl.init();
	ifm_shape = fmap_shape((wl.K - 1) * wl.sK + wl.N, (wl.H - 1) * wl.sH + wl.R, (wl.W - 1) * wl.sW + wl.S);
	ofm_shape = fmap_shape(wl.K, wl.H, wl.W);
	// wgt_shape = fmap_shape(0, 0, 0);
}

const LRLayer::Workload& LRLayer::get_workload() const{
	return wl;
}

const fmap_shape& LRLayer::real_ifmap_shape() const{
	return padded_ifm_shape;
}

vol_t LRLayer::weight_size() const{
	return 0;
}

access_t LRLayer::get_num_op(len_t batch_size) const{
	return wl.calc_op(batch_size);
}

void LRLayer::ofm_to_wgt(fmap_range& ofm_range) const{
	(void) ofm_range;
}

bool LRLayer::fmap_channel_rel() const{
	return true;
}

PoolingLayer::PoolingLayer(const std::string& _name, const PoolingLayer::Workload& wl)
	:LRLayer(_name, [&]{LRLayer::Workload lwl;
	lwl.N=1; lwl.K = wl.K; lwl.H = wl.H; lwl.W = wl.W; lwl.R=wl.R;
	lwl.S=wl.S; lwl.sH = wl.sH; lwl.sW = wl.sW;
	return lwl;}()){}

bool PoolingLayer::set_padded_ifm(const fmap_shape& padded_shape){
	if(padded_shape.h > ifm_shape.h
	|| padded_shape.w > ifm_shape.w
	|| padded_shape.c != ifm_shape.c) return false;

	len_t tot_ph = ifm_shape.h - padded_shape.h;
	len_t tot_pw = ifm_shape.w - padded_shape.w;
	if(tot_ph > 2*(wl.R - 1) || tot_pw > 2*(wl.S - 1)){
		return false;
	}
	pad_h = tot_ph/2;
	pad_w = tot_pw/2;
	padded_ifm_shape = padded_shape;
	padded_ifm_shape.update_size();
	return true;
}

void PoolingLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	ofm_range.h.from = ofm_range.h.from * wl.sH;
	ofm_range.h.from = (ofm_range.h.from > pad_h)?(ofm_range.h.from - pad_h):0;
	ofm_range.w.from = ofm_range.w.from * wl.sW;
	ofm_range.w.from = (ofm_range.w.from > pad_w)?(ofm_range.w.from - pad_w):0;
	ofm_range.h.to = (ofm_range.h.to-1) * wl.sH + wl.R - pad_h;
	ofm_range.w.to = (ofm_range.w.to-1) * wl.sW + wl.S - pad_w;
	ofm_range.h.to = MIN(ofm_range.h.to, padded_ifm_shape.h);
	ofm_range.w.to = MIN(ofm_range.w.to, padded_ifm_shape.w);
}

EltwiseLayer::EltwiseLayer(const std::string& _name, const EltwiseLayer::Workload& wl)
	:LRLayer(_name, [&]{LRLayer::Workload lwl;
	lwl.N=wl.N; lwl.K = wl.K; lwl.H = wl.H; lwl.W = wl.W; lwl.R=1;
	return lwl;}()){
	padded_ifm_shape = ifm_shape;
	pad_h = pad_w = 0;
}

bool EltwiseLayer::set_padded_ifm(const fmap_shape& padded_shape){
	return padded_shape == padded_ifm_shape;
}

void EltwiseLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	(void) ofm_range;
}

PTPLayer::PTPLayer(const std::string& _name, const PTPLayer::Workload& wl)
	:LRLayer(_name, [&]{LRLayer::Workload lwl;
	lwl.K = wl.K; lwl.H = wl.H; lwl.W = wl.W; lwl.N=1; lwl.R=1;
	return lwl;}()){
	padded_ifm_shape = ifm_shape;
	pad_h = pad_w = 0;
}

bool PTPLayer::set_padded_ifm(const fmap_shape& padded_shape){
	return padded_shape == padded_ifm_shape;
}

void PTPLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	(void) ofm_range;
}

TransposeLayer::Workload::Workload(){
	for(int i=0; i<dim::NUM; ++i){
		order[i] = static_cast<dim>(i);
	}
}

void TransposeLayer::Workload::init(){
	bool used[dim::NUM];
	memset(used, 0, sizeof(bool)*dim::NUM);
	for(int i=0; i<dim::NUM; ++i){
		assert(!used[order[i]]);
		used[order[i]] = true;
	}
}

fmap_range::dim_range& TransposeLayer::Workload::get_origin_dim(fmap_range& range, dim d) const{
	switch(order[d]){
	case dim::C:
		return range.c;
	case dim::H:
		return range.h;
	case dim::W:
		return range.w;
	default:
		assert(false);
		return range.b;
	}
}

TransposeLayer::TransposeLayer(const std::string& _name, const TransposeLayer::Workload& _wl)
	:LRLayer(_name, [&]{LRLayer::Workload lwl;
	lwl.K = _wl.K; lwl.H = _wl.H; lwl.W = _wl.W; lwl.N=1; lwl.R=1;
	return lwl;}()), wl(_wl){
	fmap_range ifm_range(ofm_shape);
	TransposeLayer::ofm_to_ifm(ifm_range);
	ifm_shape = {ifm_range.c.to, ifm_range.h.to, ifm_range.w.to};
	padded_ifm_shape = ifm_shape;
	pad_h = pad_w = 0;
}

bool TransposeLayer::set_padded_ifm(const fmap_shape& padded_shape){
	return padded_shape == padded_ifm_shape;
}

void TransposeLayer::ofm_to_ifm(fmap_range& ofm_range) const{
	fmap_range _ofm_range = ofm_range;
	wl.get_origin_dim(ofm_range, dim::C) = _ofm_range.c;
	wl.get_origin_dim(ofm_range, dim::H) = _ofm_range.h;
	wl.get_origin_dim(ofm_range, dim::W) = _ofm_range.w;
}

