#include "evt/detail/hashing.h"
#include <catch2/catch_test_macros.hpp>
#include <string>


TEST_CASE("Hashing") {
  CHECK(evt::detail::hashValues(uint8_t(5), std::string("foo")) == evt::detail::hash(std::make_tuple<uint8_t, std::string>(5, "foo")));
}
