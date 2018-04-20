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

#include <atomic>
#include <thread>


TEST_CASE("pop() succeeds when the channel is non-empty and open") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);

  int i = 999;
  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);

  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 2);
}

TEST_CASE("pop() succeeds when the channel is non-empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.push(2);
  channel.close();

  int i = 999;
  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);

  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 2);
}

TEST_CASE("pop() returns `closed` when the channel is empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);

  int i = 999;
  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);

  channel.close();
  REQUIRE(channel.pop(i) == amz::channel_op_status::closed);
  REQUIRE(i == 1);
}

TEST_CASE("pop() blocks until a value becomes available") {
  amz::bounded_channel<int> channel{64};

  std::atomic<bool> started{false};
  std::thread t{[&] {
    started = true;
    int i = 999;
    REQUIRE(channel.pop(i) == amz::channel_op_status::success);
    REQUIRE(i == 1);
  }};

  // Wait until the thread has started.
  while (!started) std::this_thread::yield();

  // Here, we assume the thread is blocked in `pop()`, and this will unblock it.
  channel.push(1);

  t.join();
}
