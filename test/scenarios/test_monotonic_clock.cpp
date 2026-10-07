#include "monotonic_clock.hpp"
#include "test_support.hpp"
int main() {
  MonotonicClock c;
  auto a = c.extend(0xfffffff0U);
  CHECK(a == 0xfffffff0U);
  CHECK(c.extend(0x10U) == a + 32);
  CHECK(c.extend(0x10U) == a + 32);
  CHECK(c.extend(0x20U) == a + 48);
}
