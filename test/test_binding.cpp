#include <cassert>
#include <iomanip>
#include <iostream>

#include "../benchmark.h"

int norm_fn() { return 3; }

int wrapper_fn(int (&fn)()) { return fn(); }

template <typename T>
T template_fn(T st, T en) {
  return en - st;
}

bm::nanos zero_time() { return bm::nanos(0); }

int main() {
  auto lamda_fn = []() { return 7.7; };
  auto bind_fn = std::bind(wrapper_fn, std::ref(norm_fn));

  // real_time binding
  auto real_time_bind_fn = bm::real_time(bind_fn, lamda_fn, norm_fn);
  // proc_time binding
  auto proc_time_bind_fn = bm::proc_time(bind_fn);

  // self build timer binding
  int i = 1;
  auto self_build_bind_fn = std::bind(
      bm::time<uint32_t, uint32_t>,
      [&]() {
        i = i * 2;
        return i;
      },
      template_fn<uint32_t>, std::cref(lamda_fn), std::ref(bind_fn), norm_fn);

  // bench binding one function
  auto res = bm::bench(10, bm::sum<uint32_t>, self_build_bind_fn);
  assert(res == 699050);
  // bench binding multi functions
  auto res2 = bm::bench(10, bm::median<bm::nanos>, real_time_bind_fn,
                        proc_time_bind_fn, zero_time);
  for (auto& r : res2) {
    std::cout << r.count() << std::endl;
  }
  // bench binding vector of functions
  auto res3 = bm::bench(10, bm::avg_nsd<bm::nanos>,
                        std::vector<std::function<bm::nanos()>>{
                            real_time_bind_fn, proc_time_bind_fn});
  for (auto& r : res3) {
    std::cout << r.first.count() << r.second << std::endl;
  }

  i = 0;
  auto mono = [&]() { return ++i; };

  // max helper binding
  assert(bm::bench(10, bm::max<int>, mono) == 10);
  // min helper binding
  i = 0;
  assert(bm::bench(10, bm::min<int>, mono) == 1);
  // min_max helper binding
  i = 0;
  assert(bm::bench(10, bm::min_max<int>, mono) == std::make_pair(1, 10));
  // sum helper binding
  i = 0;
  assert(bm::bench(10, bm::sum<int>, mono) == 55);
  // avg helper binding
  i = 0;
  assert(bm::bench(10, bm::avg<int>, mono) == 5);
  // median helper binding
  i = 0;
  assert(bm::bench(10, bm::median<int>, mono) == 6);
  // nsd helper binding
  i = 0;
  assert(bm::bench(10, bm::nsd<int>, mono) == 0.58309518948453009646);
  // avg_nsd helper binding
  i = 0;
  assert(bm::bench(10, bm::avg_nsd<int>, mono) ==
         std::make_pair(5, 0.58309518948453009646));
  // excl_avg helper binding
  i = 0;
  assert(bm::bench(10, bm::excl_avg<int, 2>, mono) == 5);
  // full helper binding
  i = 0;
  auto res4 = bm::bench(10, bm::full<int>, mono);
  for (int j = 0; j < 10; ++j) {
    assert(res4[j] == j + 1);
  }
}