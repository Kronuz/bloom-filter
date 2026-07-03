/*
 * Copyright (c) 2018,2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * usage: a runnable, self-checking BloomFilter example.
 *
 * Xapiand no longer has in-tree BloomFilter call sites, so this example pins the
 * core guarantees a consumer relies on:
 *   - an empty filter reports sampled absent keys as absent,
 *   - every inserted key is found again, with no false negatives,
 *   - a filter sized for N items still holds at N inserts,
 *   - the measured false-positive rate over a large, deterministic absent set
 *     stays within a loose bound of the configured target.
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "bloom_filter.hh"


static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)


template <typename Filter>
static void add(Filter& filter, const std::string& key, uint64_t salt = 1) {
	filter.add(key.data(), key.size(), salt);
}


template <typename Filter>
static bool contains(const Filter& filter, const std::string& key, uint64_t salt = 1) {
	return filter.contains(key.data(), key.size(), salt);
}


static std::vector<std::string> make_keys(const char* prefix, size_t count) {
	std::mt19937_64 rng(0xC0FFEEULL);
	std::vector<std::string> keys;
	keys.reserve(count);

	for (size_t i = 0; i < count; ++i) {
		keys.push_back(std::string(prefix) + ":" + std::to_string(i) + ":" + std::to_string(rng()));
	}

	return keys;
}


int main() {
	constexpr size_t expected_insertions = 8192;
	constexpr size_t absent_probes = 1000000;
	constexpr double configured_false_positive_rate = 0.000001;
	constexpr double max_measured_false_positive_rate = configured_false_positive_rate * 3.0;

	auto filter = std::make_unique<BloomFilter<expected_insertions>>();
	const auto inserted = make_keys("inserted", expected_insertions);
	const auto absent = make_keys("absent", absent_probes);

	// Empty filter: before any bits are set, every sampled key must be absent.
	CHECK(!contains(*filter, std::string{}));
	size_t empty_hits = 0;
	for (const auto& key : absent) {
		if (contains(*filter, key)) {
			++empty_hits;
		}
	}
	CHECK(empty_hits == 0);

	// Sizing: fill exactly to N, the load the type was configured for.
	for (const auto& key : inserted) {
		add(*filter, key);
	}

	// The defining hard guarantee: no false negatives.
	for (const auto& key : inserted) {
		CHECK(contains(*filter, key));
	}

	// The soft guarantee: absent keys mostly miss. The RNG seed and disjoint
	// prefixes make the sample deterministic, so this does not flake.
	size_t false_positives = 0;
	for (const auto& key : absent) {
		if (contains(*filter, key)) {
			++false_positives;
		}
	}

	const double measured_false_positive_rate = static_cast<double>(false_positives) / static_cast<double>(absent.size());
	std::printf("bloom-filter usage: measured %zu/%zu false positives, rate %.8f (bound %.8f)\n",
		false_positives,
		absent.size(),
		measured_false_positive_rate,
		max_measured_false_positive_rate);
	CHECK(measured_false_positive_rate <= max_measured_false_positive_rate);

	if (failures == 0) {
		std::printf("bloom-filter usage: all checks passed (%zu inserts)\n", inserted.size());
	}

	return failures == 0 ? 0 : 1;
}
