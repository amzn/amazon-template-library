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

#include <amz/bounded_channel.hpp>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <thread>
#include <vector>


TEST_CASE("Iterators compare against past-the-end iterator properly") {
  amz::bounded_channel<int> channel{64};

  SECTION("With an empty and closed channel") {
    channel.close();
    auto it = std::begin(channel);
    REQUIRE(it == std::end(channel));
  }

  SECTION("With a non-empty and non-closed channel") {
    channel.push(1);
    auto it = std::begin(channel);
    REQUIRE(it != std::end(channel));
  }

  SECTION("With a non-empty and closed channel") {
    channel.push(1);
    channel.close();
    auto it = std::begin(channel);
    REQUIRE(it != std::end(channel));
    ++it;
    REQUIRE(it == std::end(channel));
  }
}

TEST_CASE("Copy of an iterator points to the same value as the original iterator") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);

  auto it = std::begin(channel);
  auto copy1 = it;
  REQUIRE(*it == 1);
  REQUIRE(*copy1 == 1);

  ++it;
  auto copy2 = it;
  REQUIRE(*it == 2);
  REQUIRE(*copy2 == 2);
}

TEST_CASE("Assigned iterator points to the same value as the assigned-from iterator") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);

  auto it = std::begin(channel);
  amz::bounded_channel<int>::iterator other;
  other = it;
  REQUIRE(*it == 1);
  REQUIRE(*other == 1);

  ++it;
  other = it;
  REQUIRE(*it == 2);
  REQUIRE(*other == 2);
}

TEST_CASE("Non past-the-end iterators do not compare equal") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.push(4);

  auto it1 = std::begin(channel);
  auto it2 = std::begin(channel);
  REQUIRE(it1 != it2);

  ++it1;
  ++it2;
  REQUIRE(it1 != it2);
}

TEST_CASE("Comparison with past-the-end iterator works as expected") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.close();

  auto it = std::begin(channel);
  auto end = std::end(channel);
  REQUIRE(it != end);

  ++it;
  REQUIRE(it != end);

  ++it;
  REQUIRE(it == end);
  REQUIRE(it == std::end(channel));
}

TEST_CASE("Iterator can be used to iterate over the contents of the channel when the channel is not empty") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.push(4);

  std::thread iter_thread;

  SECTION("When all the channel is populated before the iterator is created") {
    iter_thread = std::thread{[&] {
      std::vector<int> actual;
      std::copy(std::begin(channel), std::end(channel), std::back_inserter(actual));
      std::vector<int> const expected = {1, 2, 3, 4};
      REQUIRE(actual == expected);
    }};
  }

  SECTION("When more elements are added after the iterator has been created") {
    iter_thread = std::thread{[&] {
      std::vector<int> actual;
      std::copy(std::begin(channel), std::end(channel), std::back_inserter(actual));
      std::vector<int> const expected = {1, 2, 3, 4, 5, 6, 7};
      REQUIRE(actual == expected);
    }};
    channel.push(5);
    channel.push(6);
    channel.push(7);
  }

  channel.close();
  iter_thread.join();
}

TEST_CASE("Iterator blocks when the channel is empty") {
  amz::bounded_channel<int> channel{64};

  std::atomic<bool> unblocked{false};
  std::atomic<bool> started{false};
  std::thread iter_thread{[&] {
    started = true;
    auto it = std::begin(channel);
    unblocked = true;
    REQUIRE(*it == 1);
    ++it;
    REQUIRE(*it == 2);
  }};

  // Wait until the `iter_thread` has started.
  while (!started) std::this_thread::yield();

  // Make sure the thread is blocked.
  REQUIRE(started);
  REQUIRE(!unblocked);

  // Push something to unblock it.
  channel.push(1);
  channel.push(2);

  iter_thread.join();
}

TEST_CASE("Iterator drains the channel when the channel is closed") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.push(4);

  std::vector<int> actual;

  SECTION("When the channel is closed before the iterator has been created") {
    channel.close();
    auto it = std::begin(channel);
    std::copy(it, std::end(channel), std::back_inserter(actual));
    std::vector<int> const expected = {1, 2, 3, 4};
    REQUIRE(actual == expected);
  }

  SECTION("When the channel is closed after the iterator has been created") {
    auto it = std::begin(channel);
    channel.push(5);
    channel.close();
    std::copy(it, std::end(channel), std::back_inserter(actual));
    std::vector<int> const expected = {1, 2, 3, 4, 5};
    REQUIRE(actual == expected);
  }
}

TEST_CASE("More than one iterator can be used to pop from the channel at once") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.push(4);
  channel.push(5);
  channel.push(6);
  channel.close();

  auto it1 = std::begin(channel);
  auto it2 = std::begin(channel);
  REQUIRE(*it1 == 1);
  REQUIRE(*it2 == 2);

  ++it1;
  REQUIRE(*it1 == 3);

  ++it2;
  REQUIRE(*it2 == 4);

  ++it2;
  REQUIRE(*it2 == 5);

  ++it1;
  REQUIRE(*it1 == 6);
}
