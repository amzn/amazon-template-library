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

#include "common.hpp"


int main(int argc, char *argv[]) {
  std::cout << "Reading allocator from file " << filename.c_str() << std::endl;
  ipc::managed_mapped_file mmap(ipc::open_only, filename.c_str());
  Allocator* allocator = mmap.find<Allocator>("myalloc").first;
  MY_ASSERT(allocator != nullptr);

  std::vector<Pointer> pointers;
  for (int i = 0; i != child_allocations; ++i) {
    Pointer p = allocator->allocate(1);
    allocator->construct(p, i);
    pointers.push_back(p);
  }

  for (Pointer p : pointers) {
    allocator->destroy(p);
    allocator->deallocate(p, 1);
  }

  std::this_thread::sleep_for(timeout);
  allocator->purge(amz::purge_mode::opportunistic);
}
