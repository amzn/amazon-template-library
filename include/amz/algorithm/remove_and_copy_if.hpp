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

#ifndef AMZ_ALGORITHM_REMOVE_AND_COPY_IF_HPP
#define AMZ_ALGORITHM_REMOVE_AND_COPY_IF_HPP

#include <algorithm>
#include <iterator>
#include <utility>


namespace amz {

// Given a range of elements delimited by two ForwardIterators `[first, last)`
// and a predicate `pred`, `remove_and_copy_if` copies the elements for which
// `pred` is satisfied to the specified output range and removes them from the
// input range.
//
// This is very similar to `std::remove_if`, except the elements that are
// removed are also copied to a specified output range. This is also similar
// to `std::remove_copy_if`, except the input range is filtered in place.
//
// Like for `std::remove_if`, removing is done by shifting (by means of
// assignment) the elements in the input range in such a way that the
// elements that are not removed all appear contiguously as the subrange
// `[first, ret)`, where `ret` is the new end of the input range. Relative
// order of the elements that remain is preserved. Iterators in the range
// `[ret, last)` are still dereferenceable, but the elements they point to
// have valid but unspecified state.
//
// Note that the physical size of the container is unchanged. As such, a call
// to `remove_and_copy_if` is typically followed by a call to a container's
// `erase` method, which reduces the physical size of the container to match
// its new logical size.
//
// This algorithm returns a pair containing:
// (1) the iterator `ret` defined above, as would be returned by an equivalent
//     call to `std::remove_if`
// (2) an OutputIterator to one-past-the-last element that was copied in
//     the output range, as would be returned by an equivalent call to
//     `std::remove_copy_if`
//
// This algorithm assumes:
// (1) `[first, last)` is a valid range
// (2) The input and output ranges do not overlap
// (3) The input range's `reference` type is MoveAssignable
// (4) `pred(*it)` is valid for all `it` in the range `[first, last)`
// (5) The output range has at least `std::count_if(first, last, pred)` elements
//
// Performance guarantees:
// Given a range of length `n`, this algorithm does exactly `n` applications
// of the predicate and at most `n` copies.
//
// TODO: Consider using move assignment to move elements around instead of
//       copying them.
//
// Author: Louis Dionne
template <typename ForwardIt, typename OutputIt, typename Predicate>
std::pair<ForwardIt, OutputIt>
remove_and_copy_if(ForwardIt first, ForwardIt last, OutputIt result, Predicate const& pred) {
  using value_type = typename std::iterator_traits<ForwardIt>::value_type;
  ForwardIt compress = std::find_if(first, last, pred);
  for (first = compress; first != last; ++first) {
    value_type const& v = *first;
    if (pred(v))
      *result++ = v;
    else
      *compress++ = v;
  }
  return std::make_pair(compress, result);
}

} // end namespace amz

#endif // include guard
