cmake_minimum_required(VERSION 3.14)
project(bloom_filter LANGUAGES CXX)

include(FetchContent)

# --- Dependency: hashes ---
# bloom_filter.hh does #include "hashes.hh" for xxh64::hash and fnv1ah64::hash:
# the two hash families it derives its k probe positions from. That header lives
# in the sibling hashes library, an already-extracted standalone library;
# FetchContent pulls it in and puts its header on the include path so the include
# resolves. We track its tip with GIT_TAG main, like the rest of the family.
# (hashes in turn pulls static-string and char-classify; that is transitive and
# handled by its own CMakeLists.)
FetchContent_Declare(
  hashes
  GIT_REPOSITORY https://github.com/Kronuz/hashes.git
  GIT_TAG        ffa2f5c7d5fa3ab576f1c32b43733ef3633199cd
)
FetchContent_MakeAvailable(hashes)

# --- Library target ---
# bloom_filter is header-only: one header (bloom_filter.hh) with the BloomFilter
# class template. The target is a pure INTERFACE library that puts the source dir
# on the consumer's include path, requests cxx_std_20, and links the hashes target
# so "hashes.hh" resolves and rides onto the consumer's include path too.
add_library(bloom_filter INTERFACE)
target_include_directories(bloom_filter INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(bloom_filter INTERFACE cxx_std_20)
target_link_libraries(bloom_filter INTERFACE hashes::hashes)
# Alias so consumers can link `bloom_filter::bloom_filter` whether they add this
# via add_subdirectory or FetchContent.
add_library(bloom_filter::bloom_filter ALIAS bloom_filter)

# Only build the test when this repo is the top-level project, so consumers
# (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(bloom_filter_test test/test.cc)
  target_link_libraries(bloom_filter_test PRIVATE bloom_filter)
  add_test(NAME bloom_filter COMMAND bloom_filter_test)

  # The same checks with the 128-bit multiply compiled out, so the portable fallback that
  # MSVC and 32-bit targets use runs on every platform and not only where it is the default.
  add_executable(bloom_filter_portable_test test/test.cc)
  target_link_libraries(bloom_filter_portable_test PRIVATE bloom_filter)
  target_compile_definitions(bloom_filter_portable_test PRIVATE BLOOM_FILTER_NO_INT128)
  add_test(NAME bloom_filter_portable COMMAND bloom_filter_portable_test)

  # The header builds under -Wall -Wextra -Wpedantic -Werror (GCC and Clang). The target is
  # left out of the default build and built by a test, so a diagnostic fails that test.
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(bloom_filter_pedantic_test EXCLUDE_FROM_ALL test/pedantic.cc)
    target_link_libraries(bloom_filter_pedantic_test PRIVATE bloom_filter)
    target_compile_options(bloom_filter_pedantic_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME bloom_filter_pedantic_build
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target bloom_filter_pedantic_test)
    add_test(NAME bloom_filter_pedantic COMMAND bloom_filter_pedantic_test)
    set_tests_properties(bloom_filter_pedantic_build PROPERTIES FIXTURES_SETUP bloom_filter_pedantic_exe)
    set_tests_properties(bloom_filter_pedantic PROPERTIES FIXTURES_REQUIRED bloom_filter_pedantic_exe)
  endif()

  # A runnable usage example that self-checks the core Bloom filter contract. It
  # is also registered with ctest so example drift catches regressions.
  add_executable(bloom_filter_usage examples/usage.cc)
  target_link_libraries(bloom_filter_usage PRIVATE bloom_filter)
  add_test(NAME bloom_filter_usage COMMAND bloom_filter_usage)

  # Benchmark (off by default):
  #   cmake -B build -DBLOOM_FILTER_BUILD_BENCH=ON && cmake --build build --target bloom_bench
  # The comparator boost::bloom (Boost >= 1.89) is found only here, never by the library; the
  # benchmark builds without it.
  option(BLOOM_FILTER_BUILD_BENCH "Build bench/bloom_bench.cc" OFF)
  if(BLOOM_FILTER_BUILD_BENCH)
    add_executable(bloom_bench bench/bloom_bench.cc)
    target_link_libraries(bloom_bench PRIVATE bloom_filter)
    target_compile_options(bloom_bench PRIVATE -O3 -DNDEBUG)
    find_path(BOOST_BLOOM_INCLUDE boost/bloom/filter.hpp HINTS /opt/homebrew/include /usr/local/include)
    if(BOOST_BLOOM_INCLUDE)
      target_include_directories(bloom_bench PRIVATE ${BOOST_BLOOM_INCLUDE})
      target_compile_definitions(bloom_bench PRIVATE HAVE_BOOST_BLOOM)
    endif()
  endif()
endif()
