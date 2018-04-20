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

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include <chrono>


TEST_CASE("popping a value into something that is only assignable from the value_type of the channel works") {
  amz::bounded_channel<int> channel{64};
  boost::optional<int> result = boost::none;

  SECTION("pop()") {
    channel.push(9);
    channel.pop(result);
    REQUIRE(result == 9);
  }

  SECTION("try_pop()") {
    channel.try_pop(result);
    REQUIRE(result == boost::none);

    channel.push(9);
    channel.try_pop(result);
    REQUIRE(result == 9);
  }

  SECTION("try_pop_for()") {
    channel.try_pop_for(std::chrono::milliseconds{1}, result);
    REQUIRE(result == boost::none);

    channel.push(9);
    channel.try_pop_for(std::chrono::milliseconds{1}, result);
    REQUIRE(result == 9);
  }

  SECTION("try_pop_until()") {
    auto const future = std::chrono::steady_clock::now() + std::chrono::milliseconds{1};
    channel.try_pop_until(future, result);
    REQUIRE(result == boost::none);

    channel.push(9);
    channel.try_pop_until(future, result);
    REQUIRE(result == 9);
  }
}
