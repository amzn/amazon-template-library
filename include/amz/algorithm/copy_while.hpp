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

#ifndef AMZ_ALGORITHM_COPY_WHILE_HPP
#define AMZ_ALGORITHM_COPY_WHILE_HPP

#include <iterator>
#include <utility>


namespace amz {

// Given a range of elements delimited by two InputIterators `[first, last)`,
// `copy_while` copies the prefix of that range that satisfies the given
// predicate into an OutputIterator. In other words, it copies elements of
// the range as long as the predicate is satisfied.
//
// The algorithm returns a pair containing:
// (1) an iterator to the first element of the range that was NOT copied
//    (or last if all the elements were copied)
// (2) an OutputIterator to one-past-the-last element that was copied in the
//     output range
//
// This algorithm assumes:
// (1) `[first, last)` is a valid range
// (2) `pred(*it)` is valid for all `it` in the range `[first, last)`
//
// IMPORTANT PERFORMANCE GUARANTEES:
// Given a range whose prefix satisfying the predicate has a length of `n`,
// this algorithm does at most
// (1) `n+1` increments and `n+1` dereferences of the `first` iterator
// (2) `n+1` applications of the predicate
//
// This is of utmost importance in cases where the increment and dereferencing
// of an iterator is costly. These guarantees should be considered part of the
// interface in the case of a future refactoring. In particular, this can't be
// replaced by `boost::algorithm::copy_while` in recent Boost versions, because
// these guarantees are not met (iterator dereferences are not cached).
//
// Author: Louis Dionne
template <typename InputIterator, typename OutputIterator, typename Predicate>
std::pair<InputIterator, OutputIterator>
copy_while(InputIterator first, InputIterator last, OutputIterator result, Predicate const& pred) {
  using value_type = typename std::iterator_traits<InputIterator>::value_type;
  for (; first != last; ++first) {
    // Cache *first to meet the requirements on the number of dereferences
    value_type const& v = *first;
    if (!pred(v)) break;
    *result++ = v;
  }
  return std::make_pair(first, result);
}

} // end namespace amz

#endif // include guard
