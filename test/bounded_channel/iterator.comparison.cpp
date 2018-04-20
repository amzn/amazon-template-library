#include <amz/bounded_channel.hpp>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <iterator>


// This test makes sure that the bounded_channel's iterators do not require
// their value_type to be EqualityComparable.

struct NonComparable { };

bool operator==(NonComparable const&, NonComparable const&) = delete;
bool operator!=(NonComparable const&, NonComparable const&) = delete;

TEST_CASE("Iterators don't require an EqualityComparable value_type") {
  amz::bounded_channel<NonComparable> channel{64};
  channel.push(NonComparable{});
  channel.close();

  for (auto it = std::begin(channel); it != std::end(channel); ++it) {
    // nothing
  }
}
