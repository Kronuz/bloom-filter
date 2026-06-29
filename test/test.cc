// Smoke test for the standalone bloom-filter library.
//
// Exercises bloom_filter.hh: the BloomFilter<N> class template, its add() /
// contains() pair, and the salt parameter. A Bloom filter has a hard contract
// and a soft one. The hard contract: no false negatives, ever. Every key that
// was add()ed must be reported present by contains(), with no exceptions. The
// soft contract: a key that was never added is usually reported absent, but may
// occasionally collide into a false positive; the rate stays low as long as the
// filter is sized for the load. This test pins the hard contract exactly and
// holds the soft one to a loose upper bound over many probes.
//
// BloomFilter<N> sizes its bit array to m = N * 32 bits and uses k = 20 probes,
// targeting a ~1e-6 false-positive rate at N inserted elements. The filters here
// are heap-allocated: even a small N makes the bitset member far too large for
// the stack (the default N = 131072 is a 512 KiB object).
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "bloom_filter.hh"


// ---------------------------------------------------------------------------
// Helpers: add / query a std::string through the (const char*, size_t) API.
// ---------------------------------------------------------------------------

template <typename Filter>
static void add(Filter& f, const std::string& s, uint64_t salt = 1) {
	f.add(s.data(), s.size(), salt);
}

template <typename Filter>
static bool contains(const Filter& f, const std::string& s, uint64_t salt = 1) {
	return f.contains(s.data(), s.size(), salt);
}

// A deterministic key generator, so present/absent sets never overlap: present
// keys are "present-<i>", absent probe keys are "absent-<i>".
static std::string present_key(int i) { return "present-" + std::to_string(i); }
static std::string absent_key(int i)  { return "absent-" + std::to_string(i); }


// ---------------------------------------------------------------------------
// The hard contract: no false negatives. Every added key must read back as
// present. This must hold with zero exceptions, so the assert is unconditional.
// ---------------------------------------------------------------------------

static void test_no_false_negatives() {
	// N is the design load; insert a healthy fraction of it and demand every one
	// reads back. A small N keeps the bitset modest (8192 * 32 bits = 32 KiB).
	constexpr size_t N = 8192;
	auto f = std::make_unique<BloomFilter<N>>();

	const int count = 4000;
	for (int i = 0; i < count; ++i) {
		add(*f, present_key(i));
	}
	for (int i = 0; i < count; ++i) {
		// No false negatives, ever: a key that was added is always present.
		assert(contains(*f, present_key(i)) && "false negative: added key reported absent");
	}

	std::printf("bloom-filter no-false-negatives OK: %d added keys all present\n", count);
}


// ---------------------------------------------------------------------------
// The soft contract: clearly-absent keys are mostly absent. Probe a large set of
// keys that were never inserted and require the false-positive rate to stay
// under a loose bound. With the filter sized for its load this is comfortably
// met; the bound is deliberately slack so the test is about the contract, not a
// tight rate measurement.
// ---------------------------------------------------------------------------

static void test_false_positive_rate_loose() {
	constexpr size_t N = 8192;
	auto f = std::make_unique<BloomFilter<N>>();

	// Fill to the design load so the filter is exercised at its intended density.
	const int inserted = N;
	for (int i = 0; i < inserted; ++i) {
		add(*f, present_key(i));
	}

	// Probe a large disjoint set of never-added keys and count false positives.
	const int probes = 100000;
	int false_positives = 0;
	for (int i = 0; i < probes; ++i) {
		if (contains(*f, absent_key(i))) {
			++false_positives;
		}
	}

	const double rate = static_cast<double>(false_positives) / probes;
	// Loose bound: at the design load the true rate is ~1e-6, so well under 1%.
	// We assert < 1% to leave generous headroom and never flake.
	assert(rate < 0.01 && "false-positive rate exceeded the loose bound");

	std::printf("bloom-filter false-positive OK: %d/%d absent probes collided (rate %.6f < 0.01)\n",
		false_positives, probes, rate);
}


// ---------------------------------------------------------------------------
// Sizing: a larger N gives a larger bit array (m = N * 32) for the same number
// of inserts, so the false-positive rate at a fixed load is no worse, and the
// no-false-negative contract holds at the bigger size too. Exercise a second N
// to confirm the template parameter actually wires through.
// ---------------------------------------------------------------------------

static void test_sizing_parameter() {
	// Default N = 131072 -> m = 131072 * 32 bits = 512 KiB object on the heap.
	auto big = std::make_unique<BloomFilter<>>();

	const int count = 20000;
	for (int i = 0; i < count; ++i) {
		add(*big, present_key(i));
	}
	for (int i = 0; i < count; ++i) {
		assert(contains(*big, present_key(i)) && "false negative in default-sized filter");
	}

	// With 20k inserts in a filter designed for 131072, the load is light, so
	// false positives should be very rare. Hold to the same loose bound.
	const int probes = 100000;
	int false_positives = 0;
	for (int i = 0; i < probes; ++i) {
		if (contains(*big, absent_key(i))) {
			++false_positives;
		}
	}
	const double rate = static_cast<double>(false_positives) / probes;
	assert(rate < 0.01 && "lightly-loaded large filter should rarely collide");

	std::printf("bloom-filter sizing OK: default N=131072 holds %d keys, absent rate %.6f\n",
		count, rate);
}


// ---------------------------------------------------------------------------
// The salt parameter: it perturbs the second hash, so the same key under two
// different salts lands on different bit positions. A filter built under one
// salt must still answer no false negatives when queried under that same salt,
// and a key added under one salt is generally not seen under another. salt must
// be non-zero (the implementation asserts salt in hash()), so we never pass 0.
// ---------------------------------------------------------------------------

static void test_salt_parameter() {
	constexpr size_t N = 8192;
	auto f = std::make_unique<BloomFilter<N>>();

	const uint64_t salt_a = 1;          // the default
	const uint64_t salt_b = 0x9E3779B1; // an arbitrary non-zero salt

	const int count = 2000;
	for (int i = 0; i < count; ++i) {
		add(*f, present_key(i), salt_a);
	}

	// No false negatives under the salt the keys were added with.
	for (int i = 0; i < count; ++i) {
		assert(contains(*f, present_key(i), salt_a) && "false negative under matching salt");
	}

	// The same keys queried under a different salt hit different positions, so
	// most should miss. This is a soft property (a few may still collide), so we
	// only require that the bulk are not reported present under the wrong salt.
	int seen_under_b = 0;
	for (int i = 0; i < count; ++i) {
		if (contains(*f, present_key(i), salt_b)) {
			++seen_under_b;
		}
	}
	const double cross = static_cast<double>(seen_under_b) / count;
	assert(cross < 0.5 && "salt did not change the queried bit positions");

	std::printf("bloom-filter salt OK: matching-salt all present, cross-salt hit rate %.6f\n", cross);
}


int main() {
	test_no_false_negatives();
	test_false_positive_rate_loose();
	test_sizing_parameter();
	test_salt_parameter();
	std::printf("all bloom-filter tests passed\n");
	return 0;
}
