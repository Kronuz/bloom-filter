# Changelog

All notable changes to `bloom-filter`. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no
releases, so entries are dated.

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
