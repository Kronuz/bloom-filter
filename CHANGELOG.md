# Changelog

All notable changes to `bloom-filter`. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no
releases, so entries are dated.

## 2026-11-01

### Changed

- `bloom_bench` builds only with `-DBLOOM_FILTER_BUILD_BENCH=ON` (it was a target left out
  of the default build and built by name). `boost::bloom` is found by that target alone.

## 2026-10-31

### Changed

- `FastBloomFilter<N>::contains` and `BlockedBloomFilter<N>::contains` test their
  20 probes in groups of four (`bloom_detail::probe_group`) and leave at the first
  group that has a clear bit. The probes, the bits `add` sets and every answer are
  unchanged (frozen-copy tests); a miss at the design load reads about 4 words
  instead of 20. The 2026-10-30 form tested all 20 probes and branched once, which
  lost to `BloomFilter<N>`'s early exit on x86 (the audit measured a miss at 16 bytes
  at 38 ns against 35; finding CA7).

### Fixed

- `bloom_detail::reduce` used `unsigned __int128` unconditionally, which every
  translation unit including the header compiled. GCC's `-Wpedantic -Werror` rejected
  it, and so did MSVC and 32-bit targets (the audit finding CA8). The type is used
  only where `__SIZEOF_INT128__` is defined, through an `__extension__` typedef;
  elsewhere `bloom_detail::mulhi_portable` computes the same high word from four
  32-bit multiplies. `BLOOM_FILTER_NO_INT128` selects it on any compiler. Results
  are identical.

### Added

- Tests: the portable multiply against a schoolbook oracle, `FastBloomFilter` and
  `BlockedBloomFilter` against frozen copies (power-of-two and other sizes, one
  word or block, the design load and three times it), the words a miss reads
  (`BLOOM_FILTER_COUNT_PROBES`), the whole suite built with `BLOOM_FILTER_NO_INT128`
  (`bloom_filter_portable`), and the header under `-Wall -Wextra -Wpedantic -Werror`
  (`bloom_filter_pedantic`).

### Performance

ns per lookup, `N = 131072`, best of 7, two passes of each build (the spread between
passes, and between builds of the unchanged `BloomFilter`, is up to 4% on arm64 and 12% on the i9).
Apple M4 Pro (arm64), Apple clang 17:

| row | before (`2026-10-30`) | after | `BloomFilter` | `boost::bloom` |
|---|---|---|---|---|
| `FastBloomFilter`, 16-byte keys, miss | 18.1 | 8.0 | 27.7 | 18.6 |
| `BlockedBloomFilter`, 16-byte keys, miss | 15.4 | 8.5 | 27.7 | 18.6 |
| `FastBloomFilter`, 64-byte keys, miss | 23.1 | 11.9 | 73.4 | 23.4 |
| `BlockedBloomFilter`, 64-byte keys, miss | 20.4 | 12.1 | 73.4 | 23.4 |

Intel i9-9980HK (x86_64), Apple clang 17:

| row | before | after | `BloomFilter` | `boost::bloom` |
|---|---|---|---|---|
| `FastBloomFilter`, 16-byte keys, miss | 45.2 | 20.6 | 36.0 | 23.4 |
| `BlockedBloomFilter`, 16-byte keys, miss | 33.4 | 18.4 | 36.0 | 23.4 |
| `FastBloomFilter`, 64-byte keys, miss | 64.4 | 31.7 | 96.1 | 37.1 |
| `BlockedBloomFilter`, 64-byte keys, miss | 43.5 | 31.1 | 96.1 | 37.1 |

Intel i9-9980HK, GCC 16:

| row | before | after | `BloomFilter` | `boost::bloom` |
|---|---|---|---|---|
| `FastBloomFilter`, 16-byte keys, miss | 48.1 | 23.2 | 33.5 | 27.6 |
| `BlockedBloomFilter`, 16-byte keys, miss | 35.9 | 17.3 | 33.5 | 27.6 |
| `FastBloomFilter`, 64-byte keys, miss | 66.3 | 32.6 | 81.9 | 42.2 |
| `BlockedBloomFilter`, 64-byte keys, miss | 47.6 | 27.6 | 81.9 | 42.2 |

Hit and add lookups of both types are unchanged within that spread on every row.

## 2026-10-30

### Added

- `FastBloomFilter<N>` and `BlockedBloomFilter<N>`: opt-in types with the same
  API, size (`N * 32` bits) and `k = 20` as `BloomFilter<N>`. They derive every
  probe from one xxh64 pass instead of xxh64 plus a byte-at-a-time FNV-1a pass,
  so lookup cost stops growing with key length. They set different bits than
  `BloomFilter<N>`, so persisted bitsets are not interchangeable (`c32355b`).
- A differential test that checks `BloomFilter<N>` against a frozen copy, tests for
  the new types, and `bloom_bench`, a benchmark against `boost::bloom` (bench
  target only).

### Unchanged

- `BloomFilter<N>` keeps its bits and hashing.

### Performance

Apple arm64, ns per miss lookup, best of 7, `N = 131072`:

| key | `BloomFilter` | `FastBloomFilter` | `BlockedBloomFilter` | `boost::bloom` |
|---|---|---|---|---|
| 16 bytes | 26.7 | 17.4 | 14.7 | 17.0 |
| 64 bytes | 67.3 | 21.8 | 19.1 | 22.6 |

`FastBloomFilter` matched `BloomFilter`'s false positives (0 of 131072 absent
keys); `BlockedBloomFilter` measured about 1.3e-3 (172 of 131072).
