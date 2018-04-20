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

#ifndef AMZ_DEFERRED_RECLAMATION_ALLOCATOR_HPP
#define AMZ_DEFERRED_RECLAMATION_ALLOCATOR_HPP

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>


namespace amz {

namespace detail {
  // Equivalent to `std::destroy_at` in C++17.
  template <typename T>
  void destroy_at(T* ptr) {
    ptr->~T();
  }

  // Equivalent to `std::destroy_n`, in C++17.
  template <typename ForwardIterator, typename Size>
  ForwardIterator destroy_n(ForwardIterator first, Size n) {
    for (; n > 0; (void)++first, --n) {
      detail::destroy_at(std::addressof(*first));
    }
    return first;
  }

  struct opportunistic_t { };
  struct exhaustive_t { };
} // end namespace detail

struct purge_mode {
  //! Tag used to specify that `purge()` should stop as soon as an entry in
  //! the _delay list_ is not ready to be purged.
  static constexpr detail::opportunistic_t opportunistic{};

  //! Tag used to specify that `purge()` should purge all the elements in the
  //! _delay list_, waiting as needed to purge elements that are not ready yet.
  static constexpr detail::exhaustive_t exhaustive{};
};

//! Allocator adaptor that defers object destruction and memory reclamation
//! until a fixed time period has elapsed.
//!
//! When modifying shared data that is concurrently being accessed by other
//! threads, it is sometimes necessary to delay destructive operations (like
//! object destruction and memory reclamation) to a time where no other threads
//! may be using that data. This general pattern is known as read-copy-update
//! (RCU) [1], with many possible implementations. In a nutshell, the pattern
//! is typically as follows:
//! 1. Make the shared data unavailable to readers, typically (but not
//!    necessarily) by atomically replacing a pointer by another one pointing
//!    to a newer version of the data. After this, no new readers may gain a
//!    reference to the "old" data.
//! 2. Wait for all the previous readers to be done with their reference to
//!    the "old" data. "Previous" readers are readers that existed before
//!    step 1, and which may have obtained a reference to the "old" data
//!    before it was made unavailable.
//! 3. At this point, there cannot be any readers holding references to the
//!    "old" data, and so the destructive operation (typically destruction
//!    and memory reclamation) can be carried.
//!
//! Knowing exactly when all previous readers are done with their reference to
//! the "old" data can be challenging. However, in cases where readers are
//! known to never hold on to shared data for more than a fixed time period,
//! RCU can be substantially simplified by simply making sure that we do not
//! perform the destructive operation until after that fixed time period has
//! elapsed after making the data unavailable to new readers. This has the
//! advantage of being extremely simple, but the disadvantage that memory
//! will never be reclaimed sooner than after the fixed time period has
//! elapsed, even if no readers have references to the data.
//!
//! The purpose of this allocator adaptor is to do precisely that, i.e. to
//! defer the destruction of objects and the deallocation of the associated
//! memory until after some fixed time period has elapsed. The fixed time
//! period is called the _timeout time_, and it must be provided by the user
//! when constructing the allocator. This allocator proceeds as follows:
//! 1. When destroying data, this allocator does not do anything.
//!    Destruction is deferred until deallocation is performed.
//! 2. When deallocating data, this allocator puts the data in a buffer of a
//!    fixed size (the _delay buffer_). When the _delay buffer_ is full, it
//!    puts it on a list (the _delay list_) along with a time stamp of the
//!    current time. The elements in the _delay list_ are kept in order of
//!    their time stamp, such that older elements are at the beginning of the
//!    list. A larger size for the _delay buffer_ means a coarser granularity
//!    of the _timeout time_, but less frequent allocations to add entries to
//!    the _delay list_.
//! 3. On each deallocation, the allocator tries to destroy and deallocate as
//!    many elements from the _delay list_ as it can. An element can be
//!    destroyed and freed when more than the _timeout time_ has passed since
//!    it was put on the _delay list_ (in step 2 above). This process of
//!    removing elements that are older than the _timeout time_ from the
//!    beginning of the _delay list_ is known as _purging_ (see the `purge()`
//!    function).
//!
//! This functionality is implemented as an allocator adaptor, meaning that
//! `deferred_reclamation_allocator` is meant to wrap another, underlying,
//! allocator. All memory allocation and eventual deallocation is done through
//! the underlying allocator, which allows a great deal of flexibility in
//! creating custom allocators through composition of existing allocators.
//!
//! Notes on copy-constructibility
//! ------------------------------
//! This allocator meets the requirement of copy-constructibility by copying
//! the underlying allocator and the provided timeout-related settings.
//! However, the _delay buffer_ and the _delay list_ are not copied. This,
//! along with the definition of equality comparison based on that of the
//! underlying allocator and the _timeout time_, ensures that we have proper
//! copy semantics. Indeed, the requirements of the Allocator concept say two
//! allocators must compare equal if the memory allocated with one can be
//! deallocated with the other. Memory allocated with one `deferred_memory_allocator`
//! can always be deallocated by any other `deferred_memory_allocator` (assuming
//! the underlying allocators compare equal), so long as a to-be-destroyed
//! element is never put in more than one _delay buffer_ or _delay list_.
//! Since we never copy those, the only way for this to happen would be to
//! deallocate the same object twice, which is already an error.
//!
//! [1]: https://en.wikipedia.org/wiki/Read-copy-update
//!
//! @todo
//! - We're missing the following:
//!   + propagate_on_container_copy_assignment
//!   + propagate_on_container_move_assignment
//!   + propagate_on_container_swap
template <typename Allocator>
class deferred_reclamation_allocator {
  using AllocatorTraits = std::allocator_traits<Allocator>;
  struct force_copy_tag { };
  template <typename T>
  using alloc_rebind_t = typename AllocatorTraits::template rebind_alloc<T>;
  template <typename T>
  using alloc_pointer_t = typename alloc_rebind_t<T>::pointer;

public:
  using pointer = typename AllocatorTraits::pointer;
  using const_pointer = typename AllocatorTraits::const_pointer;
  using void_pointer = typename AllocatorTraits::void_pointer;
  using const_void_pointer = typename AllocatorTraits::const_void_pointer;
  using size_type = typename AllocatorTraits::size_type;
  using difference_type = typename AllocatorTraits::difference_type;
  using value_type = typename AllocatorTraits::value_type;

