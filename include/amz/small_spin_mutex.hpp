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

#ifndef AMZ_SMALL_SPIN_MUTEX_HPP
#define AMZ_SMALL_SPIN_MUTEX_HPP

#include <atomic>


namespace amz {

// Lightweight non-recursive spin mutex with strict size guarantees.
//
// Like for all spin mutexes, this is almost certainly not what you want.
// Indeed, this spin mutex will cause a thread seeking to acquire the mutex
// to busy-wait without doing any useful work, and without sleeping so that
// another thread can make progress. However, in very few situations, a spin
// mutex can be used to make fine-grained access to shared data thread-safe
// whilst minimizing the overhead of locking.
//
// This mutex implementation is not recursive, which means that a thread
// may not acquire the mutex when it already owns it.
//
// THIS CLASS PROVIDES THE FOLLOWING GUARANTEES, WHICH MUST BE WEAKENED UNDER
// NO CIRCUMSTANCES:
// - The size of the class is at most one byte.
// - Only true-atomic operations are used internally, i.e. we never
//   fall back to a system-level mechanism for locking.
// - The `lock()` method will busy-wait without yielding.
// - The class is TriviallyDestructible.
//
// These guarantees make this spin mutex implementation suitable for scenarios
// where other locking mechanisms or other spin mutex implementations would be
// untenable. In particular, it can be used when size is a prime concern (e.g.
// when reusing free bytes in an existing data structure), or when yielding
// to the system is unacceptable (e.g. because of latency constraints).
//
// Note that in most cases, the need for such fine-grained locking hints at
// the fact that RCU[1] should be used instead.
//
// [1]: https://en.wikipedia.org/wiki/Read-copy-update
struct small_spin_mutex {
  small_spin_mutex() = default;
  small_spin_mutex(small_spin_mutex const&) = delete;
  small_spin_mutex(small_spin_mutex&&) = delete;

  small_spin_mutex& operator=(small_spin_mutex const&) = delete;
  small_spin_mutex& operator=(small_spin_mutex&&) = delete;

  // Try locking the mutex, and return whether it succeeded.
  //
  // If the mutex is already locked, this method will return immediately
  // without blocking. To block the calling thread until the mutex can
  // be acquired, use `lock()` instead.
  //
  // @returns
  //    True if the mutex has been acquired and is now owned by the calling
  //    thread, and false otherwise.
  //
  // TODO: In C++17, apply the [[nodiscard]] attribute to make sure people
  //       do not misuse this.
  bool try_lock() noexcept {
    return !flag_.test_and_set(std::memory_order_acquire);
  }

  // Blocks until the calling thread acquires the mutex.
  //
  // This method will busy-wait until it can acquire the mutex. There is
  // no back off policy for yielding after a certain number of attempts
  // have been made.
  //
  // When this method returns, the calling thread has acquired the mutex.
  //
  // The behavior is undefined if this method is called while the calling
  // thread already owns the mutex (concretely, you should expect a deadlock).
  void lock() noexcept {
    while (!try_lock())
      /* spin */;
  }

  // Unlocks the mutex.
  //
  // The behavior is undefined if the mutex was not owned by the calling
  // thread.
  void unlock() noexcept {
    flag_.clear(std::memory_order_release);
  }

private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

} // end namespace amz

#endif // include guard
