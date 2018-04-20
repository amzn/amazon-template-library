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

#include <boost/filesystem.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib> // std::system, EXIT_FAILURE
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace fs = boost::filesystem;
namespace ipc = boost::interprocess;
using namespace std::literals;


#define MY_ASSERT(...) \
    do { if (!(__VA_ARGS__)) throw "Assertion failed: " #__VA_ARGS__ ; } while (false)

using ValueType = int;
using IpcAllocator = ipc::allocator<ValueType, ipc::managed_mapped_file::segment_manager>;
using Allocator = amz::deferred_reclamation_allocator<IpcAllocator>;
using Pointer = std::allocator_traits<Allocator>::pointer;

static constexpr auto timeout = std::chrono::milliseconds{10};
static constexpr std::size_t buffer_size = 10;
static constexpr std::size_t parent_allocations = 10000;
static constexpr std::size_t child_allocations = 10000;
static constexpr std::size_t const file_size = 10000000; // 10 MB
static fs::path filename = fs::current_path() / ".deferred_reclamation_allocator__interprocess_test";
