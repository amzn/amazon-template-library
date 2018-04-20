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

#include <memory>
#include <utility>


// This test makes sure that `purge()` is noexcept if and only if the
// destructor of the value type is nothrow destructible.

template <typename ValueType>
using deferred_alloc = amz::deferred_reclamation_allocator<
  std::allocator<ValueType>
>;

struct ThrowDestructible1 { ~ThrowDestructible1() noexcept(false) { } };
struct ThrowDestructible2 { ThrowDestructible1 member; ~ThrowDestructible2() { } };
struct NoThrowDestructible1 { ~NoThrowDestructible1() noexcept { } };
struct NoThrowDestructible2 { ~NoThrowDestructible2() { } };

#define PURGE_NOEXCEPT_TEST(...) \
  static_assert(!noexcept(std::declval<deferred_alloc<ThrowDestructible1>&>().purge(__VA_ARGS__)), ""); \
  static_assert(!noexcept(std::declval<deferred_alloc<ThrowDestructible2>&>().purge(__VA_ARGS__)), ""); \
  static_assert(noexcept(std::declval<deferred_alloc<NoThrowDestructible1>&>().purge(__VA_ARGS__)), ""); \
  static_assert(noexcept(std::declval<deferred_alloc<NoThrowDestructible2>&>().purge(__VA_ARGS__)), "");

PURGE_NOEXCEPT_TEST()
PURGE_NOEXCEPT_TEST(amz::purge_mode::opportunistic)
PURGE_NOEXCEPT_TEST(amz::purge_mode::exhaustive)

int main() { }
