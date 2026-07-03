# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
bloom_filter.hh              BloomFilter<N>: a std::bitset<N*32> with add()/contains(), double-hashed via xxh64 + fnv1ah64. Header. Verbatim from Xapiand.
test/test.cc                 Runnable smoke test: no false negatives, loose false-positive bound, two sizes, the salt parameter.
examples/usage.cc            Runnable usage example, self-checking under CTest, covering empty filters, N inserts, no false negatives, and false-positive rate.
CMakeLists.txt               INTERFACE library `bloom_filter` (+ alias bloom_filter::bloom_filter); FetchContent hashes; CTest tests `bloom_filter` and `bloom_filter_usage`.
LICENSE                      MIT, Copyright (c) 2018,2019 Dubalu LLC.
README.md                    What it is, install, usage, API.
ARCHITECTURE.md              Internal design: the m/k sizing, double hashing, the salt, where the guarantees come from.
```

Everything is header-only. The `.cc` files are runnable checks, not library
sources. The CMake target is a pure `INTERFACE` library that adds the source dir to
the include path, requests `cxx_std_20`, and links `hashes` so `"hashes.hh"`
resolves.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `hashes` (and its transitive deps, static-string and
char-classify) over the network (FetchContent, `GIT_TAG main`). Expected output
ends with both CTest entries passing. The test targets are `bloom_filter_test` and
`bloom_filter_usage`; the registered CTest names are `bloom_filter` and
`bloom_filter_usage`. They are only added when this repo is the top-level project
(CMakeLists.txt), so consumers vendoring it via `FetchContent` won't build them.

## Dependency

`bloom_filter.hh` includes `"hashes.hh"` for two symbols, `xxh64::hash` and
`fnv1ah64::hash`, the two base hashes it double-hashes into `k` probe positions.
That header lives in the sibling [hashes](https://github.com/Kronuz/hashes)
library, pulled in by CMake and linked through the `hashes::hashes` target so the
include path resolves. We track its tip with `GIT_TAG main`, like the rest of the
family. This is the library's only direct dependency; there was no `log.h` / opts
/ config coupling in the original.

## Conventions

- **C++20.** The target requests `cxx_std_20` to stay uniform with the sibling
  libraries. Don't drop the target below it.
- **Filename is stable.** The header keeps its original Xapiand name
  (`bloom_filter.hh`) so a consumer that already `#include`s it just needs this
  repo on the include path. Don't rename it.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header (Copyright (c) 2018,2019 Dubalu LLC) on
  the source file.

## Load-bearing invariants

- **No false negatives, ever.** This is the entire point of a Bloom filter. `add`
  only sets bits and `contains` only reads them; nothing clears a bit. A key that
  was added must always read back present. Any change that can unset a bit (a
  `remove`, a "reset stale entries", a counting variant that decrements) breaks
  this for *other* keys that share those bits. Don't add one. To forget keys,
  build a fresh filter.
- **`salt` must be non-zero.** `hash()` asserts it. A zero salt zeroes the second
  hash, collapsing all `k` probe positions to `h1 % m` and destroying the spread.
  Keep the assert; keep the default of `1`.
- **`m = N * 32` and `k = 20` are a matched pair.** They target a ~`1e-6`
  false-positive rate at the design load via the standard Bloom formulas (see
  ARCHITECTURE.md). Don't change one without the other, or you move the rate
  without meaning to.
- **The bit array is an inline `std::bitset<m>` member.** A `BloomFilter<N>` is a
  `4 * N`-byte object with the size baked into the type. The default `N = 131072`
  is 512 KiB. Heap-allocate large filters; the test does (`std::make_unique`)
  precisely because even a modest `N` overflows the stack.

## How to extend

- **Change the target rate.** It is encoded in `k` and the `m` multiplier, derived
  in the header's comment from `P = 1e-6`. To retarget, redo both formulas
  (`k = -ln(P)/ln(2)`, `m = N*k/ln(2)`) and update the constants and the comment
  together. Don't touch just one.
- **Always extend the executable checks.** Keep the no-false-negative assertions
  unconditional (they must hold exactly), and keep any false-positive bound loose
  enough that it checks the contract without flaking on the exact rate.

## Traps

- **`add`/`contains` are size-based, not NUL-terminated.** They take
  `(const char* data, size_t len)`. For a string literal, pass the length you mean
  (`sizeof - 1` to drop the trailing NUL); for a `std::string`, pass
  `.data()`/`.size()`. Mismatched lengths hash to different keys.
- **A `true` from `contains` is "probably", not "definitely".** Callers must treat
  it as a pre-check and still do the real lookup. Only `false` is definitive. Don't
  write code that trusts a `true` as proof of membership.
- **Same salt for add and query.** A key added under one salt is generally not
  found under another (the test relies on this). Mixing salts silently looks like
  a missing key.
- **Don't put a large filter on the stack.** It is an inline bitset; `BloomFilter<>`
  on the stack is a 512 KiB local. Heap-allocate.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `bloom_filter.hh` was copied
verbatim; the only change on extraction was resolving the one local include,
`"hashes.hh"`, against the standalone hashes library through CMake instead of
Xapiand's source tree. There is no source decoupling delta. Keep it that way; any
change here should be reconcilable with upstream as a plain edit.