  template <typename T>
  struct rebind {
    using other = deferred_reclamation_allocator<alloc_rebind_t<T>>;
  };

  deferred_reclamation_allocator() = delete;

  //! Create a deferred allocator with the given underlying allocator, and
  //! other settings.
  //!
  //! @param allocator
  //!        The underlying allocator to use for allocations and deallocations.
  //! @param timeout
  //!        The time period for which deallocated data must be kept around
  //!        before actual destruction and deallocation occurs.
  //! @param delay_buffer_size
  //!        The size of the _delay buffer_, which controls how often we flush
  //!        the buffer to the _delay list_ and try to purge the _delay list_.
  //!        This must be an integer greater than 0.
  template <typename Rep, typename Period>
  deferred_reclamation_allocator(Allocator allocator,
                                 std::chrono::duration<Rep, Period> timeout,
                                 std::size_t delay_buffer_size = 100)
    : allocator_{allocator}
    , timeout_{std::chrono::duration_cast<Duration>(timeout)}
    , now_{TimeoutClock::now()}
    , buffer_allocator_{allocator}
    , buffer_capacity_{delay_buffer_size}
    , current_buffer_size_{0}
    , delay_list_{}
  {
    assert(delay_buffer_size >= 1);
    current_buffer_ = buffer_new();
  }

  //! Create a deferred allocator with a default-constructed instance of the
  //! underlying allocator and the given settings.
  //!
  //! @param timeout
  //!        The time period for which deallocated data must be kept around
  //!        before actual destruction and deallocation occurs.
  //! @param delay_buffer_size
  //!        The size of the _delay buffer_, which controls how often we flush
  //!        the buffer to the _delay list_ and try to purge the _delay list_.
  //!        This must be an integer greater than 0.
  template <typename Rep, typename Period>
  deferred_reclamation_allocator(std::chrono::duration<Rep, Period> timeout,
                                 std::size_t delay_buffer_size = 100)
    : deferred_reclamation_allocator{Allocator{}, timeout, delay_buffer_size}
  { }

