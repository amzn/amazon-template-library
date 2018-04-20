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


TEST_CASE("try_pop_for() succeeds when the channel is non-empty and open") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);

  int i = 999;
  REQUIRE(channel.try_pop_for(std::chrono::seconds{0}, i) == amz::channel_op_status::success);
  REQUIRE(i == 1);
}

TEST_CASE("try_pop_for() succeeds when the channel is non-empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.close();

  int i = 999;
  REQUIRE(channel.try_pop_for(std::chrono::seconds{0}, i) == amz::channel_op_status::success);
  REQUIRE(i == 1);
}

TEST_CASE("try_pop_for() returns `closed` when the channel is empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.close();
  int i = 999;
  REQUIRE(channel.try_pop_for(std::chrono::seconds{0}, i) == amz::channel_op_status::closed);
  REQUIRE(i == 999);
}

TEST_CASE("try_pop_for() returns `timeout` when the channel stays empty past the timeout") {
  amz::bounded_channel<int> channel{64};
  int i = 999;
  REQUIRE(channel.try_pop_for(std::chrono::milliseconds{1}, i) == amz::channel_op_status::timeout);
  REQUIRE(i == 999);
}

TEST_CASE("try_pop_for() succeeds when the channel becomes non-empty within the timeout") {
  amz::bounded_channel<int> channel{64};

  std::atomic<bool> started{false};
  std::thread t{[&] {
    started = true;
    // Try popping for so long that we will virtually never run into the
    // situation where the other thread does not have the time to push()
    // before we time out, which would incorrectly fail this test.
    int i = 999;
    REQUIRE(channel.try_pop_for(std::chrono::seconds{10}, i) == amz::channel_op_status::success);
    REQUIRE(i == 1);
  }};

  // Synchronize with the other thread to give it a chance to run.
  while (!started) std::this_thread::yield();

  // Unblock the other thread.
  channel.push(1);
  std::this_thread::yield();

  t.join();
}
