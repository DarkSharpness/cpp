#include "opt.h"
#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <optional>

auto find(const int n) -> std::optional<const int &> {
    static constexpr auto pool = [] {
        std::array<int, 10> arr;
        std::ranges::iota(arr, 0);
        return arr;
    }();
    auto result = std::ranges::find(pool, n);
    // allow to convert a pointer to an optional
    return std::optional<const int &>{result != pool.end() ? result : nullptr};
}

auto main() -> int {
    std::optional<const int &> opt = find(42);
    std::cout << std::format("has_value = {}\n", opt.has_value());
    opt &= nullptr; // nullptr -> optional<const int &>{}
    std::optional<long long> t = opt.transform([](const int &x) { return x; });
    t.emplace(233);
    std::cout << std::format("{}\n", (opt | t | 42));
    int x = 66;
    opt |= x;
    opt &&[&x](const int &y) {
        x = 0;
        std::cout << std::format("opt = {}\n", y);
        return std::make_optional<int>();
    };
    opt.reset();
    if (opt || true)
        opt || [] { std::cout << "opt is empty\n"; };
    return opt || [] { return 0ll; };
}
