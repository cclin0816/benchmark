# Benchmark

## Usage

### Generate Benchmark Target

using factory function to make benchmark target.

there are several default factory functions:

* `bp::real_time`: real time benchmark
* `bp::proc_time`: process time benchmark, including user time and system time

```cpp=
auto target = bp::real_time(bench_func, pre_func, post_func);
```

### Benchmark

using `bp::bench` function to benchmark.  
rounds is the number of times to run the benchmark target.  
d_meth is the method to use to calculate result. see [Result](#Result)  

for multiple targets, the execution order will be shuffled randomly.

```cpp=
bp::bench(rounds, d_meth, target1, target2, ...);
```

### Result

d_meth is used to calculate result.

there are serveral default methods:

* `bp::avg`: average of all rounds
* `bp::min`: minimum of all rounds
* `bp::max`: maximum of all rounds
* `min_max`: minimum and maximum of all rounds
* `bp::median`: median of all rounds
* `bp::stddev`: standard deviation of all rounds
* `bp::sum`: sum of all rounds
* `bp::avg_stddev`: average and standard deviation of all rounds
* `bp::excl_avg`: average of all rounds excluding outliers
* `bp::full`: all results

`bp::bench` returns array of result if there is multiple target.  
result type is return type of d_meth.

## Extending Benchmark Library

### New Factory Function

using `std::bind` to make factory function.  
`pre_func` and `post_func` can be omitted.  

```cpp=
// T_val is type return by clock_func
auto factory(std::function<T(void)> func) {
  return std::bind(time<T_val>, clock_func, clock_diff_func, func, pre_func,
                   post_func);
}
```

### New Result Method

the calculate function should take `std::span<std::chrono::nanoseconds>`

for example, you want top 3 fastest results

```cpp=
using nanos = std::chrono::nanoseconds;
std::array<nanos> top3(const std::span<nanos> span) {
  if(span.size() < 3) {
    throw std::runtime_error("not enough results");
  }
  std::nth_element(span.begin(), span.begin() + 3, span.end());
  std::array<nanos, 3> top3{span[0], span[1], span[2]};
  return top3;
}
```
