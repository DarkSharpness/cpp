#pragma once

#include "../reflect/rf.h"
#include <array>
#include <bitset>
#include <format>
#include <iostream>
#include <iterator>
#include <map>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace __detail {

template <std::size_t _Len, char _L = '(', char _R = ')'>
consteval auto make_tuple_format() {
    if constexpr (_Len == 0) {
        return std::array<char, 2>{_L, _R};
    } else {
        // <{}>, each extra arg contribute to , {}
        auto result = std::array<char, 4 * _Len>{};
        result[0]   = _L;
        result[1]   = '{';
        result[2]   = '}';
        for (std::size_t i = 1; i < _Len; ++i) {
            result[4 * i - 1] = ',';
            result[4 * i - 0] = ' ';
            result[4 * i + 1] = '{';
            result[4 * i + 2] = '}';
        }
        result[4 * _Len - 1] = _R;
        return result;
    }
}

template <std::size_t _Len, char _L = '(', char _R = ')'>
consteval auto get_format_str() -> std::string_view {
    static constexpr auto _S_fmt = make_tuple_format<_Len>();
    return std::string_view(_S_fmt.data(), _S_fmt.size());
}

} // namespace __detail

template <typename... _Args>
struct std::formatter<std::tuple<_Args...>> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::tuple<_Args...> &value, _FormatContext &ctx) const {
        return [&value, &ctx]<std::size_t... _I>(std::index_sequence<_I...>) {
            constexpr auto fmt = ::__detail::get_format_str<sizeof...(_Args)>();
            return std::format_to(ctx.out(), fmt, std::get<_I>(value)...);
        }(std::make_index_sequence<sizeof...(_Args)>{});
    }
};

template <typename _T1, typename _T2>
struct std::formatter<std::pair<_T1, _T2>> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::pair<_T1, _T2> &value, _FormatContext &ctx) const {
        return std::format_to(ctx.out(), "({}, {})", value.first, value.second);
    }
};

template <typename _T>
struct std::formatter<std::optional<_T>> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::optional<_T> &value, _FormatContext &ctx) const {
        if (value.has_value()) {
            return std::format_to(ctx.out(), "?<{}>", value.value());
        } else {
            return std::format_to(ctx.out(), "?None");
        }
    }
};

template <typename _Tp>
inline constexpr bool is_char_array_v = false;
template <>
inline constexpr bool is_char_array_v<char[]> = true;
template <std::size_t _Num>
inline constexpr bool is_char_array_v<char[_Num]> = true;

template <std::ranges::range _Range>
    requires(!is_char_array_v<_Range>)
struct std::formatter<_Range> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const _Range &range, _FormatContext &ctx) const {
        auto iter  = ctx.out();
        auto begin = std::ranges::begin(range);
        auto end   = std::ranges::end(range);
        // format part.
        iter = std::format_to(iter, "[");
        if (begin != end) [[likely]] {
            iter = std::format_to(iter, "{}", *begin);
            while (++begin != end)
                iter = std::format_to(iter, ", {}", *begin);
        }
        iter = std::format_to(iter, "]");
        return iter;
    }
};

template <reflect::aggregate_type _Tp>
    requires(!std::ranges::range<_Tp>)
struct std::formatter<_Tp> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const _Tp &value, _FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}", reflect::tuplify(value));
    }
};

template <std::size_t _Nm>
struct std::formatter<std::bitset<_Nm>> {
    constexpr auto parse(::std::format_parse_context &ctx) {
        return ctx.begin();
    }

    template <typename _FormatContext>
    auto format(const std::bitset<_Nm> &value, _FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}", value.to_string());
    }
};
