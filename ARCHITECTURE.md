# Architecture

The internals of `bloom_filter`: how a fixed bit array plus a handful of hash
probes gives a probabilistic set with no false negatives, why the sizing
constants are what they are, and where the false-positive rate comes from. For
usage see `README.md`; for the repo map and invariants see `AGENTS.md`.

## Shape

One header, header-only, one external dependency:

```
  bloom_filter.hh   BloomFilter<N>: a std::bitset<N*32> plus add()/contains()
```

The class template `BloomFilter<N>` holds a single `std::bitset<m>` member and
nothing else. `add` and `contains` are the whole surface. The hashing comes from
the sibling [hashes](https://github.com/Kronuz/hashes) library (`xxh64::hash` and
`fnv1ah64::hash`), which is the only reason this is not dependency-free.

## The two sizing constants

A Bloom filter is defined by two numbers: `m`, the number of bits, and `k`, the
number of hash probes per key. The header fixes both from the template parameter
`N`, the expected element count, and a target false-positive rate `P`.

The standard Bloom filter formulas, with the chosen `P = 1e-6`, give:

```
  k = -ln(P) / ln(2)  ~= 19.93  ->  k = 20      probes per key
  m = N * k / ln(2)   ~= N * 28.76  ->  m = N * 32   bits, rounded up
```

So `k = 20` is fixed, and `m = N * 32` scales with the expected load. Rounding
`m` up from `~28.76 * N` to `32 * N` (a power-of-two multiplier) buys a little
headroom on the rate and keeps the arithmetic clean. The header spells the
derivation out in a comment, with worked sizes: `N = 8192` is a 32 KiB filter,
the default `N = 131072` is 512 KiB, `N = 262144` is 1 MiB.

The bit array is an inline `std::bitset<m>` member, not a heap allocation, so the
size is baked into the type and a `BloomFilter<N>` is exactly `4 * N` bytes. That
is why large filters want to live on the heap.

## Two hashes, k positions

For each key the filter needs `k` independent-looking bit positions. Computing 20
real hashes per key would be expensive, so it uses the standard double-hashing
trick: take two base hashes and combine them linearly to manufacture the rest.

`hash()` computes the pair once:

```cpp
auto hash(const char* data, size_t len, uint64_t salt) const {
    return std::make_pair(
        xxh64::hash(data, len),               // h1
        fnv1ah64::hash(data, len) * salt      // h2, perturbed by salt
    );
}
```

Then `add` and `contains` both walk `n = k .. 1` and form position
`(h1 + n * h2) % m` for each `n`. `add` sets those bits; `contains` returns false
the moment one of them is clear, and true only if all `k` are set. The two hashes
come from different families (xxHash and FNV-1a) so they are effectively
independent, which is what makes the manufactured positions behave.

## The salt

`salt` (default `1`) multiplies the second hash, so the same key under a
different salt produces a different `h2` and therefore a different set of `k`
positions. That gives an independent view over the same bits: useful for keeping
several logical filters in one array, or for re-hashing without changing the key.
`hash()` asserts `salt` is non-zero, because a zero salt would collapse `h2` to
zero and make every probe position just `h1 % m`, destroying the spread. Pass the
same salt to `add` and `contains` or the query lands on the wrong bits.

## Where the guarantees come from

The no-false-negative guarantee is structural: `add` sets bits and nothing ever
clears them, so once a key's `k` bits are set, any later `contains` for that key
finds all of them set and returns true. There is no code path that unsets a bit,
which is also why there is no `remove`: clearing a key's bits could clear bits a
*different* key relies on, and then that other key would read absent, breaking the
guarantee for it.

The false positive is the price. An unrelated key can have all `k` of its
positions already set by the union of other keys' bits, purely by coincidence,
and then `contains` returns true for a key that was never added. The rate of that
is what the `m` and `k` sizing controls: at the design load (~`N` keys in `N * 32`
bits with `k = 20`) it sits around `1e-6`. Insert far more than `N` keys and the
array fills, more positions are set, and the false-positive rate rises. The filter
does not detect or prevent this; staying within the load you sized for is the
caller's job.
