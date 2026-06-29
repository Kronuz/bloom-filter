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
  GIT_TAG        main
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
endif()
