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

#ifndef AMZ_TEST_BOUNDED_ALLOCATOR_HPP
#define AMZ_TEST_BOUNDED_ALLOCATOR_HPP

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>


namespace utils {

// An allocator adapter that throws `bad_alloc` whenever the number of live
// allocations reaches a certain limit. The number of live allocations is
// shared by all copies of the allocator, and it is provided by the user of
// this class so that it can be observed from the outside.
template <typename Allocator>
class bounded_allocator {
  Allocator allocator_;
  std::size_t const max_live_allocations_;
  std::size_t& live_allocations_;

  using AllocatorTraits = std::allocator_traits<Allocator>;

  template <typename>
  friend class bounded_allocator;

public:
  bounded_allocator(Allocator allocator, std::size_t max_live_allocations, std::size_t& live_allocations)
    : allocator_{std::move(allocator)}
    , max_live_allocations_{max_live_allocations}
    , live_allocations_{live_allocations}
  { }

  explicit bounded_allocator(std::size_t max_live_allocations, std::size_t& live_allocations)
    : bounded_allocator{Allocator{}, max_live_allocations, live_allocations}
  { }

  template <typename OtherAllocator, typename = std::enable_if_t<
    std::is_constructible<Allocator, OtherAllocator const&>::value
  >>
  bounded_allocator(bounded_allocator<OtherAllocator> const& other)
    : allocator_{other.allocator_}
    , max_live_allocations_{other.max_live_allocations_}
    , live_allocations_{other.live_allocations_}
  { }

  using pointer = typename AllocatorTraits::pointer;
  using const_pointer = typename AllocatorTraits::const_pointer;
  using void_pointer = typename AllocatorTraits::void_pointer;
  using const_void_pointer = typename AllocatorTraits::const_void_pointer;
  using size_type = typename AllocatorTraits::size_type;
  using difference_type = typename AllocatorTraits::difference_type;
  using value_type = typename AllocatorTraits::value_type;

  template <typename T>
  struct rebind {
    using other = bounded_allocator<typename AllocatorTraits::template rebind_alloc<T>>;
  };

  template <typename ...Args>
  void construct(pointer p, Args&& ...args) {
    allocator_.construct(p, std::forward<Args>(args)...);
  }

  void destroy(pointer p) {
    allocator_.destroy(p);
  }

  pointer allocate(size_type n) {
    if (live_allocations_ + 1 > max_live_allocations_) {
      throw std::bad_alloc{};
    } else {
      auto p = allocator_.allocate(n);
      ++live_allocations_;
      return p;
    }
  }

  void deallocate(pointer p, size_type n) {
    allocator_.deallocate(p, n);
    --live_allocations_;
  }
};

} // end namespace utils

#endif // include guard
