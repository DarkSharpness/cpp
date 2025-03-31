#include "set/int_set.h"
#include "set/str_set.h"
#include "utils/benchmark.h"
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>

template <typename _Set>
auto benchmark() {
    constexpr int N = 5000000;
    const auto dur  = dark::benchmark::timing([&]() {
        _Set set;
        ::srand(42);
        for (int i = 0; i < N; ++i)
            set.insert(::rand());
        for (int i = 0; i < N; ++i)
            set.erase(::rand());
    });
    std::cout << std::format("Time taken to insert {} elements: {}\n", N, dur);
}

template <typename _Set>
auto benchmark_string() {
    constexpr int N = 1000000;
    const auto dur  = dark::benchmark::timing([&]() {
        _Set set;
        constexpr int M = 1000000 * 100;
        ::srand(42);
        for (int i = 0; i < N; ++i)
            set.insert(std::to_string(::rand() % M));
        for (int i = 0; i < N; ++i)
            set.erase(std::to_string(::rand() % M));
    });
    std::cout << std::format("Time taken to insert {} elements: {}\n", N, dur);
}

auto correctness() -> void {
    dark::int_set set;
    set.insert(1);
    set.insert(2);
    set.insert(4);
    set.insert(8);
    set.insert(4096);
    std::cout << std::format("Set contains 1: {}\n", set.find(1).has_value());
    set.erase(1);
    std::cout << std::format("Set contains 1: {}\n", set.find(1).has_value());
}

auto correctness_string() -> void {
    dark::radix_tree set;
    set.insert("-hello");
    set.insert("-world");
    std::cout << std::format("Set contains 'hello': {}\n", set.find("-hello").has_value());
    std::cout << std::format("Set contains 'world': {}\n", set.find("-world").has_value());
    std::cout << std::format("Set contains 'dark': {}\n", set.find("-dark").has_value());
    set.insert("-dark");
    set.insert("-w");
    std::cout << std::format("Set contains 'hello': {}\n", set.find("-hello").has_value());
    std::cout << std::format("Set contains 'world': {}\n", set.find("-world").has_value());
    std::cout << std::format("Set contains 'dark': {}\n", set.find("-dark").has_value());
    set.erase("-hello");
    std::cout << std::format("Set contains 'hello': {}\n", set.find("-hello").has_value());
    std::cout << std::format("Set contains 'world': {}\n", set.find("-world").has_value());
    std::cout << std::format("Set contains 'w': {}\n", set.find("-w").has_value());
    set.erase("-w");
    std::cout << std::format("Set contains 'w': {}\n", set.find("-w").has_value());
    std::cout << std::format("Set contains 'world': {}\n", set.find("-world").has_value());
}

auto main() -> int {
    correctness();
    correctness_string();

    benchmark<std::set<int>>();
    benchmark<std::unordered_set<std::size_t>>();
    benchmark<dark::int_set>();

    benchmark_string<std::set<std::string>>();
    benchmark_string<std::unordered_set<std::string>>();
    benchmark_string<dark::radix_tree>();
}
