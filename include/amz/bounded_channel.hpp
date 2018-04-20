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

#ifndef AMZ_BOUNDED_CHANNEL_HPP
#define AMZ_BOUNDED_CHANNEL_HPP

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>

#include <boost/optional.hpp>


namespace amz {

//! Status code returned by many operations on channels and indicating the
//! state of the channel and the result of the operation.
enum class channel_op_status {
  //! Denotes that the operation was successful.
  success,

  //! Denotes that the operation failed because the channel is empty.
  empty,

  //! Denotes that the operation failed because the channel is full.
  full,

  //! Denotes that the operation failed because the channel has been closed.
  closed,

  //! Denotes that the operation failed because it could not finish within
  //! the allocated timeout.
  timeout
};

//! Multi-producer multi-consumer thread-safe channel.
//!
//! This class represents a queue that can be concurrently pushed to and popped
//! from from different threads, without explicit synchronization. The channel
//! is bounded, which means that trying to push a new element to the channel
//! when the channel is full will result in either blocking from the calling
//! thread (for blocking operations) or soft failure (for non blocking
//! operations). When one wishes to stop populating the channel, the channel
//! can be closed, which makes it impossible for anything new to be pushed to
//! it, but still allows for consumers to pop from it until it becomes empty
//! (known as draining the channel).
//!
//! The underlying container used by the channel can be customized with a
//! template argument. The only requirement is that the container can be
//! used as the underlying container for a `std::queue`.
//!
//! The design of this channel is heavily based on Boost.Fiber's channels.
//!
//! Note on performance and usability
//! =================================
//! This channel implementation uses locks for synchronization under the hood.
//! There exist open source implementations of lock-free MPMC queues with much
//! better performance characteristics than this channel. However, we could not
//! find any that was as simple to reason about and as ergonomic as this channel.
//! If your use case is such that only a reasonable number of elements are sent
//! through the channel (which typically means that work is distributed in a
//! coarse grained manner between producers and consumers), this channel is
//! probably vastly sufficient. Otherwise, you should probably use a different
//! implementation. As always, benchmarking is key.
//!
//!
//! Note on lifetime
//! ================
//! As usual in C++, a `bounded_channel` must outlive any reference to it.
//! However, it is customary for threads using a channel to require it being
//! closed before they can be joined. When that is the case, closing the
//! channel before joining the threads and destructing the channel is required.
//! In particular, closing the channel will not block until there are no more
//! uses of the channel.
//!
//! If tracking the last usage of the channel is very difficult to do statically,
//! a `std::shared_ptr` can be used to hold the channel. In this case, closing
//! the channel will be done automatically in the destructor of the channel,
//! which is going to be executed after the last usage of the channel is done.
//! Be aware that this can lead to subtle situations where a thread requires
//! the destructor to run in order for it to complete, but the destructor can
//! only be run if when the thread completes.
//!
//! For example, any thread using a `std::shared_ptr` to a channel that would
//! unconditionally `pop()` from the channel is bound to deadlock. Indeed, it
//! would require the channel to be closed in order for `pop()` to complete,
//! but it would require the thread to finish (and hence `pop()` to complete)
//! in order for the last `std::shared_ptr` to go out of scope and trigger the
//! closing of the channel. Hence, when in doubt, prefer closing the channel
//! explicitly and giving it a statically verifiable lifetime.
template <typename T, typename Container = std::deque<T>>
class bounded_channel {
public:
  static_assert(std::is_same<T, typename Container::value_type>{},
    "The value_type of the underlying container used to implement the channel "
    "must match the value_type provided to the channel itself.");

  using value_type = T;

  bounded_channel() = delete;

  //! Creates a `bounded_channel` with the given capacity.
  explicit bounded_channel(std::size_t capacity);

  bounded_channel(bounded_channel const&) = delete;
  bounded_channel(bounded_channel&&) = delete;
  bounded_channel& operator=(bounded_channel const&) = delete;
  bounded_channel& operator=(bounded_channel&&) = delete;

