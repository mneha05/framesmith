#include "../app/src/main/cpp/FramePacer.hpp"

#include <cassert>
#include <iostream>

int main() {
  FramePacer pacer;

  for (int i = 0; i < 100; ++i) {
    pacer.push(8.0 + (i % 5));
  }
  assert(pacer.average() > 8.0 && pacer.average() < 13.0);
  assert(!pacer.janky());

  for (int i = 0; i < 100; ++i) {
    pacer.push(25.0);
  }
  assert(pacer.janky());

  std::cout << "PASS avg=" << pacer.average()
            << " p95=" << pacer.p95() << "\n";
  return 0;
}