  //! Converting copy-constructor from another `deferred_reclamation_allocator`.
  //!
  //! This allows constructing a `deferred_reclamation_allocator` from another
  //! such allocator with a different underlying allocator, so long as the
  //! underlying allocators are compatible (i.e. one can be constructed from
  //! the other). For details on how this behaves, see notes on
  //! copy-constructibility in the class-wide documentation.
  template <typename OtherAllocator, typename = std::enable_if_t<
    std::is_constructible<Allocator, OtherAllocator const&>::value
  >>
  deferred_reclamation_allocator(deferred_reclamation_allocator<OtherAllocator> const& other,
                                 force_copy_tag /* ignore this */ = {})
    : allocator_{other.allocator_}
    , timeout_{other.timeout_}
    , now_{TimeoutClock::now()}
    , buffer_allocator_{other.buffer_allocator_}
    , buffer_capacity_{other.buffer_capacity_}
    , current_buffer_size_{0}
    , delay_list_{}
  {
    current_buffer_ = buffer_new();
  }

  //! Copy constructs an allocator from another `deferred_reclamation_allocator`.
  //!
  //! For details, see notes on copy-constructibility in the class-wide
  //! documentation.
  deferred_reclamation_allocator(deferred_reclamation_allocator const& other)
    : deferred_reclamation_allocator{other, force_copy_tag{}}
  { }

  //! Move constructs a `deferred_reclamation_allocator` from another one.
  //!
  //! The underlying allocator is move-constructed, the various allocator
  //! settings are copied, and both the _delay buffer_ and the _delay list_
  //! are moved to the newly constructed allocator.
  //!
  //! A moved-from `deferred_reclamation_allocator` may only be destroyed;
  //! calling any other method is undefined behavior.
  deferred_reclamation_allocator(deferred_reclamation_allocator&& other)
    : allocator_{std::move(other.allocator_)}
    , timeout_{other.timeout_}
    , now_{TimeoutClock::now()}
    , buffer_allocator_{std::move(other.buffer_allocator_)}
    , buffer_capacity_{other.buffer_capacity_}
    , current_buffer_size_{other.current_buffer_size_}
    , current_buffer_{std::exchange(other.current_buffer_, nullptr)}
    , delay_list_{std::move(other.delay_list_)}
  { }

  deferred_reclamation_allocator& operator=(deferred_reclamation_allocator const&) = delete;
  deferred_reclamation_allocator& operator=(deferred_reclamation_allocator&&) = delete;

  //! Forwards the allocation to the underlying allocator.
  //!
  //! @warning
  //! Since this allocator performs destruction and deallocation in the same
  //! step, one should never deallocate something that has not been constructed.
  //! Doing otherwise would result in this allocator trying to destroy an
  //! object that was never constructed, which is undefined behavior. To
  //! avoid this, make sure a call to `allocate` is always matched by a call
  //! to `construct`. For illustration, the following is wrong:
  //! ```c++
  //! deferred_reclamation_allocator<...> allocator{...};
  //!
  //! // Allocate some memory, but don't construct in it.
  //! auto* ptr = allocator.allocate(1);
  //!
  //! ... do stuff ...
  //!
  //! // Perhaps I realized I wouldn't need the memory I allocated above,
  //! // so I will just free it.
  //! allocator.deallocate(ptr, 1); // <= WRONG! `*ptr` was never constructed
  //! ```
  //!
  //! @param n
  //!        The number of objects to allocate storage for.
  //! @returns
  //!        The result of calling `allocate(n)` on the underlying allocator.
  pointer allocate(std::size_t n) {
    assert(!was_moved_from());
    return allocator_.allocate(n);
  }

  //! Constructs an object using the underlying allocator.
  //!
  //! All the arguments are simply forwarded to the underlying allocator's
  //! `construct()` method.
  template <typename ...Args>
  void construct(pointer p, Args&& ...args) {
    assert(!was_moved_from());
    allocator_.construct(std::move(p), std::forward<Args>(args)...);
  }

  //! Does not do anything, since destruction is delayed until deallocation.
  //!
  //! As explained in the class-wide documentation, destruction is deferred
  //! until some fixed time period has elapsed after a request to deallocate
  //! memory is issued. Destruction and deallocation through the underlying
  //! allocator are both done at that point, not before.
  //!
  //! @warning
  //! Since this allocator does not actually destruct the object when `destroy`
  //! is called, one should never reuse memory obtained through this allocator
  //! after calling `destroy` on it. For example, the following is wrong:
  //! ```c++
  //! deferred_reclamation_allocator<...> allocator{...};
  //!
  //! // Create an object and use it
  //! auto* ptr = allocator.allocate(1);
  //! allocator.construct(ptr, args1...);
  //! use(*ptr);
  //!
  //! // Destroy the object, and construct another one in the same storage.
  //! allocator.destroy(ptr); // <= This line doesn't do anything.
  //! allocator.construct(ptr, args2...); // <= WRONG: Previous object still there!
  //! use(*ptr);
  //! ```
  void destroy(pointer) noexcept {
    assert(!was_moved_from());
  }