  //! Deactivates the channel, preventing new elements from being pushed to it.
  //!
  //! After the channel has been closed, no new values can be pushed into the
  //! channel. Threads blocked in any pushing or popping operation will be
  //! notified that the channel has been closed. Closing the channel is
  //! effectively a way to tell producers that they can't add anything more
  //! to the channel, while still allowing for consumers to consume elements
  //! that were put into the channel before it was closed.
  //!
  //! Note that closing the channel will not actually block the current thread,
  //! nor will it ensure that threads using the channel are joined. This must
  //! be ensured manually before the channel is destroyed.
  void close();

  //! Closes the channel.
  //!
  //! Since `close()` does not block or wait for all usages of the channel to
  //! go away, one must make sure that no threads are waiting on the channel
  //! (e.g. in a `pop()` or `push()` operation). In particular, this means
  //! that the notification of all waiting threads performed in `close()`
  //! should in fact notify no threads at all; otherwise the behavior is
  //! undefined.
  ~bounded_channel() { close(); }

  //! Pushes a new value into the channel and returns whether the operation
  //! succeeded, possibly blocking if the channel is full.
  //!
  //! - If the channel has been closed, returns `closed`.
  //! - If the channel is open and not full, enqueues the new value, notifies
  //!   at least one thread waiting on a popping operation and returns `success`.
  //! - If the channel is open but full, waits until either the channel is
  //!   closed (returns `closed`) or it is not full anymore (enqueues the new
  //!   value, notifies waiting threads and returns `success`).
  channel_op_status push(value_type const& va) { return this->push_impl(va); }
  channel_op_status push(value_type&& va)      { return this->push_impl(std::move(va)); }

  //! Tries pushing a new value into the channel and returns whether the
  //! operation succeeded, without blocking if the channel is full.
  //!
  //! - If the channel has been closed, returns `closed`.
  //! - If the channel is open and not full, enqueues the new value, notifies
  //!   at least one thread waiting on a popping operation and returns `success`.
  //! - If the channel is open but full, returns `full`.
  channel_op_status try_push(value_type const& va) { return this->try_push_impl(va); }
  channel_op_status try_push(value_type&& va)      { return this->try_push_impl(std::move(va)); }

  //! Tries pushing a new value into the channel for a given amount of time
  //! and returns whether the operation succeeded within the allocated time.
  //!
  //! - If the channel is closed, returns `closed`.
  //! - If the channel is open and not full, enqueues the new value, notifies
  //!   at least one thread waiting on a popping operation and returns `success`.
  //! - If the channel is open but full, waits until either the channel
  //!   becomes non-full (enqueues the new value, notifies waiting threads
  //!   and returns `success`), is closed (returns `closed`), or the timeout
  //!   expires (returns `timeout`).
  //!
  //! Note
  //! ====
  //! This function may block for longer than the given timeout due to
  //! scheduling or resource contention delays.
  template <typename Rep, typename Period>
  channel_op_status try_push_for(std::chrono::duration<Rep, Period> timeout_duration, value_type const& va) {
    return this->try_push_until(std::chrono::steady_clock::now() + timeout_duration, va);
  }
  template <typename Rep, typename Period>
  channel_op_status try_push_for(std::chrono::duration<Rep, Period> timeout_duration, value_type&& va) {
    return this->try_push_until(std::chrono::steady_clock::now() + timeout_duration, std::move(va));
  }

  //! Equivalent to `try_push_for`, but tries pushing until a specific point in
  //! time is reached instead of using a relative duration.
  template <typename Clock, typename Duration>
  channel_op_status try_push_until(std::chrono::time_point<Clock, Duration> timeout_time, value_type const& va) {
    return this->try_push_until_impl(timeout_time, va);
  }
  template <typename Clock, typename Duration>
  channel_op_status try_push_until(std::chrono::time_point<Clock, Duration> timeout_time, value_type&& va) {
    return this->try_push_until_impl(timeout_time, std::move(va));
  }

