# bloom-filter

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/bloom-filter/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/bloom-filter/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

A fixed-size Bloom filter for C++20: answer "have I seen this key?" with no false
negatives and about one false positive in a million, in memory that does not
grow with the data. For skipping expensive lookups when the answer is no.

![bloom-filter: a key hashes to bit positions; any clear bit means absent, all set means probably present](docs/bloom-filter.svg)

## ✨ Highlights

- **No false negatives.** A key that was added always reads back present.
- **About 1e-6 false positives** at the design load: `N × 32` bits and 20 probes.
- **Fixed footprint.** `BloomFilter<N>` is a `4 × N`-byte object; the default
  `N = 131072` is 512 KiB.
- **Salted views.** A non-zero salt gives an independent filter over the same
  bits.
- **One header**, hashing with [hashes](https://github.com/Kronuz/hashes)'
  `xxh64` and `fnv1ah64`.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it fetches hashes. CI builds it
on Linux (x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus ASan, UBSan,
and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(bloom_filter
  GIT_REPOSITORY https://github.com/Kronuz/bloom-filter.git
  GIT_TAG        main)
FetchContent_MakeAvailable(bloom_filter)

target_link_libraries(my_app PRIVATE bloom_filter::bloom_filter)
```

## 🚀 Quick start

```cpp
#include "bloom_filter.hh"

#include <memory>
#include <string>

int main() {
	auto seen = std::make_unique<BloomFilter<8192>>();   // 32 KiB; large ones belong on the heap

	std::string key = "user-42";
	seen->add(key.data(), key.size());
	bool hit = seen->contains(key.data(), key.size());   // true, always
	bool miss = seen->contains("other", 5);              // false, almost always
}
```

[`examples/usage.cc`](examples/usage.cc) is a runnable usage example.

## 🎯 When to use it

- ✅ A cheap "definitely not there" in front of a disk read, a network call, or a
  big table.
- ❌ Removing keys: a Bloom filter cannot delete without causing false negatives.
- ❌ A static key set built once: [fusefilter](https://github.com/Kronuz/fusefilter)
  answers the same question in fewer bits per key.

## ⚡ Faster layouts (opt-in)

`BloomFilter<N>` keeps its original bit layout: it runs an xxh64 pass and a
byte-at-a-time FNV-1a pass, so a lookup costs ~0.9 ns per key byte on top of 20
probes. `FastBloomFilter<N>` and `BlockedBloomFilter<N>` have the same API and
size but derive every probe from one xxh64 pass, so key length no longer
multiplies the cost. They set different bits than `BloomFilter<N>`: never load
a bitset persisted from one type into another.

Apple arm64, ns per op (best of 7; 16-byte keys, N=131072, miss): legacy 26.7,
Fast 17.4, Blocked 14.7, boost::bloom (pre-hashed key plus our xxh64) 17.0. With
64-byte keys: legacy 67.3, Fast 21.8, Blocked 19.1, boost 22.6. `Fast` has the
same false-positive rate as the legacy type (0 of 131072 absent keys); `Blocked`
confines each key to one cache line and pays ~1.3e-3 (172 of 131072). Run
`bloom_bench` to reproduce.

## 📚 Documentation

- [docs/API.md](docs/API.md): `add`, `contains`, sizing, and salts.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the probe positions are derived.
- [AGENTS.md](AGENTS.md): repository map and invariants.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) first. CI must
pass on every platform.

## 📄 License

MIT, Copyright (c) 2018, 2019 Dubalu LLC. See [LICENSE](LICENSE). Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).
