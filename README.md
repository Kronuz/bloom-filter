# bloom-filter

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/bloom-filter/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/bloom-filter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Header-only Bloom filter for C++20: fixed size, no false negatives, one false positive in a
million at design load. A key sets 20 bits in an array of `N × 32` bits; any clear bit
means the key is absent, and all set means it is probably present. Reach for it to skip a
disk read, a network call or a big-table lookup when the answer is no.

## ✨ Highlights

- **No false negatives:** a key that was added always reads back present.
- **Sized for one in a million:** `N × 32` bits and 20 probes target a false-positive
  rate of 1e-6 at `N` keys.
- **Fixed footprint:** `BloomFilter<N>` is a `4 × N`-byte object; the default
  `N = 131072` is 512 KiB.
- **Salted views:** a non-zero salt gives an independent filter over the same bits.
- **Three layouts, one API:** `BloomFilter<N>` hashes with xxh64 and a byte-at-a-time
  FNV-1a; `FastBloomFilter<N>` derives every probe from one xxh64 pass and leaves at the
  first group of four probes with a clear bit; `BlockedBloomFilter<N>` keeps a key's 20
  bits in one 64-byte cache line.
- **Hashing from [hashes](https://github.com/Kronuz/hashes)** (`xxh64`, `fnv1ah64`).

## 🚀 Quick start

```cpp
#include "bloom_filter.hh"

#include <cstdio>
#include <memory>
#include <string>

int main() {
	auto seen = std::make_unique<BloomFilter<8192>>();  // 32 KiB; large filters belong on the heap

	std::string key = "user-42";
	seen->add(key.data(), key.size());
	std::printf("%d\n", seen->contains(key.data(), key.size()));  // 1, always
	std::printf("%d\n", seen->contains("other", 5));              // 0, almost always
}
```

[`examples/usage.cc`](examples/usage.cc) is a runnable usage example.

## 📦 Installation

Requires C++20 and CMake 3.14 or newer. CMake fetches
[hashes](https://github.com/Kronuz/hashes) and its dependencies, all header-only. CI
builds and tests it on Linux (x86_64 with GCC and Clang, arm64), macOS (arm64, x86_64)
and FreeBSD, plus ASan, UBSan and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(bloom_filter
  GIT_REPOSITORY https://github.com/Kronuz/bloom-filter.git
  GIT_TAG        0c7af62f1c080382a9772845573b97893400fe75)  # or main, to track the tip
FetchContent_MakeAvailable(bloom_filter)

target_link_libraries(my_app PRIVATE bloom_filter::bloom_filter)
```

Link `bloom_filter::bloom_filter` and include `bloom_filter.hh`.

## 🎯 When to use it

- A cheap "definitely not there" in front of a disk read, a network call or a big table.
- `FastBloomFilter` for long keys or mostly-absent lookups; `BlockedBloomFilter` when one
  cache miss per lookup matters more than the false-positive rate.

Not for:

- Removing keys: a Bloom filter cannot delete without causing false negatives.
- A static key set built once: [fusefilter](https://github.com/Kronuz/fusefilter) answers
  the same question in fewer bits per key.
- Moving a persisted bitset between layouts: each type sets different bits for the same
  key.
- A false-positive rate near 1e-6 with `BlockedBloomFilter`: it measures about 1.3e-3 at
  `N` keys.

## ⚡ Performance

`bloom_bench` times one `contains` call in ns, the best of 7 passes over `N = 131072` keys of random
lowercase letters. The columns are this library's three types (`BloomFilter`, `FastBloomFilter`,
`BlockedBloomFilter`) and a comparator, [`boost::bloom::filter<std::uint64_t, 20>`](https://www.boost.org/doc/libs/release/libs/bloom/)
from Boost 1.92.0 with the same 32 bits per key, fed the key already hashed with the same
`xxh64`. The rows are the key length in bytes (16 or 64) and whether the key is in the filter
(`hit`) or not (`miss`). No filter loses a key (the benchmark counts false negatives); they differ
in which absent keys they report present, listed below the tables.

Apple M4 Pro, macOS 26.7:

| Lookup | `BloomFilter` | `FastBloomFilter` | `BlockedBloomFilter` | `boost::bloom` |
|---|---:|---:|---:|---:|
| 16 bytes, miss | 27.8 | 7.6 | 8.4 | 18.7 |
| 16 bytes, hit | 19.1 | 17.6 | 15.1 | 15.4 |
| 64 bytes, miss | 72.3 | 12.0 | 12.0 | 23.2 |
| 64 bytes, hit | 59.8 | 22.6 | 19.8 | 21.8 |

Intel Core i9-9980HK, macOS 15.8:

| Lookup | `BloomFilter` | `FastBloomFilter` | `BlockedBloomFilter` | `boost::bloom` |
|---|---:|---:|---:|---:|
| 16 bytes, miss | 35.6 | 18.2 | 18.2 | 23.0 |
| 16 bytes, hit | 39.1 | 39.8 | 33.5 | 37.5 |
| 64 bytes, miss | 89.6 | 31.3 | 26.7 | 39.9 |
| 64 bytes, hit | 96.3 | 60.2 | 42.7 | 58.2 |

Both machines use Apple clang 17.0.0 with `-O3 -DNDEBUG` (the bench target's flags). False
positives over 131072 absent keys: 0 for `BloomFilter` and `FastBloomFilter`, 172 (about 1.3e-3)
for `BlockedBloomFilter` with 16-byte keys, and 0 or 1 for `boost::bloom`.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLOOM_FILTER_BUILD_BENCH=ON
cmake --build build --target bloom_bench
./build/bloom_bench
```

## 📚 Documentation

- [docs/API.md](docs/API.md): `add`, `contains`, sizing and salts.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the probe positions are derived.
- [CHANGELOG.md](CHANGELOG.md): notable changes, with their measurements.
- [AGENTS.md](AGENTS.md): repository map and invariants.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#bloom-filter)
  in Kronuz C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Add `-DBLOOM_FILTER_BUILD_BENCH=ON` to build the benchmark.

## 🤝 Contributing

Issues and pull requests are welcome; read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first.

## 📄 License

MIT, see [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
