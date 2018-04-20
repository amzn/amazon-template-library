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


TEST_CASE("push() succeeds when the channel is non-full and open") {
  amz::bounded_channel<int> channel{64};
  REQUIRE(channel.push(1) == amz::channel_op_status::success);

  int i = 999;
  channel.pop(i);
  REQUIRE(i == 1);
}

TEST_CASE("push() returns `closed` when the channel is non-full and closed") {
  amz::bounded_channel<int> channel{64};
  channel.close();
  REQUIRE(channel.push(1) == amz::channel_op_status::closed);
}

TEST_CASE("push() returns `closed` when the channel is full and closed") {
  amz::bounded_channel<int> channel{3};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.close();
  REQUIRE(channel.push(4) == amz::channel_op_status::closed);
}

TEST_CASE("push() blocks until the channel becomes non-full") {
  amz::bounded_channel<int> channel{2};
  channel.push(1);
  channel.push(2);

  std::atomic<bool> started{false};
  std::thread t{[&] {
    started = true;
    REQUIRE(channel.push(3) == amz::channel_op_status::success);
  }};

  // Wait until the thread has started.
  while (!started) std::this_thread::yield();

  // Here, we assume the thread is blocked in `push()`, and this will unblock it.
  int i = 999;
  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);

  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 2);

  REQUIRE(channel.pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 3);

  t.join();
}
