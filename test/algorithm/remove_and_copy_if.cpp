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

#include <amz/algorithm/remove_and_copy_if.hpp>

#include <array>
#include <iterator>
#include <utility>
#include <vector>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>


template <typename In, typename Out, typename Pred>
auto rmcp_if(In& in, Out& out, Pred const& pred) {
  return amz::remove_and_copy_if(in.begin(), in.end(), std::back_inserter(out), pred);
}

TEST_CASE("remove nothing") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.end());
}

TEST_CASE("remove first element") {
  std::array<int, 6> data = {{-1, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 5);
}

TEST_CASE("remove 2") {
  std::array<int, 6> data = {{-1, 1, -2, 3, 4, 5}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1, -2};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 4);
}

TEST_CASE("remove 3") {
  std::array<int, 6> data = {{-1, 1, -2, -3, 4, 5}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1, -2, -3};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 3);
}

TEST_CASE("remove 4") {
  std::array<int, 6> data = {{-1, 1, -2, -3, -4, 5}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1, -2, -3, -4};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 2);
}

TEST_CASE("remove 5") {
  std::array<int, 6> data = {{-1, -2, -3, -4, -5, -6}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1, -2, -3, -4, -5, -6};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin());
}

TEST_CASE("remove 6") {
  std::array<int, 6> data = {{1, 2, -3, 4, 5, 6}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-3};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 5);
}

TEST_CASE("remove 7") {
  std::array<int, 6> data = {{1, 2, -3, -4, 5, 6}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-3, -4};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin() + 4);
}

TEST_CASE("corner case: empty input range") {
  std::array<int, 0> data;
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int) { return true; });

  REQUIRE(actual.empty());
  REQUIRE(result.first == data.end());
}

TEST_CASE("corner case: singleton input range; don't remove anything") {
  std::array<int, 1> data = {{-1}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {-1};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.begin());
}

TEST_CASE("corner case: singleton input range, remove everything") {
  std::array<int, 1> data = {{1}};
  std::vector<int> actual;
  auto result = rmcp_if(data, actual, [](int x) { return x < 0; });

  std::vector<int> expected = {};
  REQUIRE(actual == expected);
  REQUIRE(result.first == data.end());
}
