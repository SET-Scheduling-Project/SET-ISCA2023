/* This file contains
 *	Bitset: a wrapper class for std::bitset
 */

#ifndef BITSET_H
#define BITSET_H

#include <bitset>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <vector>


/* Used for iterating Bitset objects
 * Example (calculating sum of bitset):
 *
 * Bitset b({1,3,6});
 * int sum = 0;
 * FOR_BITSET(i, b){
 *     sum += i;
 * }
 * assert(sum == 10);
 */
#define FOR_BITSET(var, set) for(Bitset::bitlen_t var = set.first(); var != set.size(); var = set.next(var))

// Maximal number of bits in the bitset. (must be representable by bitlen_t)
#define MAX_BITS_IN_BS 640

// Wrapper for std::bitset
class Bitset: private std::bitset<MAX_BITS_IN_BS>{
private:
	typedef std::bitset<MAX_BITS_IN_BS> std_bs;
	Bitset(std_bs&& base);

public:
	typedef std::uint16_t bitlen_t;

	static_assert(std::numeric_limits<bitlen_t>::max() >= MAX_BITS_IN_BS,
		"bitlen_t must be able to hold MAX_BITS_IN_BS, change it to a larger type.");

	Bitset()=default;
	explicit Bitset(bitlen_t bit);
	explicit Bitset(std::initializer_list<bitlen_t> bits);
	explicit Bitset(std::vector<bitlen_t> list);

	// Wrapper functions:
	bitlen_t count() const;
	bitlen_t first() const;
	bitlen_t next(bitlen_t bit) const;
	bool contains(bitlen_t bit) const;
	void set(bitlen_t bit);
	void reset(bitlen_t bit);
	void clear();
	bitlen_t size() const;
	Bitset& operator|=(const Bitset& other);
	bool operator==(const Bitset& other) const;
	//Bitset& operator|=(bitlen_t other);
	friend Bitset operator|(const Bitset& lhs, const Bitset& rhs);

	// Print to ostream
	friend std::ostream& operator<<(std::ostream& out, const Bitset& set);
};

#undef MAX_BITS_IN_BS

#endif // BITSET_H
