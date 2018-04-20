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

#ifndef AMZ_CALL_HPP
#define AMZ_CALL_HPP

#include <boost/blank.hpp>
#include <boost/optional.hpp>

#include <chrono>
#include <cstddef>
#include <type_traits>
#include <utility>


namespace amz {

//! Executes the Callable object `f` if the LimitingFlag `flag` is active,
//! otherwise do nothing.
//!
//! A `LimitingFlag` is an object with a single method, `bool active()`, which
//! returns whether some action should be taken. They can be used in conjunction
//! with `amz::call` to limit how many times or how often a function should
//! be called (i.e. an action should be taken). This is similar to how
//! `std::once_flag` is combined with `std::call_once` to ensure that
//! a function is not called more than once (even across threads), but
//! `LimitingFlag`s are more general (and are not required to be thread safe).
//!
//! @param flag
//!        A LimitingFlag to determine whether `f` should be executed. `f`
//!        is executed if `flag.active()` is true, otherwise nothing is done.
//!
//! @param f
//!        Callable to be executed.
//!
//! @param args...
//!        Arguments forwarded to the Callable `f` if it is executed.
//!
//! @returns
//!        An optional containing the result of calling `f` is a call was
//!        made, and an empty optional otherwise. If `f` returns `void`,
//!        `amz::call` returns an optional containing a `boost::blank`.
//!
//! Example usage
//! -------------
//! Let's say you have a loop that processes some data, and which may be
//! waiting for data at times. You'd like to notify the user that the
//! program is alive and doing something, but you only want to do that
//! every so often. One way to do this is to use the `amz::at_most_every`
//! flag to limit the rate at which a message is printed:
//! ```
//! int main() {
//!   auto at_most_every_second = amz::at_most_every{std::chrono::seconds{1}};
//!   while (is_waiting_for_data()) {
//!     if (process_data()) {
//!       break;
//!     }
//!
//!     // Called no more than once per second.
//!     amz::call(at_most_every_second, []{
//!       std::cout << "I'm still alive..." << std::endl;
//!     });
//!   }
//! }
//! ```
//!
//! However, notice that if any part of the loop blocks for longer than a
//! second, the message will not be printed during that second. In other
//! words, these call flags allow _limiting_ how often a call is performed,
//! but it does not allow ensuring that the call is performed at _least_
//! some number of times.
template <typename LimitingFlag, typename Callable, typename ...Args,
          typename = decltype(std::declval<LimitingFlag&>().active()) /* only enable when the flag is a LimitingFlag */,
          typename Result = decltype(std::declval<Callable&&>()(std::declval<Args&&>()...)),
          typename = std::enable_if_t<!std::is_void<Result>::value>>
boost::optional<Result>
call(LimitingFlag& flag, Callable&& f, Args&& ...args) {
  if (flag.active()) {
    return std::forward<Callable>(f)(std::forward<Args>(args)...);
  } else {
    return boost::none;
  }
}

template <typename LimitingFlag, typename Callable, typename ...Args,
          typename = decltype(std::declval<LimitingFlag&>().active()) /* only enable when the flag is a LimitingFlag */,
          typename Result = decltype(std::declval<Callable&&>()(std::declval<Args&&>()...)),
          typename = std::enable_if_t<std::is_void<Result>::value>>
boost::optional<boost::blank>
call(LimitingFlag& flag, Callable&& f, Args&& ...args) {
  if (flag.active()) {
    std::forward<Callable>(f)(std::forward<Args>(args)...);
    return boost::blank{};
  } else {
    return boost::none;
  }
}

//! LimitingFlag that is active at most once per period of a given duration.
//!
//! This LimitingFlag allows ensuring that an action is not taken more often
//! than once every so often.
struct at_most_every {
  explicit at_most_every(std::chrono::steady_clock::duration interval)
    : interval_{interval}
  {
    auto one_tick = std::chrono::steady_clock::duration::zero();
    ++one_tick;
    last_active_ = std::chrono::steady_clock::now() - (interval + one_tick);
  }

  bool active() {
    auto const now = std::chrono::steady_clock::now();
    if (now > last_active_ + interval_) {
      last_active_ = now;
      return true;
    }
    return false;
  }

private:
  std::chrono::steady_clock::time_point last_active_;
  std::chrono::steady_clock::duration const interval_;
};

//! LimitingFlag that is active at most a given number of times, and then
//! becomes inactive forever.
//!
//! This LimitingFlag allows ensuring that an action is taken at most a
//! certain number of times, and no more.
struct at_most {
  explicit at_most(std::size_t times)
    : max_activations_{times}
    , n_activations_{0}
  { }

  bool active() {
    if (n_activations_ < max_activations_) {
      ++n_activations_;
      return true;
    }

    return false;
  }

private:
  std::size_t const max_activations_;
  std::size_t n_activations_;
};

} // end namespace amz

#endif // include guard
