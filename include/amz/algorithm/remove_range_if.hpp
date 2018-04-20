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

#ifndef AMZ_ALGORITHM_REMOVE_RANGE_IF_HPP
#define AMZ_ALGORITHM_REMOVE_RANGE_IF_HPP

#include <algorithm>
#include <iterator>

namespace amz {

// Given a range of elements delimited by two ForwardIterators `[first, last)`
// and predicates `equivalent` and `pred`, divides the range into the largest
// sub-ranges of equivalent elements (as determined by `equivalent`), removes
// those sub-ranges for which `pred` returns `true` and returns the a
// past-the-end iterator for the new end of the range.
//
// This is very similar to `std::remove_if`, except the elements are first
// grouped using `equivalent` and are removed as sub-ranges -- rather than as
// individual elements.
//
// Like for `std::remove_if`, removing is done by shifting (by means of
// move assignment) the elements in the input range in such a way that the
// elements that are not removed all appear contiguously as the subrange
// `[first, ret)`, where `ret` is the new end of the input range. Relative
// order of the elements that remain is preserved. Iterators in the range
// `[ret, last)` are still dereferenceable, but the elements they point to
// have valid but unspecified state.
//
// Note that the physical size of the container is unchanged. As such, a call
// to `remove_range_if` is typically followed by a call to a container's
// `erase` method, which reduces the physical size of the container to match
// its new logical size.
//
// This algorithm assumes:
// 1) `[first, last)` is a valid range;
// 2) the input range's `reference` type is MoveAssignable;
// 3) `pred(sub_first, sub_last)` is valid for all sub-ranges,
//    `[sub_first, sub_last)` of `[first, last)` that contains equivalent
//    elements;
// 4) `equivalent(*it1, *it2)` is valid for all ForwardIterators `it1` and
//    `it2` in the range `[first, last)`
// 5) and `equivalent` maintains an equivalence relation over the input
//    sequence.
//
// Performance guarantees:
// * Exactly `std::distance(first, last)-1` applications of `equivalent`
// * No more than `std::distance(first, last)-1` applications of `std::move`
// * Exactly `N` applications of `pred` where `N` is the number of sub-ranges
//
// Author: John McFarlane
template<typename ForwardIterator, typename EquivalenceRelation, typename RangePredicate>
ForwardIterator remove_range_if(ForwardIterator first, ForwardIterator last, EquivalenceRelation equivalent, RangePredicate pred) {
    auto write_pos = first;
    while (first != last) {
        // Establish sub-range of equivalent elements, `[first, sub_last)`.
        auto sub_last = std::find_if(std::next(first), last, [equivalent, first](auto const& element) {
            return !equivalent(*first, element);
        });

        // If the sub-range is *not* to be removed,
        if (!pred(first, sub_last)) {
            // if it needs to be shifted toward the start of the sequence,
            if (write_pos != first) {
                // move it.
                std::move(first, sub_last, write_pos);
            }

            std::advance(write_pos, std::distance(first, sub_last));
        }

        first = sub_last;
    }
    return write_pos;
}

}

#endif  // AMZ_ALGORITHM_REMOVE_RANGE_IF_HPP
