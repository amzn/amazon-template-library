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

#include <list>
#include <vector>


TEST_CASE("The underlying container of the channel can be customized") {
  using Container = std::list<int>;
  amz::bounded_channel<int, Container> channel{64};
  channel.push(1);
  channel.push(2);
  channel.push(3);
  channel.push(4);
  channel.push(5);
  channel.close();

  std::vector<int> actual;
  for (int i : channel) {
    actual.push_back(i);
  }

  std::vector<int> const expected = {1, 2, 3, 4 ,5};
  REQUIRE(actual == expected);
}
