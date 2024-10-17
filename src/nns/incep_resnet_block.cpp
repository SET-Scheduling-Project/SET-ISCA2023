#include "nns/nns.h"

#include <cstdarg>
#include <cstdio>
#include <exception>
#include <string>


static std::string fmt(const char* format, ...) {
	va_list args;
	va_start(args, format);
	int s_len = std::vsnprintf(nullptr, 0, format, args) + 1; // Extra space for '\0'
	va_end(args);
	if(s_len <= 0){
		throw std::runtime_error(std::string("Error during formatting ")+format);
	}
	char* buf = new char[s_len];
	va_start(args, format);
	std::vsnprintf(buf, s_len, format, args);
	va_end(args);
	std::string s(buf);
	delete[] buf;
	return s;
}

static lid_t Stem(Network& n) {
	InputData input("input", fmap_shape(192, 71));
	return n.add(NLAYER("Conv_4b_3x3", Conv, C=192, K=256, H=35, R=3, sH=2), {}, 0, {input});
}

static lid_t Inception_Resnet_A(Network& n, lid_t lastid, int block_no) {
	auto branch1 = n.add(NLAYER(fmt("A%d_Conv_1a_1x1",block_no), Conv, C=256, K=32, H=35));
	n.add(NLAYER(fmt("A%d_Conv_2a_1x1",block_no), Conv, C=256, K=32, H=35), {lastid});
	auto branch2 = n.add(NLAYER(fmt("A%d_Conv_2b_3x3",block_no), Conv, C=32, K=32, H=35, R=3));
	n.add(NLAYER(fmt("A%d_Conv_3a_1x1",block_no), Conv, C=256, K=32, H=35), {lastid});
	n.add(NLAYER(fmt("A%d_Conv_3b_3x3",block_no), Conv, C=32, K=32, H=35, R=3));
	auto branch3 = n.add(NLAYER(fmt("A%d_Conv_3c_3x3",block_no), Conv, C=32, K=32, H=35, R=3));
	auto conv4 = n.add(NLAYER(fmt("A%d_Conv_4_1x1",block_no), Conv, C=96, K=256, H=35), {branch1,branch2,branch3});
	return n.add(NLAYER(fmt("A%d_Eltwise",block_no), Eltwise, N=2, K=256, H=35), {lastid, conv4});
}


const Network inception_resnet_block = []{
	Network n;
	auto lastid=Stem(n);
	Inception_Resnet_A(n, lastid, 1);
	return n;
}();

