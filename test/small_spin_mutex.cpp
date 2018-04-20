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

#include <amz/small_spin_mutex.hpp>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <type_traits>


static_assert(sizeof(amz::small_spin_mutex) <= sizeof(char),
  "amz::small_spin_mutex is guaranteed to be no more than one byte in size");

static_assert(!std::is_copy_constructible<amz::small_spin_mutex>::value,
  "amz::small_spin_mutex should not be CopyConstructible");

static_assert(!std::is_move_constructible<amz::small_spin_mutex>::value,
  "amz::small_spin_mutex should not be MoveConstructible");

static_assert(!std::is_copy_assignable<amz::small_spin_mutex>::value,
  "amz::small_spin_mutex should not be CopyAssignable");

static_assert(!std::is_move_assignable<amz::small_spin_mutex>::value,
  "amz::small_spin_mutex should not be MoveAssignable");

static_assert(std::is_trivially_destructible<amz::small_spin_mutex>::value,
  "amz::small_spin_mutex should always be TriviallyDestructible");

TEST_CASE("check noexcept properties") {
  amz::small_spin_mutex a{};
  static_assert(noexcept(amz::small_spin_mutex{}), "");
  static_assert(noexcept(a.lock()), "");
  static_assert(noexcept(a.try_lock()), "");
  static_assert(noexcept(a.unlock()), "");
}

TEST_CASE("lock/unlock") {
  amz::small_spin_mutex a{};
  amz::small_spin_mutex b{};

  a.lock();
  b.lock();
  a.unlock();
  b.unlock();
}

TEST_CASE("try_lock/unlock") {
  amz::small_spin_mutex a{};
  amz::small_spin_mutex b{};

  REQUIRE(a.try_lock());
  REQUIRE(!a.try_lock());
  REQUIRE(!a.try_lock());
  REQUIRE(!a.try_lock());

  REQUIRE(b.try_lock());
  REQUIRE(!b.try_lock());
  REQUIRE(!b.try_lock());
  REQUIRE(!b.try_lock());

  a.unlock();
  REQUIRE(a.try_lock());
  a.unlock();

  b.unlock();
  REQUIRE(b.try_lock());
  b.unlock();
}

TEST_CASE("try_lock when already locked") {
  amz::small_spin_mutex a{};
  amz::small_spin_mutex b{};

  a.lock();
  REQUIRE(!a.try_lock());

  b.lock();
  REQUIRE(!a.try_lock());
  REQUIRE(!b.try_lock());

  a.unlock();
  b.unlock();
}

template <typename Range, typename RandomNumberGenerator>
auto& pick_random(Range const& range, RandomNumberGenerator& rng) {
  auto first = std::begin(range);
  auto last = std::end(range);
  assert(first != last && "can't pick a random element if the range is empty");
  auto const dist = std::distance(first, last);
  auto random = std::next(first, rng() % dist);
  assert(random != last && "can never happen because we picked within the range");
  return *random;
}

TEST_CASE("multithreaded access") {
  // We create N threads that check for the validity of a variable, and update
  // it with a new valid value. Validity of the variable is established by
  // the variable being in some fixed set (see below). If the mutex was
  // somehow not making its job, the hope is that a thread might catch the
  // variable in some in-between state, which would hopefully be invalid.
  struct Record {
    std::string string;
    amz::small_spin_mutex mutex;
  };

  std::set<std::string> const valid_strings{
    "foo", "bar", "baz", "dinosaur", "battery", "multithreaded", "access",
    "I", "hate", "deadlocks", "and", "I'll", "be", "incredibly", "careful",
    "when", "using", "this", "class",

    "long string that takes a while to copy and hence has more chances to "
    "catch a thread in the middle of a copy xxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
  };

  std::random_device rd;
  std::mt19937 g(rd());
  Record record{pick_random(valid_strings, g)};

  std::size_t const THREADS = 4;
  std::vector<std::thread> threads;
  std::generate_n(std::back_inserter(threads), THREADS, [&record, valid_strings] {
    return std::thread{[&record, valid_strings] {
      std::random_device rd;
      std::mt19937 g(rd());

      for (int i = 0; i != 1000; ++i) {
        auto& s = pick_random(valid_strings, g);
        record.mutex.lock();
        REQUIRE(valid_strings.count(record.string));
        record.string = s;
        record.mutex.unlock();
      }
    }};
  });

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_CASE("small_spin_mutex is default constructed to an unlocked state") {
  // This test may look crazy, but we had a bug where the atomic_flag was not
  // properly initialized inside the mutex, and all hell broke loose.

  // Fill the memory with ones so that if the spin lock is not initialized
  // properly, it'll show.
  char* memory = static_cast<char*>(std::malloc(sizeof(amz::small_spin_mutex)));
  std::uninitialized_fill_n(memory, sizeof(amz::small_spin_mutex), 1);
  amz::small_spin_mutex* mutex = new (memory) amz::small_spin_mutex; // default construct, do not value-initialize
  REQUIRE(mutex->try_lock());
  mutex->unlock();

  mutex->~small_spin_mutex();
  std::free(memory);
}
