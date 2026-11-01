# Changelog

All notable changes to `bloom-filter`. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no
releases, so entries are dated.

## 2026-10-31

### Fixed

- `bloom_detail::reduce` used `unsigned __int128` unconditionally, which every
  translation unit including the header compiled. GCC's `-Wpedantic -Werror` rejected
  it, and so did MSVC and 32-bit targets (the audit finding CA8). The type is used
  only where `__SIZEOF_INT128__` is defined, through an `__extension__` typedef;
  elsewhere `bloom_detail::mulhi_portable` computes the same high word from four
  32-bit multiplies. `BLOOM_FILTER_NO_INT128` selects it on any compiler. Results
  are identical.

### Added

- Tests: the portable multiply against a schoolbook oracle, the whole suite built with
  `BLOOM_FILTER_NO_INT128` (`bloom_filter_portable`), and the header under
  `-Wall -Wextra -Wpedantic -Werror` (`bloom_filter_pedantic`).

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
