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


struct OnDestruction {
  OnDestruction(std::function<void()> f) : callback(f) { }
  ~OnDestruction() { callback(); }
  std::function<void()> callback;
};

TEST_CASE("deallocated objects live at least for the duration of the timeout") {
  using ValueType = OnDestruction;
  using UnderlyingAllocator = std::allocator<ValueType>;
  using Allocator = amz::deferred_reclamation_allocator<UnderlyingAllocator>;
  using TimePoint = std::chrono::steady_clock::time_point;

  auto const test = [](auto timeout, std::size_t delay_buffer_size, std::size_t cycles) {
    // Time at which we call the allocator's `deallocate()` method for an object.
    std::map<std::size_t, TimePoint> deallocation_times;

    // Actual time at which objects are reclaimed.
    std::map<std::size_t, TimePoint> reclamation_times;

    {
      // Important:
      // The maps have to outlive the allocator, since the allocator's destructor
      // may call the object's destructors, which use the maps.
      Allocator allocator{timeout, delay_buffer_size};

      // Allocate/deallocate a bunch of objects, and mark the time at which
      // deallocation is requested for each of them. We do this for a couple
      // of timeout cycles.
      {
        auto const start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; std::chrono::steady_clock::now() <= start + (timeout * cycles); ++i) {
          ValueType* p = allocator.allocate(1);
          allocator.construct(p, [i, &reclamation_times] {
            reclamation_times[i] = std::chrono::steady_clock::now();
          });
          allocator.destroy(p);
          deallocation_times[i] = std::chrono::steady_clock::now();
          allocator.deallocate(p, 1);
        }
      }

      // Make sure that nothing that we requested to deallocate _and_ that was
      // actually reclaimed, was reclaimed before its timeout time had elapsed.
      for (auto const& reclamation : reclamation_times) {
        auto deallocation = deallocation_times.find(reclamation.first);
        REQUIRE(deallocation != deallocation_times.end());

        auto const deallocation_time = deallocation->second;
        auto const reclamation_time = reclamation.second;
        REQUIRE(reclamation_time > deallocation_time + timeout);
      }
    }
  };

  test(std::chrono::milliseconds{1}, 100, 10);
  test(std::chrono::milliseconds{10}, 100, 10);
  test(std::chrono::milliseconds{50}, 100, 10);
}
