#include "bools.h"
#include <format>
#include <iostream>
#include <map>
#include <string_view>
#include <vector>

#define dispatch(_fn)                                                                    \
    _fn(boolean::sub_table, "sub_table");                                                \
    _fn(boolean::xor_table, "xor_table");                                                \
    _fn(boolean::and_table, "and_table");                                                \
    _fn(boolean::or_table, "or_table");                                                  \
    _fn(boolean::not_table<2>, "not_table");                                             \
    do {                                                                                 \
    } while (0)

auto main() -> int {
    [[maybe_unused]]
    static constexpr auto result = boolean::find_possible_table({
        boolean::or_table,  //
        boolean::xor_table, //
    });

    std::vector<std::string_view> possible_names{};
    std::map<std::string_view, std::vector<std::pair<std::string_view, std::string_view>>>
        can_from_pairs{};

    static constexpr auto print_table = [](int n) {
        std::cout << std::format("    Latent: {}\n", n);
        std::cout << std::format("      0 0 -> {}\n", bool(n & 1));
        std::cout << std::format("      0 1 -> {}\n", bool(n & 2));
        std::cout << std::format("      1 0 -> {}\n", bool(n & 4));
        std::cout << std::format("      1 1 -> {}\n", bool(n & 8));
    };

    auto tmp_result = result;

    auto pretty_print =
        [&]<bool _Verbose = true>(boolean::bool_table<2> target, std::string_view name) {
            if (!tmp_result.test(target.to_ulong()))
                return;
            std::cout << std::format("  Table: {} is possible.\n", name);
            possible_names.push_back(name);
            if constexpr (_Verbose)
                print_table(target.to_ulong());
        };

    // dispatch(pretty_print);

    auto test_cope = [&](boolean::bool_table<2> x, boolean::bool_table<2> y) {
        auto possible = boolean::find_possible_table({x, y});
        tmp_result    = possible;
        dispatch(pretty_print.template operator()<false>);
    };

    auto loop_cope = [&](boolean::bool_table<2> x, std::string_view name_x) {
        auto lambda = [&](boolean::bool_table<2> y, std::string_view name_y) {
            if (x.to_ulong() > y.to_ulong())
                return;
            std::cout << std::format("case: {} and {}\n", name_x, name_y);
            possible_names.clear();
            test_cope(x, y);
            for (auto &&name : possible_names)
                if (name != name_x && name != name_y)
                    can_from_pairs[name].emplace_back(name_x, name_y);
            std::cout << '\n';
        };
        dispatch(lambda);
    };

    auto *ptr = std::cout.rdbuf(nullptr);
    dispatch(loop_cope);
    std::cout.rdbuf(ptr);

    for (auto &&[name, pairs] : can_from_pairs) {
        std::cout << std::format("Table: {} can be derived from:\n", name);
        for (auto &&[x, y] : pairs) {
            if (x == y)
                std::cout << std::format("  {}\n", x);
            else
                std::cout << std::format("  {} + {}\n", x, y);
        }
        std::cout << '\n';
    }
}
