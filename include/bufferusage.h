/* This file contains
 *	BufferUsage: a class that records the occupied buffer size of each chip.
 */

#ifndef BUFFERUSAGE_H
#define BUFFERUSAGE_H

#include <iostream>
#include <unordered_map>

#include "util.h"


// Records the usage of each buffer
class BufferUsage{
private:
	// usage: records size of used buffer on each chip
	// usage[chip] = used_buffer_size_on_this_chip
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

	// Add to a chip.
	bool add(pos_t chip, vol_t size);
	// Adds to all chips in usage.
	bool all_add(vol_t size);
	// Multiple size of all chips in usage.
	bool multiple(vol_t n);

	// Maximal usage among all chips.
	vol_t max() const;
	// Average usage among all chips.
	double avg() const;
	// Get maximal capacity of buffer.
	vol_t get_capacity() const;

	friend std::ostream& operator<<(std::ostream& os, const BufferUsage& usage);
};

#endif // BUFFERUSAGE_H
