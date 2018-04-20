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

#include <boost/filesystem.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <thread>
#include <vector>


struct OnDestruction {
  OnDestruction(std::function<void()> f) : callback(f) { }
  ~OnDestruction() { callback(); }
  std::function<void()> callback;
};

template <typename ValueType, typename F>
void with_ipc_allocator(F f) {
  namespace fs = boost::filesystem;
  namespace ipc = boost::interprocess;
  fs::path filename = fs::temp_directory_path() / fs::unique_path();
  constexpr std::size_t const FILE_SIZE = 100000000; // 100 MB
  using Allocator = ipc::allocator<ValueType, ipc::managed_mapped_file::segment_manager>;
  ipc::file_mapping::remove(filename.c_str());
  ipc::managed_mapped_file mmap(ipc::create_only, filename.c_str(), FILE_SIZE);
  Allocator allocator = mmap.get_allocator<ValueType>();
  try {
    f(allocator);
  } catch (...) {
    ipc::file_mapping::remove(filename.c_str());
    throw;
  }
}

TEST_CASE("basic utilization of the allocator with fancy pointers") {
  auto const test = [](auto timeout, std::size_t buffer_size) {
    with_ipc_allocator<OnDestruction>([=](auto ipc_allocator) {
      using Allocator = amz::deferred_reclamation_allocator<decltype(ipc_allocator)>;
      using Pointer = typename std::allocator_traits<Allocator>::pointer;

      std::size_t const allocations = 50000;
      std::set<int> deleted;

      {
        Allocator allocator{ipc_allocator, timeout, buffer_size};
        std::vector<Pointer> pointers;

        for (int i = 0; i != allocations; ++i) {
          Pointer p = allocator.allocate(1);
          allocator.construct(p, [&deleted, i] { deleted.insert(i); });
          pointers.push_back(p);
        }

        for (Pointer p : pointers) {
          allocator.destroy(p);
          allocator.deallocate(p, 1);
        }
      }

      for (int i = 0; i != allocations; ++i) {
        REQUIRE(deleted.count(i));
      }
    });
  };

  for (std::size_t buffer_size : {1, 2, 10, 100, 1000, 10000}) {
    test(std::chrono::microseconds{10}, buffer_size);
  }
}

TEST_CASE("opportunistic purge with fancy pointers") {
  auto const test = [](auto timeout, std::size_t buffer_size) {
    with_ipc_allocator<OnDestruction>([=](auto ipc_allocator) {
      using Allocator = amz::deferred_reclamation_allocator<decltype(ipc_allocator)>;
      using Pointer = typename std::allocator_traits<Allocator>::pointer;

      std::size_t const allocations = 50000;
      std::set<int> deleted;

      {
        Allocator allocator{ipc_allocator, timeout, buffer_size};
        std::vector<Pointer> pointers;

        for (int i = 0; i != allocations; ++i) {
          Pointer p = allocator.allocate(1);
          allocator.construct(p, [&deleted, i] { deleted.insert(i); });
          pointers.push_back(p);
        }

        for (Pointer p : pointers) {
          allocator.destroy(p);
          allocator.deallocate(p, 1);
        }

        allocator.purge(amz::purge_mode::opportunistic);
        std::this_thread::sleep_for(timeout);
        allocator.purge(amz::purge_mode::opportunistic);
      }

      for (int i = 0; i != allocations; ++i) {
        REQUIRE(deleted.count(i));
      }
    });
  };

  for (std::size_t buffer_size : {1, 2, 10, 100, 1000, 100000}) {
    test(std::chrono::milliseconds{10}, buffer_size);
  }
}

TEST_CASE("exhaustive purge with fancy pointers") {
  auto const test = [](auto timeout, std::size_t buffer_size) {
    with_ipc_allocator<OnDestruction>([=](auto ipc_allocator) {
      using Allocator = amz::deferred_reclamation_allocator<decltype(ipc_allocator)>;
      using Pointer = typename std::allocator_traits<Allocator>::pointer;

      std::size_t const allocations = 50000;
      std::set<int> deleted;

      {
        Allocator allocator{ipc_allocator, timeout, buffer_size};
        std::vector<Pointer> pointers;

        for (int i = 0; i != allocations; ++i) {
          Pointer p = allocator.allocate(1);
          allocator.construct(p, [&deleted, i] { deleted.insert(i); });
          pointers.push_back(p);
        }

        for (Pointer p : pointers) {
          allocator.destroy(p);
          allocator.deallocate(p, 1);
        }

        allocator.purge(amz::purge_mode::exhaustive);
      }

      for (int i = 0; i != allocations; ++i) {
        REQUIRE(deleted.count(i));
      }
    });
  };

  for (std::size_t buffer_size : {1, 2, 10, 100, 1000, 100000}) {
    test(std::chrono::milliseconds{10}, buffer_size);
  }
}