  //! Mark the given pointer for delayed destruction and deletion by putting
  //! it on the _delay list_.
  //!
  //! The pointer is first put on the _delay buffer_ for it to eventually be
  //! added to the _delay list_ and then purged. When it is finally purged,
  //! both destruction and deallocation will go through the underlying
  //! allocator's `destroy()` and `deallocate()` functions. This also means
  //! that this method is only `noexcept` when the allocator's `value_type`
  //! is nothrow destructible.
  //!
  //! Whenever the _delay buffer_ is full, it will be appended to the _delay
  //! list_ and a new _delay buffer_ will be created (or reused, see below).
  //! Whenever the _delay buffer_ is flushed to the _delay list_, the allocator
  //! will update its version of the current time and it will try to purge
  //! elements on the _delay list_ whose _timeout time_ has elapsed.
  //!
  //! Note on memory allocation during deallocation
  //! ---------------------------------------------
  //! When memory is deallocated through this allocator and the _delay buffer_
  //! is full, it must be offloaded to the _delay list_. To do so, the allocator
  //! first tries to purge the _delay list_ and reuse a buffer that's not needed
  //! anymore. However, in case no such buffer can be reused (i.e. the _delay
  //! list_ is empty or no elements on the _delay list_ are ready to be purged),
  //! a new buffer will be allocated with the underlying allocator. This can
  //! cause memory usage to go up when `deallocate()` is called. This situation
  //! will typically happen when the allocator starts up and the _delay list_
  //! is empty, or when many deallocations are requested within a time window
  //! smaller than the _timeout time_.
  //!
  //! If an exception is thrown when the allocator tries to allocate a new
  //! buffer in the above procedure, the allocator will wait until it can
  //! purge an entry from the _delay list_. It will then reuse the buffer
  //! that was thus made available, and use this as a new _delay buffer_.
  //! If the _delay list_ is already empty, however, the allocator can not
  //! hope to reuse an existing buffer on the _delay list_. In this case, the
  //! allocator will simply wait for the _timeout time_ to elapse before
  //! purging the current _delay buffer_ and the object(s) being deallocated.
  //!
  //! This allows the allocator to operate correctly even in low-memory
  //! conditions, at the cost of blocking for at most the _timeout time_
  //! when performing a deallocation.
  //!
  //! @param p
  //!        A pointer to deallocate when the required amount of time has
  //!        elapsed.
  //! @param n
  //!        The number of objects passed to the earlier call to `allocate()`.
  //!
  //! @todo
  //! - What is the allocator's exception guarantee when an exception is thrown?
  void deallocate(pointer p, std::size_t n) noexcept(std::is_nothrow_destructible<value_type>{}) {
    assert(!was_moved_from());
    assert(!current_buffer_full() && "the buffer should never be full when entering `deallocate()`, "
                                     "since we purge it as soon as it becomes full");

    current_buffer_push_back({p, n}); // preallocated; does not throw

    // When the buffer is full, we timestamp it and offload it to the delay
    // list. We then purge the delay list and try to start a new buffer,
    // possibly reusing a buffer that was just made available.
    if (current_buffer_full()) {
      // 1. Timestamp and offload the current buffer.
      now_ = current_buffer_->timestamp = TimeoutClock::now();
      delay_list_.push_back(*std::exchange(current_buffer_, nullptr)); // intrusive list; does not throw

      // 2. Try to reuse an existing buffer by purging the delay list.
      current_buffer_ = purge_delay_list_and_reuse_existing_buffer();

      // 3. If we were not able to reuse an existing buffer because no entry
      //    in the delay list was ready, we allocate a new one and handle
      //    error conditions.
      if (current_buffer_ == nullptr) {
        try {
          current_buffer_ = buffer_new(); // strong exception guarantee
        } catch (std::bad_alloc const&) {
          // Wait until we can free at least one entry in the delay list, purge
          // it and reuse the buffer. In the worst case, we'll be waiting to
          // purge and reuse the `current_buffer_` that we just inserted on the
          // delay list.
          assert(!delay_list_.empty() && "we just pushed back the latest buffer to the delay "
                                         "list, so there should be at least one element");
          auto const& oldest = delay_list_.front();
          std::this_thread::sleep_until(oldest.timestamp + timeout_);
          now_ = TimeoutClock::now();
          current_buffer_ = purge_delay_list_and_reuse_existing_buffer();
        }
      }

      assert(current_buffer_ != nullptr);
      current_buffer_size_ = 0; // mark the current buffer as being empty
    }
  }

