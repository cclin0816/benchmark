#include <time.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace bp {

namespace cn = std::chrono;
using nanos = cn::nanoseconds;
using micros = cn::microseconds;
using millis = cn::milliseconds;
using secs = cn::seconds;
using tp = cn::time_point<cn::steady_clock, nanos>;

// invokable_r concept
template <class R, class Fn, class... Args>
concept invocable_r = std::is_invocable_r_v<R, Fn, Args...>;

/**
 * @brief empty function for function hook
 *
 */
[[maybe_unused]] void empty_fn() {}

/**
 * @brief similiar to std::chrono::steady_clock::now() but with processing time
 * (user + system)
 *
 */
tp proc_time_now() {
  timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return tp(nanos(ts.tv_nsec) + secs(ts.tv_sec));
}

/**
 * @brief difference between two time points
 *
 * @return difference in nanoseconds
 */
nanos tp_diff(const tp& start, const tp& end) {
  return cn::duration_cast<nanos>(end - start);
}

/**
 * @brief timing function
 *
 * @tparam T_VAL type of time points return by the now() function
 * @param now function returning current time
 * @param diff function returning difference between two time points as
 * nanoseconds
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return time spent in func as nanoseconds
 */
template <typename T_VAL>
[[nodiscard]] nanos time(std::function<T_VAL()> now,
                         std::function<nanos(T_VAL, T_VAL)> diff,
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
 * @brief factory for time functions, using std::chrono::steady_clock::now
 *
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return timing function
 */
[[nodiscard]] auto real_time(std::function<void()> func,
                             std::function<void()> pre_func = empty_fn,
                             std::function<void()> post_func = empty_fn) {
  return std::bind(time<tp>, cn::steady_clock::now, tp_diff, func, pre_func,
                   post_func);
}

/**
 * @brief factory for time functions, using proc_time_now
 *
 * @param func function to be timed
 * @param pre_func function to be called before func
 * @param post_func function to be called after func
 * @return timing function
 */
[[nodiscard]] auto proc_time(std::function<void()> func,
                             std::function<void()> pre_func = empty_fn,
                             std::function<void()> post_func = empty_fn) {
  return std::bind(time<tp>, proc_time_now, tp_diff, func, pre_func, post_func);
}

/**
 * @brief benchmark funtions
 *
 * @param rounds number of rounds to be performed
 * @param d_meth calculate function for result for round results
 * @param funcs functions to be benchmarked
 * @return results calculated by d_meth, as array if funcs.size() > 1
 */
template <typename D_METH, typename... Funcs,
          typename R = std::invoke_result_t<D_METH, const std::span<nanos>>>
auto bench(uint64_t rounds, D_METH&& d_meth, Funcs&&... funcs) {
  constexpr auto n_func = sizeof...(Funcs);
  static_assert(n_func > 0, "bench: at least one function required");
  static_assert(!std::is_void_v<R>, "bench: return type must not be void");

  // function parameter pack to array of std::function
  std::array<std::function<nanos()>, n_func> func_arr{
      std::forward<Funcs>(funcs)...};
  std::array<R, n_func> results;
  std::vector<std::vector<nanos>> duration(n_func, std::vector<nanos>(rounds));
  {
    auto total_runs = rounds * n_func;
    auto rng = std::mt19937(std::random_device()());
    auto seq = std::vector<uint64_t>(total_runs);
    std::iota(seq.begin(), seq.end(), 0);
    std::shuffle(seq.begin(), seq.end(), rng);
    for (const auto i : seq) {
      auto func_id = i % n_func;
      auto func_rd = i / n_func;
      duration[func_id][func_rd] = func_arr[func_id]();
    }
  }
  for (auto i = 0UL; i < n_func; ++i) {
    results[i] = std::invoke(d_meth, duration[i]);
  }
  if constexpr (n_func == 1) {
    return results[0];
  } else {
    return results;
  }
}

// max helper
nanos max(const std::span<nanos> span) {
  return *std::max_element(span.begin(), span.end());
}

// min helper
nanos min(const std::span<nanos> span) {
  return *std::min_element(span.begin(), span.end());
}

// min_max helper
std::pair<nanos, nanos> min_max(const std::span<nanos> span) {
  const auto [min_it, max_it] = std::minmax_element(span.begin(), span.end());
  return std::make_pair(*min_it, *max_it);
}

// sum helper
nanos sum(const std::span<nanos> span) {
  return std::accumulate(span.begin(), span.end(), nanos(0));
}

// avg helper
nanos avg(const std::span<nanos> span) { return sum(span) / span.size(); }

// median helper
nanos median(const std::span<nanos> span) {
  auto n = span.size();
  auto mid = n / 2;
  std::nth_element(span.begin(), span.begin() + mid, span.end());
  return span[mid];
}

// stddev helper
double stddev(const std::span<nanos> span) {
  auto _avg = avg(span);
  auto _sum = std::accumulate(
      span.begin(), span.end(), 0UL, [_avg](const auto& acc, const auto& val) {
        return acc +
               (val.count() - _avg.count()) * (val.count() - _avg.count());
      });
  return sqrtl((double)_sum / span.size());
}

// avg_stddev helper
std::pair<nanos, double> avg_stddev(const std::span<nanos> span) {
  auto _avg = avg(span);
  auto _dev = stddev(span);
  return {_avg, _dev};
}

// exclude_avg helper
template <uint64_t N>
nanos excl_avg(const std::span<nanos> span) {
  if (N * 2 >= span.size()) {
    return median(span);
  }
  std::span<nanos> span_excl(span.begin() + N, span.end() - N);
  return avg(span_excl);
}

// full helper
std::vector<nanos> full(const std::span<nanos> span) {
  return std::vector<nanos>(span.begin(), span.end());
}

}  // namespace bp