  //! Dequeues an element from the channel and returns whether the operation
  //! succeeded, possibly blocking if the channel is empty.
  //!
  //! - If the channel is not empty, immediately dequeues a value from the
  //!   channel into the output parameter `va`, notifies at least one thread
  //!   waiting on a pushing operation, and returns `success`.
  //! - If the channel is empty, waits until either one new item is pushed to
  //!   the channel (extracts value into `va`, notify waiting threads and
  //!   returns `success`), or the channel is closed (returns `closed`).
  //!
  //! Note
  //! ====
  //! This method can pop into any variable that can be assigned from an
  //! element in the channel. This allows popping into an optional value,
  //! for example.
  template <typename Value, typename =
    std::enable_if_t<std::is_assignable<Value&, value_type&&>::value>
  >
  channel_op_status pop(Value& va);

  //! Tries dequeuing an element from the channel and returns whether the
  //! operation succeeded, without blocking if the channel is empty.
  //!
  //! - If the channel is not empty, immediately dequeues a value from the
  //!   channel into the output parameter `va`, notifies at least one thread
  //!   waiting on a pushing operation, and returns `success`.
  //! - If the channel is empty and has been closed, returns `closed`.
  //! - If the channel is empty and has not been closed, returns `empty`.
  //!
  //! Note
  //! ====
  //! This method can pop into any variable that can be assigned from an
  //! element in the channel. This allows popping into an optional value,
  //! for example.
  template <typename Value, typename =
    std::enable_if_t<std::is_assignable<Value&, value_type&&>::value>
  >
  channel_op_status try_pop(Value& va);

  //! Tries dequeuing an element from the channel for a given amount of time
  //! and returns whether the operation succeeded within the allocated time.
  //!
  //! - If the channel is not empty, immediately dequeues a value from the
  //!   channel into the output parameter `va`, notifies at least one thread
  //!   waiting on a pushing operation, and returns `success`.
  //! - If the channel is empty and has been closed, returns `closed`.
  //! - If the channel is empty and has not been closed, waits until either
  //!   the channel becomes non-empty (dequeues a value, notifies waiting
  //!   threads and returns `success`), is closed (returns `closed`), or
  //!   the timeout expires (returns `timeout`).
  //!
  //! Notes
  //! =====
  //! - This function may block for longer than the given timeout due to
  //!   scheduling or resource contention delays.
  //! - This method can pop into any variable that can be assigned from an
  //!   element in the channel. This allows popping into an optional value,
  //!   for example.
  template <typename Rep, typename Period, typename Value, typename =
    std::enable_if_t<std::is_assignable<Value&, value_type&&>::value>
  >
  channel_op_status try_pop_for(std::chrono::duration<Rep, Period> timeout_duration, Value& va) {
    return this->try_pop_until(std::chrono::steady_clock::now() + timeout_duration, va);
  }

  //! Equivalent to `try_pop_for`, but tries popping until a specific point in
  //! time is reached instead of using a relative duration.
  template <typename Clock, typename Duration, typename Value, typename =
    std::enable_if_t<std::is_assignable<Value&, value_type&&>::value>
  >
  channel_op_status try_pop_until(std::chrono::time_point<Clock, Duration> timeout_time, Value& va);


  //! InputIterator associated to a channel.
  //!
  //! This InputIterator can be used to consume values from a channel. A valid
  //! iterator will compare equal to the past-the-end iterator when the channel
  //! associated to it has been closed and is empty.
  //!
  //! Notes
  //! =====
  //! - This iterator works in terms of `pop()`, which means that it will
  //!   produce values until the channel is empty, even if it has been closed.
  //!   In other words, a channel that is closed while being iterated upon will
  //!   get fully drained by the iterator.
  //! - This is an InputIterator, which means that it can't be used to make
  //!   more than a single pass over the channel. In other words, do not
  //!   assume that making a copy of an iterator will allow retrieving the
  //!   contents of the channel a second time -- once an iterator has been
  //!   used to extract a value from the channel, this value can never be
  //!   retrieved again using any other iterator over the same channel.
  class iterator;