  //! Purges everything on the _delay list_ and in the current _delay buffer_,
  //! waiting for the _timeout times_ of objects to elapse when required.
  //!
  //! The destructor will reclaim everything that was deallocated through
  //! `deallocate()`, i.e. everything that was put on the _delay list_.
  //! The destructor __does__ respect the guarantee that nothing will be
  //! reclaimed before its _timeout time_ has elapsed, which means that it
  //! will have to wait before it can destroy and reclaim some objects.
  //!
  //! In particular, if the _delay buffer_ is not empty, the allocator will
  //! have to wait for the total _timeout time_ before it can reclaim objects
  //! in it, since they have not been timestamped yet.
  //!
  //! This function is noexcept exactly when the `value_type` is nothrow
  //! destructible, since it may need to destroy objects on the _delay list_.
  //!
  //! @note
  //! There are two main approaches we could use to do this reclamation:
  //! - We could simply wait for the _timeout time_ of the youngest object to
  //!   elapse, and then reclaim everything.
  //! - We could start trying to progressively reclaim the oldest objects first
  //!   and work our way to the youngest one, waiting whenever we need to let
  //!   an object's _timeout time_ elapse.
  //! The former is simpler and requires less overall work. However, the latter
  //! may be faster, as we may be able to reclaim the youngest objects without
  //! having to wait by the time we make it to them. Also, we expect such an
  //! allocator to be destroyed quite rarely, but with potentially large numbers
  //! of objects on the delay list. Hence, it seems like optimizing the
  //! destructor latency is preferable to optimizing the total amount of work
  //! it does. Consequently, the approach we currently use is the latter.
  //! This is an implementation detail that must not be relied upon.
  ~deferred_reclamation_allocator() noexcept(std::is_nothrow_destructible<value_type>{}) {
    // If we've been moved-from, we don't have anything special to do.
    if (was_moved_from()) {
      return;
    }

    // 1. Check the current time and timestamp the current buffer. We then
    //    proceed with the _delay list_; we'll handle the buffer at the end.
    auto now = TimeoutClock::now();
    current_buffer_->timestamp = now;

    // 2. Reclaim all the buffers on the _delay list_, waiting as needed.
    purge(purge_mode::exhaustive);
    assert(delay_list_.empty());

    // 3. If the current buffer is not empty, wait for the remaining time
    //    required and reclaim everything in it. The buffer is not full
    //    (otherwise it would have been on the _delay list_), so it can't
    //    be handled exactly as the ones above.
    if (!current_buffer_empty()) {
      auto const ready_to_delete = current_buffer_->timestamp + timeout_;
      if (now <= ready_to_delete) {
        std::this_thread::sleep_until(ready_to_delete);
      }
      reclaim_buffer_elements(current_buffer_->elements,
                              current_buffer_->elements + current_buffer_size_);
    }
    buffer_delete(current_buffer_);
  }

  //! Returns whether an allocator can be used to deallocate storage allocated
  //! by another allocator.
  //!
  //! Two `deferred_reclamation_allocator`s are equal if and only if their
  //! underlying allocators are equal and their _timeout times_ are equal.
  //!
  //! For details, see the comment on copy-constructibility in the class-wide
  //! documentation.
  friend bool operator==(deferred_reclamation_allocator const& a,
                         deferred_reclamation_allocator const& b)
  { return a.timeout_ == b.timeout_ && a.allocator_ == b.allocator_; }

  //! Equivalent to `!(a == b)`.
  friend bool operator!=(deferred_reclamation_allocator const& a,
                         deferred_reclamation_allocator const& b)
  { return !(a == b); }

