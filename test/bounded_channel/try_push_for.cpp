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
#include <chrono>
#include <thread>


TEST_CASE("try_push_for() succeeds when the channel is non-full and open") {
  amz::bounded_channel<int> channel{64};
  REQUIRE(channel.try_push_for(std::chrono::seconds{0}, 1) == amz::channel_op_status::success);

  int i = 999;
  channel.pop(i);
  REQUIRE(i == 1);
}

TEST_CASE("try_push_for() returns `closed` when the channel is non-full and closed") {
  amz::bounded_channel<int> channel{64};
  channel.close();
  REQUIRE(channel.try_push_for(std::chrono::seconds{0}, 1) == amz::channel_op_status::closed);
}

TEST_CASE("try_push_for() returns `closed` when the channel is full and closed") {
  amz::bounded_channel<int> channel{3};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.close();
  REQUIRE(channel.try_push_for(std::chrono::seconds{0}, 4) == amz::channel_op_status::closed);
}

TEST_CASE("try_push_for() returns `timeout` when the channel stays full past the timeout") {
  amz::bounded_channel<int> channel{3};
  channel.push(1);
  channel.push(2);
  channel.push(3);

  REQUIRE(channel.try_push_for(std::chrono::milliseconds{1}, 4) == amz::channel_op_status::timeout);
}

TEST_CASE("try_push_for() succeeds when the channel becomes non-full within the timeout") {
  amz::bounded_channel<int> channel{2};
  channel.push(1);
  channel.push(2);
  REQUIRE(channel.try_push(888) == amz::channel_op_status::full); // the channel is now full with [1, 2] in it

  std::atomic<bool> started{false};
  std::thread t{[&] {
    started = true;
    // Try pushing for so long that we will virtually never run into the
    // situation where the other thread does not have the time to pop()
    // before we time out, which would incorrectly fail this test.
    REQUIRE(channel.try_push_for(std::chrono::seconds{10}, 3) == amz::channel_op_status::success);
  }};

  // Synchronize with the other thread to give it a chance to run.
  while (!started) std::this_thread::yield();

  // Unblock the other thread by popping something off the channel.
  int i = 999;
  channel.pop(i);
  REQUIRE(i == 1);
  std::this_thread::yield();

  channel.pop(i);
  REQUIRE(i == 2);

  channel.pop(i);
  REQUIRE(i == 3);

  t.join();
}