  //! Returns an iterator to the beginning of the channel.
  iterator begin() { return iterator{*this}; }

  //! Returns a past-the-end iterator over the channel.
  iterator end() { return iterator{}; }

private:
  std::size_t const capacity_;
  std::queue<T, Container> queue_;
  // Note: timed_mutex is necessary because we use try_lock_for, and
  //       condition_variable_any is necessary because we use timed_mutex.
  using mutex_type = std::timed_mutex;
  mutex_type mutex_;
  std::condition_variable_any consumers_; // notified when we push something new; waited on by popping (consumer) threads
  std::condition_variable_any producers_; // notified when we pop something; waited on by pushing (producer) threads
  bool closed_;

  template <typename Value>
  channel_op_status push_impl(Value&& va);
  template <typename Value>
  channel_op_status try_push_impl(Value&& va);
  template <typename Value, typename TimePoint>
  channel_op_status try_push_until_impl(TimePoint timeout_time, Value&& va);

  // WARNING -- not thread safe
  bool is_full() const { return queue_.size() >= capacity_; }

  // WARNING -- not thread safe
  bool is_closed() const { return closed_; }

  // WARNING -- not thread safe
  bool is_empty() const { return queue_.empty(); }
};

//////////////////////////////////////////////////////////////////////////////
// Channel implementation
//////////////////////////////////////////////////////////////////////////////
template <typename T, typename Container>
bounded_channel<T, Container>::bounded_channel(std::size_t capacity)
  : capacity_{capacity}
  , queue_{}
  , mutex_{}
  , consumers_{}
  , producers_{}
  , closed_{false}
{ }

template <typename T, typename Container>
void bounded_channel<T, Container>::close() {
  {
    std::unique_lock<mutex_type> lock{mutex_};
    closed_ = true;
  }
  producers_.notify_all();
  consumers_.notify_all();
}

//
// push(), try_push(), try_push_until()
//
template <typename T, typename Container>
template <typename Value>
channel_op_status bounded_channel<T, Container>::push_impl(Value&& va) {
  std::unique_lock<mutex_type> lock{mutex_};
  producers_.wait(lock, [this] { return this->is_closed() || !this->is_full(); });
  if (is_closed()) {
    return channel_op_status::closed;
  } else {
    assert(!is_full());
    queue_.push(std::forward<Value>(va));
    lock.unlock();
    consumers_.notify_one();
    return channel_op_status::success;
  }
}

template <typename T, typename Container>
template <typename Value>
channel_op_status bounded_channel<T, Container>::try_push_impl(Value&& va) {
  std::unique_lock<mutex_type> lock{mutex_};
  if (is_closed()) {
    return channel_op_status::closed;
  } else if (!is_full()) {
    queue_.push(std::forward<Value>(va));
    lock.unlock();
    consumers_.notify_one();
    return channel_op_status::success;
  } else {
    assert(is_full());
    return channel_op_status::full;
  }
}

template <typename T, typename Container>
template <typename Value, typename TimePoint>
channel_op_status bounded_channel<T, Container>::try_push_until_impl(TimePoint timeout_time, Value&& va) {
  std::unique_lock<mutex_type> lock{mutex_, timeout_time}; // try to lock, but not past the timeout time
  if (!lock.owns_lock()) {
    return channel_op_status::timeout;
  }

  bool const timed_out = !producers_.wait_until(lock, timeout_time, [this] {
    return this->is_closed() || !this->is_full();
  });
  if (timed_out) {
    return channel_op_status::timeout;
  } else if (is_closed()) {
    return channel_op_status::closed;
  } else {
    assert(!is_full() && "we have not timed out and the channel is not closed; the channel should not be full");
    queue_.push(std::forward<Value>(va));
    lock.unlock();
    consumers_.notify_one();
    return channel_op_status::success;
  }
}

//
// pop(), try_pop(), try_pop_until()
//
template <typename T, typename Container>
template <typename Value, typename>
channel_op_status bounded_channel<T, Container>::pop(Value& va) {
  std::unique_lock<mutex_type> lock{mutex_};
  consumers_.wait(lock, [this] { return !this->is_empty() || this->is_closed(); });
  if (!is_empty()) {
    va = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    producers_.notify_one();
    return channel_op_status::success;
  } else {
    assert(is_closed());
    return channel_op_status::closed;
  }
}

template <typename T, typename Container>
template <typename Value, typename>
channel_op_status bounded_channel<T, Container>::try_pop(Value& va) {
  std::unique_lock<mutex_type> lock{mutex_};
  if (!is_empty()) {
    va = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    producers_.notify_one();
    return channel_op_status::success;
  } else if (is_closed()) {
    return channel_op_status::closed;
  } else {
    assert(is_empty());
    return channel_op_status::empty;
  }
}

template <typename T, typename Container>
template <typename Clock, typename Duration, typename Value, typename>
channel_op_status bounded_channel<T, Container>::try_pop_until(std::chrono::time_point<Clock, Duration> timeout_time, Value& va) {
  std::unique_lock<mutex_type> lock{mutex_, timeout_time}; // try to lock for no longer than the timeout
  if (!lock.owns_lock()) {
    return channel_op_status::timeout;
  }

  bool const timed_out = !consumers_.wait_until(lock, timeout_time, [this] {
    return !this->is_empty() || this->is_closed();
  });
  if (timed_out) {
    return channel_op_status::timeout;
  } else if (!is_empty()) {
    va = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    producers_.notify_one();
    return channel_op_status::success;
  } else {
    assert(is_closed());
    return channel_op_status::closed;
  }
}

//////////////////////////////////////////////////////////////////////////////
// Iterator implementation
//////////////////////////////////////////////////////////////////////////////
template <typename T, typename Container>
class bounded_channel<T, Container>::iterator {
private:
  bounded_channel<T, Container>* channel_; // nullptr if and only if the iterator is past-the-end
  boost::optional<T> value_;

public:
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = typename bounded_channel<T, Container>::value_type;
  using pointer = value_type*;
  using reference = value_type&;

