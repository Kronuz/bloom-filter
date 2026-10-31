# bloom-filter API reference

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

`FastBloomFilter<N>` and `BlockedBloomFilter<N>` have the same template
parameter, members, size and salt rules. They hash each key once, so their cost
does not grow with key length. `BlockedBloomFilter<N>` keeps each key in one
64-byte block and has a higher false-positive rate (about 1.3e-3 at `N` keys);
it requires `N * 32` to be a multiple of 512. The three types set different
bits for the same key, so their bitsets are not interchangeable.

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
