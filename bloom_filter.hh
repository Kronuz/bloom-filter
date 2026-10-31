/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#pragma once

#include <array>          // for std::array
#include <cassert>        // for assert
#include <cstdint>        // for uint64_t
#include <bitset>         // for std::bitset
#include <utility>        // for std::make_pair

#include "hashes.hh"      // for xxh64::hash, fnv1ah64::hash


template <size_t N=131072>
class BloomFilter {
	// [http://blog.michaelschmatz.com/2016/04/11/how-to-write-a-bloom-filter-cpp/]
	// [http://citeseer.ist.psu.edu/viewdoc/download;jsessionid=4060353E67A356EF9528D2C57C064F5A?doi=10.1.1.152.579&rep=rep1&type=pdf]
	// P = 0.000001
	// k = -ln(P) / ln(2)
	// k = 13.815510558 / 0.6931471806
	// k = 19.9315685693
	// k = 20
	// m = N * k / ln(2)
	// m = N * 19.9315685693 / 0.6931471806
	// m = N * 28.7551751321
	// m = N * 32
	// if N = 8192
	//    m = 8192 * 32
	//    m = 262,144 bits => 32,768 bytes => 32K
	// if N = 131072
	//    m = 131,072 * 32
	//    m = 4,194,304 bits => 524,288 bytes => 512K
	// if N = 262144
	//    m = 262,144 * 32
	//    m = 8,388,608 bits => 1,048,576 bytes => 1M
	static constexpr size_t k = 20;
	static constexpr size_t m = N * 32;
	std::bitset<m> bits;

	auto hash(const char* data, size_t len, uint64_t salt) const {
		assert(salt);
		return std::make_pair(
			xxh64::hash(data, len),
			fnv1ah64::hash(data, len) * salt
		);
	}

public:
	void add(const char* data, size_t len, uint64_t salt = 1) {
		auto hashes = hash(data, len, salt);
		for (auto n = k; n; --n) {
			bits[(hashes.first + n * hashes.second) % m] = true;
		}
	}

	bool contains(const char* data, size_t len, uint64_t salt = 1) const {
		auto hashes = hash(data, len, salt);
		for (auto n = k; n; --n) {
			if (!bits[(hashes.first + n * hashes.second) % m]) {
				return false;
			}
		}
		return true;
	}
};


// FastBloomFilter<N> and BlockedBloomFilter<N>: opt-in alternatives to BloomFilter<N>
// with the same API, the same size (N * 32 bits) and k = 20 probes, but a different
// bit layout. They are NOT bit-compatible with BloomFilter<N>: the same key sets
// different bits, so a bitset persisted from one type must not be loaded into
// another. BloomFilter<N> itself is unchanged.
//
// Hashing: one xxh64 pass over the key gives h. Everything else is derived from h
// and the salt with a few multiplies (a splitmix-style finalizer), so the cost no
// longer grows with key length: the legacy filter runs a second, byte-at-a-time
// FNV-1a pass, a dependent multiply chain of ~0.7 ns per key byte.
//
//  * FastBloomFilter<N>: flat bitset, k probes at h + n * d (double hashing, d odd
//    and decorrelated from h by the finalizer), mapped to [0, m) with a
//    multiply-high range reduction. False-positive rate matches BloomFilter<N>
//    (~1e-6 at N keys). contains() has no early exit: the k loads are independent
//    and overlap, and one final branch replaces a mispredict per probe.
//  * BlockedBloomFilter<N>: h picks one 64-byte block (one cache line), and all k
//    bits are set inside it, so a probe costs one cache miss instead of up to k.
//    The price is a higher false-positive rate at the same size (the classic
//    blocked-filter trade-off; see the benchmark), so choose it for speed on
//    filters larger than the cache.
namespace bloom_detail {
	inline constexpr uint64_t mix64(uint64_t x) noexcept {
		x ^= x >> 32;
		x *= 0xd6e8feb86659fd93ULL;
		x ^= x >> 32;
		x *= 0xd6e8feb86659fd93ULL;
		x ^= x >> 32;
		return x;
	}
	// floor(x * range / 2^64): a range reduction with no division.
	inline uint64_t reduce(uint64_t x, uint64_t range) noexcept {
		return static_cast<uint64_t>((static_cast<unsigned __int128>(x) * range) >> 64);
	}
}

template <size_t N=131072>
class FastBloomFilter {
	static constexpr size_t k = 20;
	static constexpr size_t m = N * 32;
	static_assert(m % 64 == 0, "N * 32 must be a multiple of 64");
	alignas(64) std::array<uint64_t, m / 64> words{};

public:
	void add(const char* data, size_t len, uint64_t salt = 1) {
		assert(salt);
		uint64_t h = xxh64::hash(data, len);
		uint64_t d = bloom_detail::mix64(h ^ (salt * 0x9E3779B97F4A7C15ULL)) | 1;
		for (size_t n = k; n; --n) {
			h += d;
			auto bit = bloom_detail::reduce(h, m);
			words[bit >> 6] |= uint64_t(1) << (bit & 63);
		}
	}

	bool contains(const char* data, size_t len, uint64_t salt = 1) const {
		assert(salt);
		uint64_t h = xxh64::hash(data, len);
		uint64_t d = bloom_detail::mix64(h ^ (salt * 0x9E3779B97F4A7C15ULL)) | 1;
		uint64_t miss = 0;
		for (size_t n = k; n; --n) {
			h += d;
			auto bit = bloom_detail::reduce(h, m);
			miss |= ~words[bit >> 6] & (uint64_t(1) << (bit & 63));
		}
		return miss == 0;
	}
};

template <size_t N=131072>
class BlockedBloomFilter {
	static constexpr size_t k = 20;
	static constexpr size_t m = N * 32;
	static constexpr size_t block_words = 8;  // 512 bits
	static constexpr size_t nblocks = m / 512;
	static_assert(m % 512 == 0 && m >= 512, "N * 32 must be a multiple of 512");
	alignas(64) std::array<uint64_t, m / 64> words{};

	// The k bit positions inside a block come from the top 9 bits of x = n * d.
	const uint64_t* block_of(uint64_t h) const { return &words[bloom_detail::reduce(h, nblocks) * block_words]; }

public:
	void add(const char* data, size_t len, uint64_t salt = 1) {
		assert(salt);
		uint64_t h = xxh64::hash(data, len);
		uint64_t* block = const_cast<uint64_t*>(block_of(h));
		uint64_t d = bloom_detail::mix64(h ^ (salt * 0x9E3779B97F4A7C15ULL)) | 1;
		uint64_t x = 0;
		for (size_t n = k; n; --n) {
			x += d;
			auto bit = x >> 55;  // 0..511
			block[bit >> 6] |= uint64_t(1) << (bit & 63);
		}
	}

	bool contains(const char* data, size_t len, uint64_t salt = 1) const {
		assert(salt);
		uint64_t h = xxh64::hash(data, len);
		const uint64_t* block = block_of(h);
		uint64_t d = bloom_detail::mix64(h ^ (salt * 0x9E3779B97F4A7C15ULL)) | 1;
		uint64_t x = 0;
		uint64_t miss = 0;
		for (size_t n = k; n; --n) {
			x += d;
			auto bit = x >> 55;
			miss |= ~block[bit >> 6] & (uint64_t(1) << (bit & 63));
		}
		return miss == 0;
	}
};