  //! Purges the _delay list_, destroying and deallocating elements that have
  //! been in the _delay list_ for more than the _timeout time_.
  //!
  //! This method can be used by an application with special knowledge of its
  //! own usage pattern to shrink the _delay list_. It has two flavors:
  //! opportunistic and exhaustive.
  //! - Opportunistic means that only elements of the _delay list_ that are
  //!   old enough to be reclaimed will be reclaimed, and `purge()` will stop
  //!   as soon as it encounters an element that is too young to be reclaimed.
  //!   This is the default mode of operation.
  //!
  //! - Exhaustive means that all the elements in the delay list will be
  //!   reclaimed. When an element is too young to be reclaimed, the method
  //!   will simply wait for it to become reclaimable.
  //!
  //! To pick the desired flavor, use the following:
  //! ```c++
  //! allocator.purge(amz::purge_mode::opportunistic);
  //! allocator.purge(amz::purge_mode::exhaustive);
  //! allocator.purge(); // same as opportunistic
  //! ```
  //!
  //! Note that in all cases, the current _delay buffer_ is NOT reclaimed.
  //! This is because the _delay buffer_ is not timestamped until it is full,
  //! which means we would have no way to ensure that elements in the _delay
  //! buffer_ can be reclaimed other than waiting the full _timeout time_.
  //! We don't do that.
  //!
  //! This method is `noexcept` exactly when destroying the `value_type` of
  //! this allocator is `noexcept`.
  template <typename Flavor = detail::opportunistic_t>
  void purge(Flavor = {}) noexcept(std::is_nothrow_destructible<value_type>{}) {
    static_assert(std::is_same<Flavor, detail::opportunistic_t>{} ||
                  std::is_same<Flavor, detail::exhaustive_t>{},
      "'deferred_reclamation_allocator::purge' has two flavor: opportunistic and exhaustive. pick one.");
    assert(!was_moved_from());

    auto const reclaim_full_buffer = [this](alloc_pointer_t<DelayBuffer> buffer) {
      reclaim_buffer_elements(buffer->elements,
                              buffer->elements + buffer_capacity_);
      buffer_delete(buffer);
    };

    now_ = TimeoutClock::now();

    while (!delay_list_.empty()) {
      auto& oldest = delay_list_.front();

      // If the oldest buffer can be purged, just do it and keep going.
      // Note that a buffer must be full in order to make it to the delay list.
      // Hence, we know that the buffers we manipulate below are always full,
      // which means their size == their capacity.
      auto const ready_to_delete = oldest.timestamp + timeout_;
      if (now_ > ready_to_delete) {
        delay_list_.pop_front_and_dispose(reclaim_full_buffer);
      }

      // Otherwise, if the oldest buffer is still too young to be purged,
      // there's two options:
      // (1) we were being opportunistic: just stop trying to purge
      else if (std::is_same<Flavor, detail::opportunistic_t>{}) {
        return;
      }

      // (2) we're being exhaustive: wait for enough time to pass and try again
      else if (std::is_same<Flavor, detail::exhaustive_t>{}) {
        std::this_thread::sleep_until(ready_to_delete);
        delay_list_.pop_front_and_dispose(reclaim_full_buffer);
        // We know we slept until at least that time point, so we can use
        // this as our `now` to avoid calling `TimeoutClock::now()`.
        now_ = ready_to_delete;
      }

      else {
        assert(false && "unreachable");
      }
    }
  }

private:
  template <typename>
  friend class deferred_reclamation_allocator;
  Allocator allocator_;

  ////////////////////////////////////////////////////////////////////////////
  // Timeout-related members and definitions
  ////////////////////////////////////////////////////////////////////////////
  using TimeoutClock = std::chrono::steady_clock;
  static_assert(noexcept(TimeoutClock::now()),
    "We make the assumption that our clock does not throw in various places "
    "for providing noexcept guarantees. This must be satisfied.");

  using TimePoint = typename TimeoutClock::time_point;
  using Duration = typename TimeoutClock::duration;

  Duration const timeout_;
  TimePoint now_;

  ////////////////////////////////////////////////////////////////////////////
  // Implementation of the buffered delay list.
  //
  // Note:
  // Most of the following functions operate on the buffer currently owned by
  // the allocator. Despite it being inflexible and non-generic, we do things
  // this way because it avoids duplicating information like the size of
  // a buffer (we only need the size of the current buffer) or the capacity
  // of a buffer (they all have the same capacity).
  ////////////////////////////////////////////////////////////////////////////
  struct DelayBufferElement {
    pointer p;
    std::size_t n;
  };

