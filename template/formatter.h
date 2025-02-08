#pragma once

#include <array>
#include <format>
#include <iostream>
#include <tuple>
#include <utility>

namespace {

template <std::size_t _Len>
consteval auto make_tuple_format() {
    if constexpr (_Len == 0) {
        return std::array<char, 3>{'<', '>', '\0'};
    } else {
        // <{}>, each extra arg contribute to , {}
        auto result = std::array<char, 4 * _Len + 1>{};
        result[0]   = '<';
        result[1]   = '{';
        result[2]   = '}';
        for (std::size_t i = 1; i < _Len; ++i) {
            result[4 * i - 1] = ',';
            result[4 * i - 0] = ' ';
            result[4 * i + 1] = '{';
            result[4 * i + 2] = '}';
        }
        result[4 * _Len - 1] = '>';
        return result;
    }
}

} // namespace

template <typename... _Args>
struct std::formatter<std::tuple<_Args...>> {
    static constexpr auto _S_fmt = make_tuple_format<sizeof...(_Args)>();

    constexpr auto parse(format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::tuple<_Args...> &value, _FormatContext &ctx) const {
        return [&value, &ctx]<std::size_t... _I>(std::index_sequence<_I...>) {
            return std::format_to(ctx.out(), _S_fmt.data(), std::get<_I>(value)...);
        }(std::make_index_sequence<sizeof...(_Args)>{});
    }
};

template <typename _T1, typename _T2>
struct std::formatter<std::pair<_T1, _T2>> {
    constexpr auto parse(format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::pair<_T1, _T2> &value, _FormatContext &ctx) const {
        return std::format_to(ctx.out(), "<{}, {}>", value.first, value.second);
    }
};
