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


TEST_CASE("try_push() succeeds when the channel is non-full and open") {
  amz::bounded_channel<int> channel{64};
  REQUIRE(channel.try_push(1) == amz::channel_op_status::success);

  int i = 999;
  channel.pop(i);
  REQUIRE(i == 1);
}

TEST_CASE("try_push() returns `closed` when the channel is non-full and closed") {
  amz::bounded_channel<int> channel{64};
  channel.close();
  REQUIRE(channel.try_push(1) == amz::channel_op_status::closed);
}

TEST_CASE("try_push() returns `closed` when the channel is full and closed") {
  amz::bounded_channel<int> channel{3};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.close();
  REQUIRE(channel.try_push(4) == amz::channel_op_status::closed);
}

TEST_CASE("try_push() returns `full` when the channel is full and open") {
  amz::bounded_channel<int> channel{2};
  channel.push(1);
  channel.push(2);
  REQUIRE(channel.try_push(3) == amz::channel_op_status::full);

  // also make sure nothing was pushed
  int i = 999;
  channel.pop(i);
  REQUIRE(i == 1);
  channel.pop(i);
  REQUIRE(i == 2);

  REQUIRE(channel.try_pop(i) == amz::channel_op_status::empty);
}
