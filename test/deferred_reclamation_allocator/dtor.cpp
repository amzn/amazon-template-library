// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// A copy of the License is located at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// or in the "license" file accompanying this file. This file is distributed
// on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied. See the License for the specific language governing
// permissions and limitations under the License.

#include <amz/deferred_reclamation_allocator.hpp>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <set>
#include <string>


template <typename T>
struct OnDestruction {
  OnDestruction(T v, std::function<void(T)> f)
    : value(v), callback(f)
  { }

  ~OnDestruction() { callback(value); }

  T value;
  std::function<void(T)> callback;
};

template <typename RNG>
static std::string random_string(RNG& rng, std::size_t max_length) {
  auto randchar = [&] {
    char const charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::size_t const max_index = (sizeof(charset) - 1);
    return charset[rng() % max_index];
  };
  std::size_t const length = rng() % max_length;
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

using ValueType = OnDestruction<std::string>;
using UnderlyingAllocator = std::allocator<ValueType>;
using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;

TEST_CASE("all allocated elements are destroyed when the allocator is destroyed") {
  std::mt19937 rng{};
  auto const test = [&](auto timeout, std::size_t delay_buffer_size, std::size_t block_size) {
    // Generate a random set of unique strings with a fixed size. Those strings
    // will act as tokens for objects being destroyed.
    std::set<std::string> strings;
    while (strings.size() != 100 * block_size) {
      strings.insert(random_string(rng, 6));
    }

    // Allocate, construct, destroy, deallocate, and make sure the destructor
    // of the allocator actually cleans up everything.
    std::set<std::string> destroyed;
    {
      Allocator allocator{timeout, delay_buffer_size};
      for (auto string = strings.begin(); string != strings.end(); /* see inner loop */) {
        // Allocate a block of objects.
        ValueType* const block = allocator.allocate(block_size);

        // Construct and destroy each object in the block.
        for (ValueType* p = block; p != block + block_size; ++p) {
          REQUIRE(string != strings.end()); // otherwise, the test is broken
          allocator.construct(p, *string, [&](auto const& v){ destroyed.insert(v); });
          allocator.destroy(p);
          ++string;
        }

        // Deallocate the block.
        allocator.deallocate(block, block_size);
      }
    }
    REQUIRE(destroyed == strings);
  };

  for (std::size_t block_size = 1; block_size != 5; ++block_size) {
    test(std::chrono::microseconds{5}, 1, block_size);
    test(std::chrono::microseconds{5}, 2, block_size);
    test(std::chrono::microseconds{5}, 100, block_size);

    test(std::chrono::milliseconds{5}, 1, block_size);
    test(std::chrono::milliseconds{5}, 2, block_size);
    test(std::chrono::milliseconds{5}, 100, block_size);
  }
}

TEST_CASE("destroy an empty allocator") {
  Allocator allocator{std::chrono::microseconds{10}};
}
