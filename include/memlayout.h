#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

#include <vector>

#include "util.h"


class MemLayout{
	std::vector<pos_t> layout;
public:
	MemLayout() = default;
	// explicit MemLayout(std::vector<pos_t>& _layout);
	void clear();
	// void add_layout(pos_t pos);
	void set_layout(const std::vector<pos_t>& _layout);
	void set_layout(std::vector<pos_t>&& _layout);
	const std::vector<pos_t>& get_layouts() const;
};

#endif // MEMLAYOUT_H
