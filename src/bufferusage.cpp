#include "bufferusage.h"

#include <stdexcept>

#include "layerengine.h"
#include "schnode.h"


BufferUsage::BufferUsage()
	:BufferUsage(SchNode::layerMapper->get_ubuf_size()){}

BufferUsage::BufferUsage(vol_t _max_vol): capacity(_max_vol), valid(true){}

BufferUsage::operator bool() const{
	return valid;
}

BufferUsage BufferUsage::operator+(const BufferUsage& other) const{
	BufferUsage u = other;
	u += *this;
	return u;
}

BufferUsage& BufferUsage::operator+=(const BufferUsage& other){
	if(&other == this){
		throw std::invalid_argument(
			"BufferUsage: \"a += a\" not supported. Use a.multiple(2) instead.");
	}
	if(other.capacity != capacity){
		throw std::invalid_argument(
			"BufferUsage: only usages with same capacity can be added.");
	}
	if(!valid || !other.valid){
		valid = false;
		return *this;
	}
	for(const auto& x : other.usage){
		(void) add(x.first, x.second);
	}
	return *this;
}

void BufferUsage::max_with(const BufferUsage& other){
	if(other.capacity != capacity){
		throw std::invalid_argument(
			"BufferUsage: only usages with same capacity can take max.");
	}
	if(!valid || !other.valid){
		valid = false;
		return;
	}
	for(const auto& x : other.usage){
		vol_t& core_usage = usage[x.first];
		core_usage = MAX(core_usage, x.second);
	}
}

bool BufferUsage::add(pos_t core, vol_t size){
	valid = valid && ((usage[core] += size) <= capacity);
	return valid;
}

bool BufferUsage::all_add(vol_t size){
	for(auto& x : usage){
		valid = valid && ((x.second += size) <= capacity);
	}
	return valid;
}

bool BufferUsage::multiple(vol_t n){
	for(auto& x : usage){
		valid = valid && ((x.second *= n) <= capacity);
	}
	return valid;
}

vol_t BufferUsage::max() const{
	if(!valid) return 0;
	vol_t max_vol = 0;
	for(const auto& x : usage){
		max_vol = MAX(max_vol, x.second);
	}
	return max_vol;
}

double BufferUsage::avg() const{
	if(!valid) return 0;
	if(usage.empty()) return 0;
	double avg_vol = 0;
	for(const auto& x : usage){
		avg_vol += x.second;
	}
	return avg_vol / usage.size();
}

vol_t BufferUsage::get_capacity() const{
	return capacity;
}

std::ostream& operator<<(std::ostream& os, const BufferUsage& usage){
	return os << "Buffer(max=" << usage.max() << ", avg=" << usage.avg() << ")";
}
