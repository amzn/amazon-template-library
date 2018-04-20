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
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>


struct OnDestruction {
  OnDestruction(std::function<void()> f) : callback(f) { }
  ~OnDestruction() { callback(); }
  std::function<void()> callback;
};

using ValueType = OnDestruction;
using UnderlyingAllocator = std::allocator<ValueType>;
using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;

TEST_CASE("an allocated object IS destroyed when purging if it made it to the delay list") {
  auto const timeout = std::chrono::milliseconds{100}; // make the timeout large enough to make sure we call `purge()` before entries are ripe
  std::size_t const delay_buffer_size = 1; // make sure the buffer gets flushed on the first deallocation

  bool was_destroyed = false;
  Allocator allocator{UnderlyingAllocator{}, timeout, delay_buffer_size};
  ValueType* p = allocator.allocate(1);
  allocator.construct(p, [&] { was_destroyed = true; });
  allocator.destroy(p);
  allocator.deallocate(p, 1);
  REQUIRE(!was_destroyed);
  allocator.purge(amz::purge_mode::exhaustive);
  REQUIRE(was_destroyed);
}

TEST_CASE("an allocated object IS NOT destroyed when purging if it did not make it to the delay list") {
  auto const timeout = std::chrono::milliseconds{2};
  std::size_t const delay_buffer_size = 2; // make sure the buffer does not get flushed on the first deallocation

  bool was_destroyed = false;
  Allocator allocator{UnderlyingAllocator{}, timeout, delay_buffer_size};
  ValueType* p = allocator.allocate(1);
  allocator.construct(p, [&] { was_destroyed = true; });
  allocator.destroy(p);
  allocator.deallocate(p, 1);
  REQUIRE(!was_destroyed);
  allocator.purge(amz::purge_mode::exhaustive);
  REQUIRE(!was_destroyed);
}

TEST_CASE("deallocate after purging") {
  // The following tests are whitebox tests making sure that we properly
  // maintain the state of the delay list when we purge.

  auto const timeout = std::chrono::milliseconds{10};
  std::size_t const delay_buffer_size = 1;

  SECTION("after purging nothing") {
    std::map<std::string, bool> was_destroyed;
    Allocator allocator{UnderlyingAllocator{}, timeout, delay_buffer_size};

    ValueType* p1 = allocator.allocate(1);
    ValueType* p2 = allocator.allocate(1);

    allocator.construct(p1, [&] { was_destroyed["p1"] = true; });
    allocator.construct(p2, [&] { was_destroyed["p2"] = true; });

    allocator.destroy(p1);
    allocator.destroy(p2);

    allocator.purge(amz::purge_mode::exhaustive);
    REQUIRE(was_destroyed.empty());

    allocator.deallocate(p1, 1);
    allocator.deallocate(p2, 1);

    allocator.purge(amz::purge_mode::exhaustive);

    REQUIRE(was_destroyed.size() == 2);
    REQUIRE(was_destroyed["p1"]);
    REQUIRE(was_destroyed["p2"]);
  }

  SECTION("after purging something") {
    std::map<std::string, bool> was_destroyed;
    Allocator allocator{UnderlyingAllocator{}, timeout, delay_buffer_size};

    ValueType* p1 = allocator.allocate(1);
    ValueType* p2 = allocator.allocate(1);

    allocator.construct(p1, [&] { was_destroyed["p1"] = true; });
    allocator.construct(p2, [&] { was_destroyed["p2"] = true; });

    // Create something dummy so we have something to purge.
    {
      ValueType* dummy = allocator.allocate(1);
      allocator.construct(dummy, [&] { was_destroyed["dummy"] = true; });
      allocator.destroy(dummy);
      allocator.deallocate(dummy, 1);
    }

    // Purge and make sure we purged something.
    allocator.purge(amz::purge_mode::exhaustive);
    REQUIRE(was_destroyed["dummy"]);

    // Deallocate what's left.
    allocator.deallocate(p1, 1);
    allocator.deallocate(p2, 1);

    // Purge again and make sure we've deallocated correctly.
    allocator.purge(amz::purge_mode::exhaustive);

    REQUIRE(was_destroyed.size() == 3);
    REQUIRE(was_destroyed["dummy"]);
    REQUIRE(was_destroyed["p1"]);
    REQUIRE(was_destroyed["p2"]);
  }
}
