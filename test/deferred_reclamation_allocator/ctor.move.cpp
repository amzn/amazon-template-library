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
#include <vector>


using ValueType = int;
using UnderlyingAllocator = std::allocator<ValueType>;
using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;

TEST_CASE("move constructing passes the resources to the newly-constructed allocator") {
  auto const with_buffer_size = [](std::size_t buffer_size, std::size_t overflow) {
    std::vector<ValueType*> pointers;

    auto const timeout = std::chrono::microseconds{10};
    Allocator alloc1{timeout};

    std::size_t const allocations = buffer_size * 10 + overflow;
    for (std::size_t i = 0; i != allocations; ++i) {
      ValueType* p = alloc1.allocate(1);
      alloc1.construct(p);
      pointers.push_back(p);
    }

    Allocator alloc2{std::move(alloc1)};
    for (std::size_t i = 0; i != allocations; ++i) {
      alloc2.destroy(pointers[i]);
      alloc2.deallocate(pointers[i], 1);
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
