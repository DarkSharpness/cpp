#include "utils/error.h"
#include "utils/function.h"
#include <format>
#include <functional>
#include <iostream>

auto test() -> const std::function<int(int)> {
    return [](int x) { return x + 1; };
}

auto one_time_use(dark::function_view<int(int)> f) -> void {
    std::cout << "one_time_use" << std::endl;
    std::cout << std::format("result: {}", f(1)) << std::endl;
}

auto another_fun(int f) -> double {
    return f + 1.0;
}

auto main() -> int {
    std::function g        = [](int x) { return x + 1; };
    dark::function_view f  = [](int x) { return x + 1; };
    const auto &f2         = test();
    dark::function_view f3 = dark::function_view{f2};
    dark::function_view f4 = dark::function_view{g};
    // some harder cases
    auto h = dark::function_view<long long(const double &)>{f2};
    dark::assume(h.get() == nullptr, "h doesn't come from function pointer");
    dark::assume(f.get() != nullptr, "f does come from function pointer (lambda)");
    std::cout << std::format("result: {}", f.get()(1)) << std::endl;
    auto l = f;
    dark::assume(l.get() == f.get(), "copy constructor, so the same");
    one_time_use([](int x) { return x + 1; });
    struct guard {
        ~guard() {
            std::cout << "guard destructor" << std::endl;
        }
    };
    // allow steal from another function pointer
    f = dark::function_view<int(int)>{another_fun};
    dark::assume(f.get() == nullptr, "f doesn't come from this function pointer");
    dark::assume(f.get<double(int)>() != nullptr, "f comes from this function pointer");
    // for right value function, need an extra unsafe tag to indicate that
    // captured variables are not used
    one_time_use({[z = guard()](int x) { return x + 1; }, {}});
    std::cout << std::format("result: {}", f(1)) << std::endl;
    const auto lambda = [](int x) { return x; };
    dark::function_view<void(int)>{lambda}(1); // result cast to void
}
