#pragma once
#include <chrono>
#include <ratio>

namespace dark::benchmark {

template <typename R = std::milli, typename F, typename... Args>
[[nodiscard("Benchmark timing result should be used")]] auto timing(F &&f, Args &&...args)
    -> decltype(auto) {
    auto tic = std::chrono::steady_clock::now();
    std::forward<F>(f)(std::forward<Args>(args)...);
    auto toc = std::chrono::steady_clock::now();
    return duration_cast<std::chrono::duration<double, R>>(toc - tic);
}

} // namespace dark::benchmark
