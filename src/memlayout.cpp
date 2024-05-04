#include "memlayout.h"


/*
MemLayout::MemLayout(std::vector<pos_t>& _layout) : layout(_layout){}
*/

void MemLayout::clear(){
	layout.clear();
}

void MemLayout::set_layout(const std::vector<pos_t>& _layout){
	layout = _layout;
}

void MemLayout::set_layout(std::vector<pos_t>&& _layout){
	layout = _layout;
}

const std::vector<pos_t>& MemLayout::get_layouts() const{
	return layout;
}
