# bloom-filter

A small, header-only **Bloom filter** for C++20, extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One header, `bloom_filter.hh`: a `BloomFilter<N>` class template, a probabilistic
set that answers "have I seen this key?" in fixed space, with one tradeoff. It
never gives a false negative (if it says a key is absent, the key was definitely
never added), and it gives a false positive only rarely (if it says present, the
key is *probably* there, but might not be). That is the Bloom filter bargain: you
trade a small, tunable false-positive rate for a fixed memory footprint that does
not grow with the data, and you can never remove a key.

It is what you reach for when you want a cheap membership pre-check in front of an
expensive lookup. A "no" from the filter lets you skip the expensive path
outright; a "yes" means you still go do the real lookup, but those are the only
times you pay for it. Xapiand uses it exactly that way.

The template parameter `N` is the expected number of elements. The filter sizes
its bit array to `m = N * 32` bits and probes it with `k = 20` hash positions,
which targets a false-positive rate around `1e-6` when about `N` keys have been
added. Push well past `N` and the rate climbs; stay at or under it and it holds.
The default is `N = 131072` (a 512 KiB filter).

## Install

CMake with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  bloom_filter
  GIT_REPOSITORY https://github.com/Kronuz/bloom-filter.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(bloom_filter)

target_link_libraries(your_target PRIVATE bloom_filter::bloom_filter)
```

The `bloom_filter` target is a pure `INTERFACE` library: it compiles nothing,
requests `cxx_std_20`, and puts the header directory on your include path. It has
one dependency, the sibling [hashes](https://github.com/Kronuz/hashes) library,
which provides the `xxh64` and `fnv1ah64` hash families the filter derives its
probe positions from. CMake pulls it in for you (and `hashes` in turn pulls its
own deps), so `"hashes.hh"` resolves with no extra wiring on your side. Then:

```cpp
#include "bloom_filter.hh"
```

The header keeps its original filename, so a codebase that already
`#include "bloom_filter.hh"` just needs this repo on its include path.

## Usage

```cpp
#include "bloom_filter.hh"

// Size the filter for the load you expect. Default is N = 131072.
BloomFilter<8192> seen;

// add() and contains() take a byte buffer: pointer + length.
std::string key = "user-42";
seen.add(key.data(), key.size());

// A key that was added is *always* reported present: no false negatives.
seen.contains(key.data(), key.size());            // true

// A key that was never added is *usually* reported absent, but a rare
// collision can return true. Treat a true as "probably present, go check".
seen.contains("never-added", 11);                 // almost always false

// String literals work directly (mind the trailing NUL: pass the length you
// want; sizeof - 1 drops it).
seen.add("hello", 5);

// The optional salt perturbs the hashing, giving you an independent filter view
// over the same bits. It must be non-zero.
seen.add(key.data(), key.size(), 0x9E3779B1);
seen.contains(key.data(), key.size(), 0x9E3779B1); // true under the same salt
```

There is no `remove`: a Bloom filter cannot delete a key without risking false
negatives for others that share its bits. When the filter fills up, build a fresh
one.

## API reference

```cpp
template <size_t N = 131072>
class BloomFilter {
public:
    void add(const char* data, size_t len, uint64_t salt = 1);
    bool contains(const char* data, size_t len, uint64_t salt = 1) const;
};
```

- **`N`** (template, default `131072`) — the expected element count. It sets the
  bit-array size to `m = N * 32` bits; the probe count is fixed at `k = 20`. The
  whole bit array is an inline `std::bitset<m>` member, so a `BloomFilter<N>` is a
  `4 * N`-byte object. The default is 512 KiB; heap-allocate large ones rather
  than putting them on the stack.
- **`add(data, len, salt)`** — hashes the `len`-byte buffer at `data` and sets its
  `k` bit positions. Adding the same key twice is a no-op on the bits.
- **`contains(data, len, salt)`** — returns `true` if all `k` bit positions for
  the key are set. `false` is definitive (the key was never added); `true` is
  probabilistic (present, or a rare collision).
- **`salt`** (default `1`) — perturbs the second hash so the same key maps to a
  different set of positions. **Must be non-zero** (`hash()` asserts it). Use the
  same salt for `add` and `contains`; different salts give independent views.

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `hashes` (and its deps) over the network. The test
asserts the hard contract (every added key reads back present, no false
negatives), holds the false-positive rate under a loose bound over 100k absent
probes at the design load, exercises two filter sizes including the default, and
checks that the salt parameter actually changes the queried bit positions. It
prints `all bloom-filter tests passed` and exits 0.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `bloom_filter.hh`
was copied verbatim; the only change on extraction was wiring its one local
include, `"hashes.hh"`, to the standalone
[hashes](https://github.com/Kronuz/hashes) library through CMake. No source
edits, no decoupling delta. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design
and [AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2018,2019 Dubalu LLC. See [LICENSE](LICENSE).
