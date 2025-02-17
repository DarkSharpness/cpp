#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace boolean {

template <std::size_t _Inputs>
struct bool_table;

template <typename _Tp>
inline constexpr auto is_bool_table_v = false;

template <std::size_t _Inputs>
inline constexpr auto is_bool_table_v<bool_table<_Inputs>> = true;

template <typename _Tp>
concept bool_table_type = is_bool_table_v<_Tp>;

template <std::size_t _Len, typename _Fn>
constexpr auto make_table(_Fn &&fn) -> bool_table<_Len> {
    auto table = bool_table<_Len>{};
    for (std::size_t i = 0; i < table.size(); ++i)
        table[i] = fn(std::bitset<_Len>{i});
    return table;
}

template <std::size_t _Inputs>
struct bool_table : std::bitset<1zu << _Inputs> {
    static_assert(_Inputs <= 64 && _Inputs > 0, "Too many inputs");

    constexpr bool_table() = default;
    constexpr bool_table(std::bitset<1 << _Inputs> table) :
        std::bitset<1 << _Inputs>(table) {}

    constexpr auto operator()(std::bitset<_Inputs> input) const -> bool {
        return (*this)[input.to_ulong()];
    }

    template <std::size_t _Target = _Inputs, bool_table_type... _Source>
    constexpr auto gather(const _Source &...src) -> bool_table {
        return make_table<_Inputs>([&](std::bitset<_Inputs> input) {
            auto new_input = std::bitset<_Inputs>{};
            [&]<std::size_t... _Is>(std::index_sequence<_Is...>) {
                ((new_input[_Is] = src(input)), ...);
            }(std::make_index_sequence<_Target>{});
            return (*this)(new_input);
        });
    }
};

template <std::size_t _Inputs>
struct bool_table_vec : std::vector<bool_table<_Inputs>> {
    using std::vector<bool_table<_Inputs>>::vector;
};

template <std::size_t... _Lens>
bool_table_vec(bool_table<_Lens>...) -> bool_table_vec<std::ranges::max({_Lens...})>;

template <std::size_t _Inputs>
bool_table(std::bitset<_Inputs>) -> bool_table<std::bit_width(_Inputs)>;

inline constexpr auto or_table =
    make_table<2>([](std::bitset<2> b) { return b[0] || b[1]; });

inline constexpr auto and_table =
    make_table<2>([](std::bitset<2> b) { return b[0] && b[1]; });

inline constexpr auto xor_table =
    make_table<2>([](std::bitset<2> b) { return b[0] ^ b[1]; });

inline constexpr auto sub_table =
    make_table<2>([](std::bitset<2> b) { return b[0] && !b[1]; });

template <std::size_t _N = 1, std::size_t _M = 0>
inline constexpr auto not_table =
    make_table<_N>([](std::bitset<_N> b) { return !b[_M]; });

template <std::size_t _N = 1, std::size_t _M = 0>
inline constexpr auto map_table =
    make_table<_N>([](std::bitset<_N> b) { return !!b[_M]; });

inline constexpr auto find_possible_table(bool_table_vec<2> inputs)
    -> std::bitset<1 << (1 << 2)> {
    // a combination to find all possible tables
    auto possible_table = bool_table<(1 << 2)>{};
    auto history_tables = std::bitset<(1 << 2) << (1 << 2)>{};
    auto table_count    = 0zu;

    bool changed = false;

    auto get_nth_table = [&](std::size_t n) {
        auto table = std::bitset<1 << 2>{};
        for (int i = 0; i < 1 << 2; ++i)
            table[i] = history_tables[n << 2 | i];
        return bool_table<2>{table};
    };
    auto add_new_table = [&](std::bitset<1 << 2> table) {
        if (possible_table[table.to_ulong()])
            return;
        changed      = true;
        const auto n = table_count;
        table_count  = n + 1;
        for (int i = 0; i < 1 << 2; ++i)
            history_tables[n << 2 | i] = table[i];
        possible_table[table.to_ulong()] = true;
    };

    // basic identity tables
    add_new_table(map_table<2, 0>);
    add_new_table(map_table<2, 1>);

    // to speed up the iteration
    for (auto &table : inputs)
        add_new_table(table);

    do {
        changed = false;
        for (std::size_t i = 0; i < table_count; ++i) {
            const auto table_x = get_nth_table(i);
            for (std::size_t j = 0; j < table_count; ++j) {
                const auto table_y = get_nth_table(j);
                for (auto &table : inputs)
                    add_new_table(table.gather(table_x, table_y));
            }
        }
    } while (changed);
    return possible_table;
}

} // namespace boolean
