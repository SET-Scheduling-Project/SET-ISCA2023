#include "memlayout.h"


/*
MemLayout::MemLayout(std::vector<pos_t>& _layout) : layout(_layout){}
*/

bool MemLayout::operator==(const MemLayout& other) const {
	return layout == other.layout;
}

bool MemLayout::operator!=(const MemLayout& other) const {
	return layout != other.layout;
}

void MemLayout::clear(){
	layout.clear();
}

bool MemLayout::empty() const {
	return layout.empty();
}

void MemLayout::set_layout(const std::vector<didx_t>& _layout){
	layout = _layout;
}

void MemLayout::set_layout(std::vector<didx_t>&& _layout){
	layout = _layout;
}

const std::vector<didx_t>& MemLayout::get_layouts() const{
	return layout;
}
