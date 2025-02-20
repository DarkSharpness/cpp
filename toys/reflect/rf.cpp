#include "rf.h"
#include "../formatter/fmt.h" // IWYU pragma: keep
#include <array>
#include <iostream>
#include <tuple>
#include <utility>

struct test {
    int x;
    int y;
};

struct emp {};

struct nest {
    test x;
    [[no_unique_address]]
    emp e;
    test y;
    std::tuple<> t;
};

auto main() -> int {
    test t        = {1, 2};
    auto &&[x, y] = reflect::tuplify(t);

    x = 3;
    y = 4;
    t = reflect::untuplify<test>(std::tuple<int, int>{x, 1});

    std::cout << t.x << " " << t.y << std::endl;

    static_assert(reflect::tuplify(emp{}) == std::tuple<>{});
    static_assert(reflect::tuplify(std::pair<int, int>{}) == std::tuple<int, int>{});
    static_assert(reflect::tuplify(std::array<long, 1>{}) == std::tuple<long>{});
    static_assert(reflect::member_size<test>() == 2);
    constexpr auto nested = nest{
        .x = {1, 2},
        .e = {},
        .y = {3, 4},
        .t = {},
    };
    static_assert(reflect::flatten(nested) == std::tuple<int, int, int, int>{1, 2, 3, 4});
    auto copied = nested;
    std::apply(
        [](auto &&...args) {
            ((std::cout << args << " "), ...) << std::endl;
            int i = 0;
            ((i += 1, args = i * i), ...);
        },
        reflect::flatten(copied)
    );
    std::cout << copied.x.x << " " << copied.x.y << " " // as expected
              << copied.y.x << " " << copied.y.y << std::endl;
}
