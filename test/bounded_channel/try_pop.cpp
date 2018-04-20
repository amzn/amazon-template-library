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


TEST_CASE("try_pop() succeeds when the channel is non-empty and open") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);

  int i = 999;
  REQUIRE(channel.try_pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);
}

TEST_CASE("try_pop() succeeds when the channel is non-empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);
  channel.close();

  int i = 999;
  REQUIRE(channel.try_pop(i) == amz::channel_op_status::success);
  REQUIRE(i == 1);
}

TEST_CASE("try_pop() returns `closed` when the channel is empty and closed") {
  amz::bounded_channel<int> channel{64};
  channel.close();
  int i = 999;
  REQUIRE(channel.try_pop(i) == amz::channel_op_status::closed);
  REQUIRE(i == 999);
}

TEST_CASE("try_pop() returns `empty` when the channel is empty but not closed") {
  amz::bounded_channel<int> channel{64};
  int i = 999;
  REQUIRE(channel.try_pop(i) == amz::channel_op_status::empty);
  REQUIRE(i == 999);
}
