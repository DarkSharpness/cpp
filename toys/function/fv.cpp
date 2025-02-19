#include "fv.h"
#include <functional>
#include <iostream>

auto main() -> int {
    auto fv             = function_view{[](int a, int b) { return a + b; }};
    const auto callable = std::function{fv};
    auto fv2            = function_view{callable};
    std::cout << int(fv2(fv(255, 3), 1)) << '\n';
}
