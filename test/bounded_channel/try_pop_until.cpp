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

#include <chrono>


//
// Note: Since `try_pop_until()` and `try_pop_for()` are basically implemented
//       the same, we don't bother testing this one too much. We roughly just
//       make sure it compiles.
//

TEST_CASE("try_pop_until() succeeds when the channel is non-empty and open") {
  amz::bounded_channel<int> channel{64};
  channel.push(1);

  int i = 999;
  REQUIRE(channel.try_pop_until(std::chrono::steady_clock::now(), i) == amz::channel_op_status::success);
  REQUIRE(i == 1);
}
