#include "rf.h"
#include <iostream>
#include <tuple>

struct test {
    int x;
    int y;
};

struct emp {};

auto main() -> int {
    test t        = {1, 2};
    auto &&[x, y] = reflect::tuplify(t);

    x = 3;
    y = 4;
    t = reflect::untuplify<test>(std::tuple<int, int>{x, 1});

    std::cout << t.x << " " << t.y << std::endl;

    static_assert(reflect::tuplify(emp{}) == std::tuple<>{});
    static_assert(reflect::member_size<test>() == 2);
}
