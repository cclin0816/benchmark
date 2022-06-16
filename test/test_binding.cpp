#include <iostream>

#include "../benchmark.h"

int now() { return 0; }

int diff(int a, int b) { return b - a; }

unsigned tt() { return 0; }

int main() {
  // bm::tt(now);
  auto res =
      bm::bench(3, bm::max<bm::nanos>, bm::real_time(tt), bm::real_time(now));
  // bm::tt(now, tt);
  for (auto& r : res) {
    std::cout << r.count() << std::endl;
  }
  auto res2 = bm::bench(2, bm::max<bm::nanos>, bm::proc_time(tt));
  std::cout << res2.count() << std::endl;

  std::vector<std::function<bm::nanos()>> funcs;
  funcs.push_back(bm::real_time(now));
  funcs.push_back(bm::real_time(now));

  auto res3 = bm::bench(7, bm::sum<bm::nanos>, funcs);
  for (auto& r : res3) {
    std::cout << r.count() << std::endl;
  }

  auto re4 = bm::bench(7, bm::nsd<bm::nanos>, funcs);
  for (auto& r : re4) {
    std::cout << r << std::endl;
  }

  return 0;
}