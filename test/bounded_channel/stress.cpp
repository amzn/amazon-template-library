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

#include <algorithm>
#include <future>
#include <iterator>
#include <thread>
#include <vector>


TEST_CASE("Stress test with multiple producer and consumer threads") {
  amz::bounded_channel<int> channel{64};

  constexpr int N_INTEGERS = 10000;
  constexpr int N_PRODUCERS = 10;
  constexpr int N_CONSUMERS = 10;

  // Producers put integers in an increasing fashion into the channel.
  std::vector<std::thread> producers;
  for (int i = 0; i != N_PRODUCERS; ++i) {
    producers.emplace_back([&channel] {
      for (int i = 0; i != N_INTEGERS; ++i) {
        channel.push(i);
      }
    });
  }

  // Consumers read from the channel and populate their own local result
  // vector with whatever they extract from the channel.
  std::vector<std::future<std::vector<int>>> results;
  for (int i = 0; i != N_CONSUMERS; ++i) {
    results.push_back(std::async(std::launch::async, [&channel] {
      std::vector<int> result;
      for (int value : channel) {
        result.push_back(value);
      }
      return result;
    }));
  }

  // Block until all the producers are done. Blocking for the consumers will
  // happen in `future::get()` below. When the producers are all done, we
  // close the channel so that the consumers know we're done for good. If we
  // don't do that, the consumers will be stuck trying to pop() forever.
  for (auto& producer : producers) {
    producer.join();
  }
  channel.close();

  // Aggregate all the resulting vectors into the same vector and make sure we
  // properly funneled everything through the channel.
  std::vector<int> actual;
  for (auto& result : results) {
    std::vector<int> tmp = result.get();
    actual.insert(std::end(actual), std::begin(tmp), std::end(tmp));
  }
  std::sort(std::begin(actual), std::end(actual));

  std::vector<int> expected;
  for (int prod = 0; prod != N_PRODUCERS; ++prod) {
    for (int i = 0; i != N_INTEGERS; ++i) {
      expected.push_back(i);
    }
  }
  std::sort(std::begin(expected), std::end(expected));

  REQUIRE(actual == expected);
}
