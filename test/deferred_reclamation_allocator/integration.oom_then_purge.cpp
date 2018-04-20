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
#include "bounded_allocator.hpp"

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <new>
#include <vector>


using ValueType = int;
using UnderlyingAllocator = utils::bounded_allocator<std::allocator<ValueType>>;
using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;

TEST_CASE("purging after bad_alloc allows recovering") {
  auto const test = [&](auto timeout, std::size_t delay_buffer_size) {
    std::size_t const max_live_allocations = 1000;
    std::size_t live_allocations = 0;
    UnderlyingAllocator bounded_alloc{max_live_allocations, live_allocations};
    Allocator allocator{bounded_alloc, timeout, delay_buffer_size};
    std::vector<ValueType*> pointers;

    // Allocate a bunch of objects, and deallocate half of them. This makes
    // sure that we populate the delay list with some stuff. We do that until
    // a `bad_alloc` is thrown, and then we request the allocator to purge
    // itself.
    std::cout << "starting to allocate objects" << std::endl;
    try {
      while (true) {
        auto p1 = allocator.allocate(1);
        allocator.construct(p1, 0);
        REQUIRE_NOTHROW(pointers.push_back(p1));

        auto p2 = allocator.allocate(1);
        allocator.construct(p2, 0);
        allocator.destroy(p2);
        allocator.deallocate(p2, 1);
      }
    } catch (std::bad_alloc const&) {
      // Make sure we throw if we try to allocate at this point, then purge.
      REQUIRE_THROWS_AS(allocator.allocate(1), std::bad_alloc);
      std::cout << "got bad_alloc with " << live_allocations
                << " live allocations, purging exhaustively" << std::endl;
      allocator.purge(amz::purge_mode::exhaustive);
      std::cout << live_allocations << " live allocations left after purging" << std::endl;
    }

    // Validate that we can indeed allocate after we've purged.
    REQUIRE_NOTHROW([&]{
      auto p = allocator.allocate(1);
      allocator.construct(p, 0);
      allocator.destroy(p);
      allocator.deallocate(p, 1);
    }());

    std::cout << "purging done, deallocating everything left" << std::endl;

    // Deallocate all remaining objects that were allocated just for the test.
    for (ValueType* p : pointers) {
      allocator.destroy(p);
      allocator.deallocate(p, 1);
    }
  };

  for (std::size_t delay_buffer_size : {1, 2, 10, 100}) {
    test(std::chrono::nanoseconds{1}, delay_buffer_size);
    test(std::chrono::microseconds{10}, delay_buffer_size);
    test(std::chrono::milliseconds{10}, delay_buffer_size);
    test(std::chrono::milliseconds{100}, delay_buffer_size);
  }
}
