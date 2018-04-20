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

#include <chrono>
#include <cstddef>
#include <memory>


using ValueType = int;
using UnderlyingAllocator = std::allocator<ValueType>;
using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;

TEST_CASE("copies yield allocators that can deallocate what was allocated by a compatible allocator") {
  auto const with_buffer_size = [](std::size_t buffer_size, std::size_t overflow) {
    auto const timeout = std::chrono::microseconds{10};
    Allocator alloc1{timeout};
    Allocator alloc2{alloc1};

    REQUIRE(alloc1 == alloc2);

    std::size_t const allocations = buffer_size * 10 + overflow;
    for (std::size_t i = 0; i != allocations; ++i) {
      ValueType* p = alloc1.allocate(1);
      alloc1.construct(p);

      alloc2.destroy(p);
      alloc2.deallocate(p, 1);
    }
  };

  for (std::size_t overflow : {0, 1, 2}) {
    with_buffer_size(1, overflow);
    with_buffer_size(2, overflow);
    with_buffer_size(20, overflow);
    with_buffer_size(40, overflow);
    with_buffer_size(100, overflow);
    with_buffer_size(1000, overflow);
  }
}
