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
  std::cout << "Writing allocator to file " << filename.c_str() << std::endl;
  ipc::managed_mapped_file mmap(ipc::create_only, filename.c_str(), file_size);
  IpcAllocator ipc_allocator = mmap.get_allocator<ValueType>();
  Allocator* allocator = mmap.construct<Allocator>("myalloc")(ipc_allocator, timeout, buffer_size);

  MY_ASSERT(allocator != nullptr);

  for (int i = 0; i != parent_allocations; ++i) {
    Pointer p = allocator->allocate(1);
    allocator->construct(p, i);
    allocator->destroy(p);
    allocator->deallocate(p, 1);
  }

  MY_ASSERT(mmap.flush());
}
