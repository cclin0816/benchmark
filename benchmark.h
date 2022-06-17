/**
 * @file benchmark.h
 * @author cclin0816 (github.com/cclin0816)
 * @brief pleas refer to github.com/cclin0816/benchmark for more information
 * @version 1.0
 * @date 2022-06-17
 *
 * @copyright MIT License, Copyright (c) 2022 cclin0816
 */

#if defined(__linux__) || defined(__unix__)
#include <time.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace bm {

inline namespace v1_0 {

// is_same type trait, using this for compatibility reasons
template <typename Tp, typename... Rest>
struct is_same : std::false_type {};

template <typename Tp>
struct is_same<Tp> : std::true_type {};

template <typename First, typename Second>
struct is_same<First, Second> : std::is_same<First, Second> {};

template <typename First, typename Second, typename... Rest>
struct is_same<First, Second, Rest...>
    : std::integral_constant<bool, std::is_same<First, Second>::value &&
                                       is_same<First, Rest...>::value> {};

// for c++ 11 and 14 use std::result_of
// for c++ 17 and 20 use std::invoke_result
#if __cplusplus >= 201703L
template <typename Fn, typename... Args>
using invoke_r_t = std::invoke_result_t<Fn, Args...>;
#else
template <typename Fn, typename... Args>
using invoke_r_t = typename std::result_of<Fn(Args...)>::type;
#endif

// attribute for c++ 17 and 20
#if __cplusplus >= 201703L
#define NODISCARD [[nodiscard]]
#define MAYBE_UNUSED [[maybe_unused]]
#else
#define NODISCARD
#define MAYBE_UNUSED
#endif

// alias for std::chrono
namespace cn = std::chrono;
using nanos = cn::nanoseconds;
using micros = cn::microseconds;
using millis = cn::milliseconds;
using secs = cn::seconds;
using tp = cn::time_point<cn::steady_clock, nanos>;

/**
 * @brief empty function for function hook
 *
 */
MAYBE_UNUSED void empty_fn() {}

/**
 * @brief similiar to std::chrono::steady_clock::now but with
 * processing time (user + system)
 *
 */
NODISCARD tp proc_time_now() {
#if defined(__linux__) || defined(__unix__)

  timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return tp(nanos(ts.tv_nsec) + secs(ts.tv_sec));

#elif defined(_WIN32)

  FILETIME creation, exit, kernel, user;
  GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
  uint64_t user_time =
      ((((uint64_t)user.dwHighDateTime) << 32) | (uint64_t)user.dwLowDateTime) *
      100UL;
  uint64_t kernel_time = ((((uint64_t)kernel.dwHighDateTime) << 32) |
                          (uint64_t)kernel.dwLowDateTime) *
                         100UL;
  return tp(nanos(user_time + kernel_time));

#endif
}

/**
 * @brief difference between two time points
 *
 * @return difference in nanoseconds
 */
NODISCARD nanos tp_diff(const tp& start, const tp& end) {
  return cn::duration_cast<nanos>(end - start);
}

/**
 * @brief timing function
 *
 * @tparam T_VAL return type of now()
 * @tparam T_DIFF return type of diff()
 * @param now function returning current time
 * @param diff function returning difference between two time points
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return time spent in func as T_DIFF
 */
template <typename T_VAL, typename T_DIFF>
NODISCARD T_DIFF time(std::function<T_VAL()> now,
                      std::function<T_DIFF(T_VAL, T_VAL)> diff,
                      std::function<void()> func,
                      std::function<void()> pre_func,
                      std::function<void()> post_func) {
  pre_func();
  auto start = now();
  func();
  auto end = now();
  post_func();
  return diff(start, end);
}

/**
 * @brief factory function for time, using std::chrono::steady_clock::now
 *
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return timing function
 */
NODISCARD auto real_time(std::function<void()> func,
                         std::function<void()> pre_func = empty_fn,
                         std::function<void()> post_func = empty_fn)
    -> decltype(std::bind(time<tp, nanos>, cn::steady_clock::now, tp_diff, func,
                          pre_func, post_func)) {
  return std::bind(time<tp, nanos>, cn::steady_clock::now, tp_diff, func,
                   pre_func, post_func);
}

/**
 * @brief factory function for time, using proc_time_now
 *
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return timing function
 */
NODISCARD auto proc_time(std::function<void()> func,
                         std::function<void()> pre_func = empty_fn,
                         std::function<void()> post_func = empty_fn)
    -> decltype(std::bind(time<tp, nanos>, proc_time_now, tp_diff, func,
                          pre_func, post_func)) {
  return std::bind(time<tp, nanos>, proc_time_now, tp_diff, func, pre_func,
                   post_func);
}

std::vector<uint64_t> gen_rd_seq(const uint64_t n) {
  std::vector<uint64_t> seq(n);
  std::iota(seq.begin(), seq.end(), 0);
  std::shuffle(seq.begin(), seq.end(), std::mt19937{std::random_device{}()});
  return seq;
}

/**
 * @brief benchmark funtions
 *
 * @param rounds number of rounds to be performed
 * @param d_meth calculate function for result for round results
 * @param func function to be benchmarked
 * @param rest ... additional functions to be benchmarked
 * @return results as array
 */
template <typename D_Meth, typename Func, typename... Rest,
          uint64_t Func_Sz = sizeof...(Rest) + 1,
          typename Func_R = invoke_r_t<Func>,
          typename D_Meth_R = invoke_r_t<D_Meth, std::vector<Func_R>&>>
auto bench(uint64_t rounds, D_Meth&& d_meth, Func&& func, Rest&&... rest)
    -> std::array<D_Meth_R, Func_Sz> {
  static_assert(is_same<Func_R, invoke_r_t<Rest>...>::value,
                "functions must return the same type");
  static_assert(!std::is_void<Func_R>::value, "functions must return a value");
  std::array<std::function<Func_R()>, Func_Sz> funcs{
      std::forward<Func>(func), std::forward<Rest>(rest)...};
  std::array<D_Meth_R, Func_Sz> results;
  std::vector<std::vector<Func_R>> run_results(Func_Sz,
                                               std::vector<Func_R>(rounds));

  const auto total_runs = rounds * Func_Sz;
  const auto seq = gen_rd_seq(total_runs);
  for (const auto i : seq) {
    auto func_idx = i % Func_Sz;
    auto run_idx = i / Func_Sz;
    run_results[func_idx][run_idx] = funcs[func_idx]();
  }

  for (auto i = 0UL; i < Func_Sz; ++i) {
    results[i] = d_meth(run_results[i]);
  }
  return results;
}

/**
 * @brief benchmark functions
 *
 * @param rounds number of rounds to be performed
 * @param d_meth calculate function for result for round results
 * @param funcs functions to be benchmarked
 * @return results as vector
 */
template <typename D_Meth, typename Func_R,
          typename D_Meth_R = invoke_r_t<D_Meth, std::vector<Func_R>&>>
auto bench(uint64_t rounds, D_Meth&& d_meth,
           std::vector<std::function<Func_R()>> funcs)
    -> std::vector<D_Meth_R> {
  const auto func_sz = funcs.size();
  std::vector<D_Meth_R> results(func_sz);
  std::vector<std::vector<Func_R>> run_results(func_sz,
                                               std::vector<Func_R>(rounds));
  const auto total_runs = rounds * func_sz;
  const auto seq = gen_rd_seq(total_runs);
  for (auto i : seq) {
    auto func_idx = i % func_sz;
    auto run_idx = i / func_sz;
    run_results[func_idx][run_idx] = funcs[func_idx]();
  }
  for (auto i = 0UL; i < func_sz; ++i) {
    results[i] = d_meth(run_results[i]);
  }
  return results;
}

/**
 * @brief benchmark functions
 *
 * @param rounds number of rounds to be performed
 * @param d_meth calculate function for result for round results
 * @param func function to be benchmarked
 * @return result
 */
template <typename D_Meth, typename Func, typename Func_R = invoke_r_t<Func>,
          typename D_Meth_R = invoke_r_t<D_Meth, std::vector<Func_R>&>>
auto bench(uint64_t rounds, D_Meth&& d_meth, Func&& func) -> D_Meth_R {
  static_assert(!std::is_void<Func_R>::value, "functions must return value");
  std::vector<Func_R> run_results(rounds);
  for (auto i = 0UL; i < rounds; ++i) {
    run_results[i] = func();
  }
  return d_meth(run_results);
}

// max helper
template <typename T>
T max(const std::vector<T>& vec) {
  return *std::max_element(vec.begin(), vec.end());
}

// min helper
template <typename T>
T min(const std::vector<T>& vec) {
  return *std::min_element(vec.begin(), vec.end());
}

// min_max helper
template <typename T>
std::pair<T, T> min_max(const std::vector<T>& vec) {
  const auto min_max = std::minmax_element(vec.begin(), vec.end());
  return std::make_pair(*min_max.first, *min_max.second);
}

// sum helper
template <typename T>
T sum(const std::vector<T>& vec) {
  return std::accumulate(vec.begin(), vec.end(), T{});
}

// avg helper
template <typename T>
T avg(const std::vector<T>& vec) {
  return sum(vec) / vec.size();
}

// median helper
template <typename T>
T median(std::vector<T>& vec) {
  const auto n = vec.size();
  const auto mid = n / 2;
  std::nth_element(vec.begin(), vec.begin() + mid, vec.end());
  return vec[mid];
}

// normalized standard deviation helper
template <typename T>
double nsd(const std::vector<T>& vec) {
  auto _avg = avg(vec);
  auto _sum = std::accumulate(vec.begin(), vec.end(), T{},
                              [_avg](const T& acc, const T& val) {
                                const auto diff = val - _avg;
                                return acc + diff * diff;
                              });
  return sqrtl((double)_sum / vec.size()) / _avg;
}

// normalized standard deviation helper
template <>
double nsd(const std::vector<nanos>& vec) {
  auto _avg = (int64_t)avg(vec).count();
  auto _sum = std::accumulate(vec.begin(), vec.end(), 0.0L,
                              [_avg](const double& acc, const nanos& val) {
                                const auto diff = (int64_t)val.count() - _avg;
                                return acc + (double)(diff * diff);
                              });
  return sqrtl(_sum / vec.size()) / _avg;
}

// avg_nsd helper
template <typename T>
std::pair<T, double> avg_nsd(const std::vector<T>& vec) {
  auto _avg = avg(vec);
  auto _dev = nsd(vec);
  return std::make_pair(_avg, _dev);
}

// exclude_avg helper
template <typename T, uint64_t N>
T excl_avg(std::vector<T>& vec) {
  if (N * 2 >= vec.size()) {
    return median<T>(vec);
  }
  std::vector<T> excl_vec(vec.begin() + N, vec.end() - N);
  return avg(excl_vec);
}

// full helper
template <typename T>
std::vector<T> full(const std::vector<T>& vec) {
  return vec;
}

}  // namespace v1_0

}  // namespace bm