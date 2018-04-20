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

#ifndef AMZ_TEST_OOM_ALLOCATOR_HPP
#define AMZ_TEST_OOM_ALLOCATOR_HPP

#include <memory>
#include <new>
#include <type_traits>
#include <utility>


namespace utils {

// An allocator adapter that throws `bad_alloc` whenever some boolean flag is
// set, and unsets the flag when it's done. By having the user control when
// that flag is set, it's possible to test allocators in artificial
// out-of-memory conditions. After a `bad_alloc` has been thrown, the
// flag is reset to `false` so that a user can observe that a `bad_alloc`
// was indeed thrown.
template <typename Allocator>
class oom_allocator {
  Allocator allocator_;
  bool& oom_flag_;

  using AllocatorTraits = std::allocator_traits<Allocator>;

  template <typename>
  friend class oom_allocator;

public:
  oom_allocator(Allocator allocator, bool& oom_flag)
    : allocator_{std::move(allocator)}
    , oom_flag_{oom_flag}
  { }

  explicit oom_allocator(bool& oom_flag)
    : oom_allocator{Allocator{}, oom_flag}
  { }

  template <typename OtherAllocator, typename = std::enable_if_t<
    std::is_constructible<Allocator, OtherAllocator const&>::value
  >>
  oom_allocator(oom_allocator<OtherAllocator> const& other)
    : allocator_{other.allocator_}
    , oom_flag_{other.oom_flag_}
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
    using other = oom_allocator<typename AllocatorTraits::template rebind_alloc<T>>;
  };

  template <typename ...Args>
  void construct(pointer p, Args&& ...args) {
    allocator_.construct(p, std::forward<Args>(args)...);
  }

  void destroy(pointer p) {
    allocator_.destroy(p);
  }

  pointer allocate(size_type n) {
    if (oom_flag_) {
      oom_flag_ = false;
      throw std::bad_alloc{};
    } else {
      return allocator_.allocate(n);
    }
  }

  void deallocate(pointer p, size_type n) {
    allocator_.deallocate(p, n);
  }
};

} // end namespace utils

#endif // include guard
