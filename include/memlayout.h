#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

#include <vector>

#include "util.h"


class MemLayout{
	std::vector<didx_t> layout;
public:
	MemLayout() = default;
	// explicit MemLayout(std::vector<pos_t>& _layout);

	bool operator==(const MemLayout& other) const;
	bool operator!=(const MemLayout& other) const;

	void clear();
	bool empty() const;
	// void add_layout(pos_t pos);
	void set_layout(const std::vector<didx_t>& _layout);
	void set_layout(std::vector<didx_t>&& _layout);
	const std::vector<didx_t>& get_layouts() const;
};

struct MemLayouts{
	std::vector<MemLayout> iMemLayouts;
	MemLayout wMemLayout, oMemLayout;
};

#endif // MEMLAYOUT_H
