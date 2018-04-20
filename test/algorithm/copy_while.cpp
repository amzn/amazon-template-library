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

#include <amz/algorithm/copy_while.hpp>

#include <array>
#include <iterator>
#include <utility>
#include <vector>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>


// An iterator that counts the number of dereferences and increments are done
// on it. Useful in the tests below.
template <typename Iterator>
struct counting_iterator : std::iterator_traits<Iterator> {
  counting_iterator(Iterator it, int& increments, int& dereferences)
    : iterator(it), increments(increments), dereferences(dereferences)
  { }

  counting_iterator(counting_iterator const& other) = default;

  Iterator iterator;
  int& increments;
  int& dereferences;

  counting_iterator& operator++() {
    ++increments;
    ++iterator;
    return *this;
  }

  counting_iterator operator++(int) {
    counting_iterator copy = *this;
    ++(*this);
    return copy;
  }

  typename std::iterator_traits<Iterator>::reference operator*() {
    ++dereferences;
    return *iterator;
  }

  friend bool operator==(counting_iterator const& a, counting_iterator const& b)
  { return a.iterator == b.iterator; }
  friend bool operator!=(counting_iterator const& a, counting_iterator const& b)
  { return !(a == b); }
};

template <typename T>
auto less_than(T const& t) {
  return [=](T const& x) { return x < t; };
}

TEST_CASE("test with an empty range") {
  std::array<int, 0> data;
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), [](int) { return true; });
  REQUIRE(actual.empty());
}

TEST_CASE("test case 0") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(0));

  std::vector<int> expected;
  REQUIRE(actual == expected);
}

TEST_CASE("test case 1") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(1));

  std::vector<int> expected;
  expected.push_back(0);
  REQUIRE(actual == expected);
}

TEST_CASE("test case 2") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(2));

  std::vector<int> expected = {0, 1};
  REQUIRE(actual == expected);
}

TEST_CASE("test case 3") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(3));

  std::vector<int> expected = {0, 1, 2};
  REQUIRE(actual == expected);
}

TEST_CASE("test case 4") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(4));

  std::vector<int> expected = {0, 1, 2, 3};
  REQUIRE(actual == expected);
}

TEST_CASE("test case 5") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(5));

  std::vector<int> expected = {0, 1, 2, 3, 4};
  REQUIRE(actual == expected);
}

TEST_CASE("test case 6") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(6));
  std::vector<int> expected = {0, 1, 2, 3, 4, 5};
  REQUIRE(actual == expected);
}

TEST_CASE("test case 7") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), [](int) { return true; });
  std::vector<int> expected = {0, 1, 2, 3, 4, 5};
  REQUIRE(actual == expected);
}

TEST_CASE("check returned pair") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  std::vector<int> actual;
  using BackInserter =std::back_insert_iterator<std::vector<int>>;
  std::pair<int*, BackInserter> result =
    amz::copy_while(data.begin(), data.end(), std::back_inserter(actual), less_than(3));

  std::vector<int> expected = {0, 1, 2};
  REQUIRE(actual == expected);

  REQUIRE(result.first == data.begin() + 3);
  *result.second = 999;
  expected.push_back(999);
  REQUIRE(actual == expected);
}

TEST_CASE("check exact number of increments and dereferences") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  int first_increments = 0, first_dereferences = 0;
  counting_iterator<int*> first(data.begin(), first_increments, first_dereferences);

  int last_increments = 0, last_dereferences = 0;
  counting_iterator<int*> last(data.end(), last_increments, last_dereferences);

  std::vector<int> result;
  amz::copy_while(first, last, std::back_inserter(result), less_than(3));
  REQUIRE(first_increments == 3);   // first does not reach the end
  REQUIRE(first_dereferences == 4); //

  REQUIRE(last_increments == 0);   // Should obviously be met
  REQUIRE(last_dereferences == 0); //
}

TEST_CASE("increments and dereferences all the range") {
  std::array<int, 6> data = {{0, 1, 2, 3, 4, 5}};
  int first_increments = 0, first_dereferences = 0;
  counting_iterator<int*> first(data.begin(), first_increments, first_dereferences);

  int last_increments = 0, last_dereferences = 0;
  counting_iterator<int*> last(data.end(), last_increments, last_dereferences);

  std::vector<int> result;
  amz::copy_while(first, last, std::back_inserter(result), [](int) { return true; });
  REQUIRE(first_increments == 6);   // first reaches the end
  REQUIRE(first_dereferences == 6); //

  REQUIRE(last_increments == 0);   // Should obviously be met
  REQUIRE(last_dereferences == 0); //
}
