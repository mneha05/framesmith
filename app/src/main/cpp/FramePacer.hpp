#pragma once
#include <array>
#include <cstddef>
class FramePacer {
public:
  void push(double ms);
  double average() const;
  double p95() const;
  bool janky(double budget_ms=16.667) const;
private:
  std::array<double,120> samples_{}; std::size_t count_{0}; std::size_t cursor_{0};
};
