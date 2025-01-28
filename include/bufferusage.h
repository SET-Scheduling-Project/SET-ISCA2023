/* This file contains
 *	BufferUsage: a class that records the occupied buffer size of each core.
 */

#ifndef BUFFERUSAGE_H
#define BUFFERUSAGE_H

#include <iostream>
#include <unordered_map>

#include "util.h"


// Records the usage of each buffer
class BufferUsage{
private:
	// usage: records size of used buffer on each core
	// usage[core] = used_buffer_size_on_this_core
	std::unordered_map<pos_t, vol_t, pos_hash> usage;

	// capacity: maximal volume of each buffer
	vol_t capacity;

	// valid: sets to false if the current usage already exceeded max_vol.
	// usage will stop recording when valid=false
	bool valid;

public:
	BufferUsage();
	BufferUsage(vol_t _max_vol);

	// Returns whether buffer is valid
	// Example: BufferUsage b; if(b){...}
	operator bool() const;

	// Chip-wise addition with other.
	BufferUsage operator+(const BufferUsage& other) const;
	BufferUsage& operator+=(const BufferUsage& other);

	// Chip-wise "max" with other.
	void max_with(const BufferUsage& other);

	// Add to a core.
	bool add(pos_t core, vol_t size);
	// Adds to all cores in usage.
	bool all_add(vol_t size);
	// Multiple size of all cores in usage.
	bool multiple(vol_t n);

	// Maximal usage among all cores.
	vol_t max() const;
	// Average usage among all cores.
	double avg() const;
	// Get maximal capacity of buffer.
	vol_t get_capacity() const;

	friend std::ostream& operator<<(std::ostream& os, const BufferUsage& usage);
};

#endif // BUFFERUSAGE_H
