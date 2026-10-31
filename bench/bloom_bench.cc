// Benchmark: BloomFilter (legacy layout) vs FastBloomFilter vs BlockedBloomFilter,
// and boost::bloom::filter when Boost >= 1.89 is found (comparator only).
// ns per operation, best of several runs; false-positive counts are printed
// because a different layout has different false positives by design.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "bloom_filter.hh"
#ifdef HAVE_BOOST_BLOOM
#include <boost/bloom/filter.hpp>
#endif

static volatile size_t sink;

template <class F>
static double best_ns(F&& f, double ops, int reps = 7) {
	f();
	double best = 1e18;
	for (int i = 0; i < reps; ++i) {
		auto a = std::chrono::steady_clock::now();
		f();
		auto b = std::chrono::steady_clock::now();
		best = std::min(best, std::chrono::duration<double, std::nano>(b - a).count() / ops);
	}
	return best;
}

template <size_t N>
static void run(size_t keylen) {
	std::mt19937_64 rng(5);
	std::vector<std::string> in(N), out(N);
	auto mk = [&](std::string& s) { s.resize(keylen); for (auto& c : s) c = 'a' + rng() % 26; };
	for (auto& s : in) mk(s);
	for (auto& s : out) mk(s);
	std::printf("N=%zu keylen=%zu\n", N, keylen);

	auto bench = [&](const char* name, auto make, auto add, auto has) {
		auto f = make();
		double t_add = best_ns([&] { auto g = make(); for (auto& s : in) add(*g, s); sink = 1; }, N);
		for (auto& s : in) add(*f, s);
		size_t fn = 0, fp = 0;
		for (auto& s : in) fn += !has(*f, s);
		for (auto& s : out) fp += has(*f, s);
		double t_hit = best_ns([&] { size_t c = 0; for (auto& s : in) c += has(*f, s); sink = c; }, N);
		double t_miss = best_ns([&] { size_t c = 0; for (auto& s : out) c += has(*f, s); sink = c; }, N);
		std::printf("  %-22s add %7.2f  hit %7.2f  miss %7.2f ns   false-neg %zu  false-pos %zu/%zu\n",
		            name, t_add, t_hit, t_miss, fn, fp, N);
	};
	auto strAdd = [](auto& f, const std::string& s) { f.add(s.data(), s.size()); };
	auto strHas = [](auto& f, const std::string& s) { return f.contains(s.data(), s.size()); };
	bench("BloomFilter (legacy)", [] { return std::make_unique<BloomFilter<N>>(); }, strAdd, strHas);
	bench("FastBloomFilter", [] { return std::make_unique<FastBloomFilter<N>>(); }, strAdd, strHas);
	bench("BlockedBloomFilter", [] { return std::make_unique<BlockedBloomFilter<N>>(); }, strAdd, strHas);
#ifdef HAVE_BOOST_BLOOM
	// boost takes a pre-hashed integer: hash once with xxh64 so the key hashing cost is comparable.
	using BB = boost::bloom::filter<std::uint64_t, 20>;
	bench("boost::bloom<u64,20>", [] { return std::make_unique<BB>(N * 32); },
	      [](auto& f, const std::string& s) { f.insert(xxh64::hash(s.data(), s.size())); },
	      [](auto& f, const std::string& s) { return f.may_contain(xxh64::hash(s.data(), s.size())); });
#endif
}

int main() {
	run<8192>(16);
	run<131072>(16);
	run<131072>(64);
}
