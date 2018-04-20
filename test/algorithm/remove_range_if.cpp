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

#include <amz/algorithm/remove_range_if.hpp>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <vector>

TEST_CASE("remove_range_if empty") {
    auto sequence = std::vector<int>{};

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [](auto, auto) {
        FAIL("function should not be visited for empty sequence");
        return false;
    }, [](auto, auto) {
        FAIL("function should not be visited for empty sequence");
        return false;
    });

    CHECK(old_end == new_end);
}

TEST_CASE("remove_range_if one element keep") {
    auto sequence = std::array<char, 1>{{'a'}};
    auto const expected_sequence = sequence;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [](auto, auto) {
        FAIL("function should not be visited for empty sequence");
        return false;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        CHECK(range_first == std::begin(sequence));
        CHECK(range_last == old_end);
        return false;
    });

    CHECK(old_end == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(p_call_count == 1);
}

TEST_CASE("remove_range_if one element remove") {
    auto sequence = std::vector<std::unique_ptr<int>>{};
    sequence.emplace_back(new int(7654321));

    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [](auto const&, auto const&) {
        FAIL("function should not be visited for empty sequence");
        return false;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        CHECK(range_first == std::begin(sequence));
        CHECK(range_last == old_end);
        return true;
    });

    CHECK(std::begin(sequence) == new_end);
    CHECK(1 == sequence.size());
    CHECK(7654321 == *sequence[0]);
    CHECK(p_call_count == 1);
}

TEST_CASE("remove_range_if two same elements keep") {
    auto sequence = std::list<short>{{123, 123}};
    auto const expected_sequence = sequence;
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        CHECK(123 == element1);
        CHECK(123 == element2);
        return true;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        CHECK(range_first == std::begin(sequence));
        CHECK(range_last == std::end(sequence));
        return false;
    });

    CHECK(old_end == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 1);
}

TEST_CASE("remove_range_if two same elements drop") {
    auto sequence = std::list<short>{{123, 123}};
    auto const expected_sequence = std::list<short>{{123, 123}};
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        CHECK(123 == element1);
        CHECK(123 == element2);
        return true;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        CHECK(range_first == std::begin(sequence));
        CHECK(range_last == std::end(sequence));
        return true;
    });

    CHECK(std::begin(sequence) == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 1);
}

TEST_CASE("remove_range_if two different elements keep") {
    auto sequence = std::list<short>{{123, 456}};
    auto const expected_sequence = sequence;
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        CHECK(sequence.front() == std::min(element1, element2));
        CHECK(sequence.back() == std::max(element1, element2));
        return false;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        switch (p_call_count) {
            case 1:
                CHECK(range_first == std::begin(sequence));
                CHECK(range_last == std::next(range_first));
                break;
            case 2:
                CHECK(range_first == std::next(std::begin(sequence)));
                CHECK(range_last == std::next(range_first));
                break;
        }
        return false;
    });

    CHECK(old_end == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 2);
}

TEST_CASE("remove_range_if two different elements drop first") {
    auto sequence = std::list<short>{{123, 456}};
    auto const expected_sequence = std::list<short>{{456}};
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        CHECK(sequence.front() == std::min(element1, element2));
        CHECK(sequence.back() == std::max(element1, element2));
        return false;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        switch (p_call_count) {
            case 1:
                CHECK(range_first == std::begin(sequence));
                CHECK(range_last == std::next(range_first));
                return true;
            case 2:
                CHECK(range_first == std::next(std::begin(sequence)));
                CHECK(range_last == std::next(range_first));
                return false;
        }
        FAIL("called too many times?");
        return false;
    });

    CHECK(std::prev(old_end) == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 2);
}

TEST_CASE("remove_range_if two different elements drop second") {
    auto sequence = std::list<short>{{123, 456}};
    auto const expected_sequence = std::list<short>{{123}};
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        CHECK(sequence.front() == std::min(element1, element2));
        CHECK(sequence.back() == std::max(element1, element2));
        return false;
    }, [&](auto range_first, auto range_last) {
        ++ p_call_count;
        switch (p_call_count) {
            case 1:
                CHECK(range_first == std::begin(sequence));
                CHECK(range_last == std::next(range_first));
                return false;
            case 2:
                CHECK(range_first == std::next(std::begin(sequence)));
                CHECK(range_last == std::next(range_first));
                return true;
        }
        FAIL("called too many times?");
        return false;
    });

    CHECK(std::prev(old_end) == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 2);
}

TEST_CASE("remove_range_if large varied string") {
    // choose ranges using case-insensitive equality;
    // filter ranges that begin with upper case
    using namespace std::literals;
    auto sequence = "AaAgRRRRrrrjJJJ843kaniu32NFNNFFFFggggg"s;
    auto const expected_sequence = "gjJJJ843kaniu32ggggg"s;
    auto e_call_count = 0;
    auto p_call_count = 0;

    auto old_end = std::end(sequence);
    auto new_end = amz::remove_range_if(std::begin(sequence), old_end, [&](auto element1, auto element2) {
        ++ e_call_count;
        return std::tolower(element1) == std::tolower(element2);
        return false;
    }, [&](auto range_first, auto) {
        ++ p_call_count;
        return std::isupper(*range_first);
    });

    CHECK(std::next(std::begin(sequence), 20) == new_end);
    CHECK(std::equal(std::begin(expected_sequence), std::end(expected_sequence), std::begin(sequence)));
    CHECK(e_call_count == sequence.size() - 1);
    CHECK(p_call_count == 19);
}