  iterator() noexcept
    : channel_{nullptr}
    , value_{boost::none}
  { }

  explicit iterator(bounded_channel<T, Container>& channel) noexcept
    : channel_{&channel}
    , value_{boost::none}
  {
    ++*this;
  }

  iterator(iterator const& other)
    : channel_{other.channel_}
    , value_{other.value_}
  { }

  iterator& operator=(iterator const& other) {
    channel_ = other.channel_;
    value_ = other.value_;
    return *this;
  }

  friend bool operator==(iterator const& a, iterator const& b) {
    // Note that this is actually a too wide definition of equality, since
    // two iterators over the same channel that happen to point to equal
    // elements (even if those elements were in different positions in the
    // channel) will compare equal. It's a fact of life that InputIterators
    // do not play very well with copying and equality comparison.
    return a.channel_ == b.channel_ && a.value_ == b.value_;
  }

  friend bool operator!=(iterator const& a, iterator const& b) {
    return !(a == b);
  }

  iterator& operator++() {
    assert(channel_ != nullptr && "incrementing a past-the-end channel iterator");
    switch (channel_->pop(value_)) {
      case channel_op_status::success: {
        break;
      }
      case channel_op_status::closed: {
        channel_ = nullptr;
        value_ = boost::none;
        break;
      }
      default: {
        assert(false && "pop() should always return either success or closed");
        break;
      }
    };
    return *this;
  }

  iterator operator++(int) = delete; // This should not be provided for InputIterators

  reference operator*() noexcept {
    assert(value_ != boost::none);
    return *value_;
  }

  pointer operator->() noexcept {
    assert(value_ != boost::none);
    return &*value_;
  }
};

} // end namespace amz

#endif // include guard
