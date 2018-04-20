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

#include <amz/call.hpp>

#include <boost/blank.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp> // required by Catch

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <chrono>
#include <cstddef>


TEST_CASE("at_most_every triggers not too often") {
  auto at_most_every_ms = amz::at_most_every{std::chrono::milliseconds{1}};

  std::size_t calls = 0;
  auto start = std::chrono::steady_clock::now();
  while (true) {
    amz::call(at_most_every_ms, [&]{ ++calls; });

    // Iterate for roughly one second, and break out afterwards.
    if (std::chrono::steady_clock::now() > start + std::chrono::seconds{1})
      break;
  }
  auto end = std::chrono::steady_clock::now();

  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  // We can't have been called more than the number of milliseconds that elapsed
  // when we iterated, because we can't be called more often than once every ms.
  REQUIRE(calls <= elapsed_ms.count());

  // We must have been called at least once (the first time around).
  REQUIRE(calls >= 1);

  // But realistically, we should have been called even more than that.
  // Given that a millisecond is pretty long compared to the expected time
  // it takes to go once around the loop, it's probably safe to assume that
  // we've been called during at least a third of the milliseconds that elapsed.
  REQUIRE(calls >= elapsed_ms.count() / 3);
}

TEST_CASE("at_most{n} triggers n times") {
  for (std::size_t times = 0; times != 10; ++times) {
    auto n_times = amz::at_most{times};
    std::size_t calls = 0;

    for (std::size_t i = 0; i != 1000; ++i) {
      amz::call(n_times, [&]{
        ++calls;
      });
    }

    REQUIRE(calls == times);
  }
}

struct mock_flag {
  bool active() const { return active_; }
  bool active_;
};

TEST_CASE("amz::call returns the result of the function when active") {
  mock_flag active{true};

  SECTION("with a non-void return type") {
    boost::optional<int> result = amz::call(active, [] { return 3; });
    REQUIRE(result != boost::none);
    REQUIRE(*result == 3);
  }

  SECTION("with a void return type") {
    boost::optional<boost::blank> result = amz::call(active, [] { });
    REQUIRE(result != boost::none);
  }
}

TEST_CASE("amz::call returns an empty optional when inactive") {
  mock_flag inactive{false};

  SECTION("with a non-void return type") {
    boost::optional<int> result = amz::call(inactive, [] { return 3; });
    REQUIRE(result == boost::none);
  }

  SECTION("with a void return type") {
    boost::optional<boost::blank> result = amz::call(inactive, [] { });
    REQUIRE(result == boost::none);
  }
}
