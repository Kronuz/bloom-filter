// The header must compile without a diagnostic under -Wall -Wextra -Wpedantic -Werror (GCC and
// Clang; CMake sets the flags for this target only), so a consumer that builds with them can
// include it. A 128-bit integer type used outside `__extension__` is the diagnostic this guards
// (GCC: "ISO C++ does not support '__int128'"). The check is that this file builds, and the
// program also exercises each type so that the inline functions are instantiated.
#include <cstdio>
#include <memory>

#include "bloom_filter.hh"

template <typename F>
static bool round_trip() {
	auto f = std::make_unique<F>();
	f->add("present", 7);
	return f->contains("present", 7);
}

int main() {
	if (!round_trip<BloomFilter<8192>>() || !round_trip<FastBloomFilter<8192>>() || !round_trip<BlockedBloomFilter<8192>>()) {
		std::printf("bloom-filter pedantic: round trip failed\n");
		return 1;
	}
	std::printf("bloom-filter pedantic OK\n");
	return 0;
}
