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
#include <memory>


// An allocator that refuses to free what's been allocated by anyone but itself.
template <typename T>
struct SelfCompatibleAllocator : std::allocator<T> {
  bool operator==(SelfCompatibleAllocator const& other) const
  { return &other == this; }
  bool operator!=(SelfCompatibleAllocator const& other) const
  { return !(*this == other); }
};

// An allocator that is always equal to another allocator of the same type.
template <typename T>
using AlwaysEqualAllocator = std::allocator<T>;

TEST_CASE("an allocator should be equal to itself") {
  using Allocator = amz::deferred_reclamation_allocator<SelfCompatibleAllocator<int>>;
  Allocator alloc{std::chrono::microseconds{10}};
  REQUIRE(alloc == alloc);
}

TEST_CASE("making a copy of an allocator should yield an allocator that compares equal") {
  using Allocator = amz::deferred_reclamation_allocator<AlwaysEqualAllocator<int>>;
  Allocator alloc{std::chrono::microseconds{10}};
  Allocator copy{alloc};
  REQUIRE(alloc == copy);
}

TEST_CASE("two allocators with equal allocators and timeouts should compare equal") {
  using Allocator = amz::deferred_reclamation_allocator<AlwaysEqualAllocator<int>>;
  Allocator alloc1{std::chrono::microseconds{10}};
  Allocator alloc2{std::chrono::microseconds{10}};
  REQUIRE(alloc1 == alloc2);
}

TEST_CASE("allocators with different timeouts should not compare equal") {
  using Allocator = amz::deferred_reclamation_allocator<AlwaysEqualAllocator<int>>;
  Allocator alloc1{std::chrono::microseconds{10}};
  Allocator alloc2{std::chrono::microseconds{11}};

  REQUIRE(alloc1 != alloc2);
}

TEST_CASE("allocators with different allocators should not compare equal") {
  using Allocator = amz::deferred_reclamation_allocator<SelfCompatibleAllocator<int>>;
  Allocator alloc1{std::chrono::microseconds{10}};
  Allocator alloc2{std::chrono::microseconds{10}};

  REQUIRE(alloc1 != alloc2);
}

TEST_CASE("allocators with both different timeouts and allocators should not compare equal") {
  using Allocator = amz::deferred_reclamation_allocator<SelfCompatibleAllocator<int>>;
  Allocator alloc1{std::chrono::microseconds{10}};
  Allocator alloc2{std::chrono::microseconds{11}};

  REQUIRE(alloc1 != alloc2);
}