  struct DelayBuffer
    : boost::intrusive::slist_base_hook<
      boost::intrusive::link_mode<boost::intrusive::normal_link>,
      boost::intrusive::void_pointer<void_pointer>
    >
  {
    DelayBuffer() = default;
    TimePoint timestamp;
    DelayBufferElement elements[];
  };

  using DelayList = boost::intrusive::slist<DelayBuffer, boost::intrusive::cache_last<true>>;

  // Reclaim the `DelayBufferElement`s in the provided range with the
  // underlying allocator. This does not make any check related to the
  // _timeout time_.
  template <typename Iterator>
  void reclaim_buffer_elements(Iterator elem, Iterator last) {
    for (; elem != last; ++elem) {
      detail::destroy_n(elem->p, elem->n); // may throw if ~value_type can throw
      allocator_.deallocate(elem->p, elem->n); // does not throw
    }
  }

  // Returns whether the current buffer is full.
  bool current_buffer_full() const noexcept {
    return current_buffer_size_ == buffer_capacity_;
  }

  // Returns whether the current buffer is empty.
  bool current_buffer_empty() const noexcept {
    return current_buffer_size_ == 0;
  }

  // Appends an element to the current buffer. The behavior is undefined if
  // this function is called when the current buffer is full. This function
  // never throws or allocates.
  void current_buffer_push_back(DelayBufferElement elem) noexcept {
    assert(!current_buffer_full() && "trying to push_back in the current buffer, but it is full");
    current_buffer_->elements[current_buffer_size_] = elem;
    ++current_buffer_size_;
  }

  // We need to allocate buffers as chunks of individual bytes because their
  // size is not fixed at compile-time (the dreaded trailing array member).
  using BufferAllocator = alloc_rebind_t<char>;

  // Allocates a new empty buffer of the maximum buffer size with the underlying
  // allocator. The timestamp of the buffer is just default constructed (i.e.
  // not representing a meaningful value).
  alloc_pointer_t<DelayBuffer> buffer_new() {
    std::size_t const buffer_element_bytes = buffer_capacity_ * sizeof(DelayBufferElement);
    alloc_pointer_t<char> bytes = buffer_allocator_.allocate(sizeof(DelayBuffer) + buffer_element_bytes);
    assert(bytes != nullptr);
    return new (std::addressof(*bytes)) DelayBuffer{};
  }

  // Deallocates and destroys a buffer. It is undefined behavior to use this
  // function on a buffer that is not empty, i.e. that has not been fully
  // purged using `reclaim_buffer_elements`.
  void buffer_delete(alloc_pointer_t<DelayBuffer> buffer) {
    assert(buffer != nullptr);
    buffer->~DelayBuffer();
    alloc_pointer_t<char> bytes = reinterpret_cast<char*>(std::addressof(*buffer));
    buffer_allocator_.deallocate(bytes, buffer_capacity_);
  }

  alloc_pointer_t<DelayBuffer> purge_delay_list_and_reuse_existing_buffer() {
    alloc_pointer_t<DelayBuffer> reuse = nullptr;
    while (!delay_list_.empty()) {
      auto& oldest = delay_list_.front();
      // If the current time is too early, stop trying to purge.
      if (now_ <= oldest.timestamp + timeout_)
        return reuse;

      // Otherwise, reclaim everything in the buffer and unlink it from the delay list.
      reclaim_buffer_elements(oldest.elements, oldest.elements + buffer_capacity_);
      delay_list_.pop_front(); // does not throw or invalidate references

      // If we haven't found a buffer to reuse yet, we keep this one for reuse.
      // Otherwise, we keep the buffer we've already found and deallocate this
      // one. Because of the order in which we visit buffers on the delay list,
      // the first one is going to be the oldest one, i.e. the one that we
      // allocated first. We presume it's better to free buffers that were
      // allocated more recently than the other way around.
      if (reuse == nullptr) {
        reuse = &oldest;
      } else {
        buffer_delete(&oldest);
      }
    }
    return reuse;
  }

  bool was_moved_from() const noexcept {
    return current_buffer_ == nullptr;
  }

  BufferAllocator buffer_allocator_;
  std::size_t const buffer_capacity_;
  std::size_t current_buffer_size_;
  alloc_pointer_t<DelayBuffer> current_buffer_; // `nullptr` iff `*this` has been moved-from
  DelayList delay_list_;
};
} // end namespace amz

#endif // include guard
