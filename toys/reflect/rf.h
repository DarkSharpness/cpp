#pragma once
#include <tuple>
#include <type_traits>

namespace reflect {

template <typename _Tp>
concept aggregate_type = std::is_aggregate_v<std::decay_t<_Tp>>;

namespace __detail {

/* A init helper to get the size of a struct. */
struct any_init {
    template <typename _Tp>
    constexpr operator _Tp();
};

struct arg_counter {
    template <typename... _Args>
    auto operator()(_Args &&...) -> std::integral_constant<int, sizeof...(_Args)>;
};

template <aggregate_type _Tp>
inline consteval auto member_size_fallback() -> std::size_t {
    static_assert(false, "The struct has too many members.");
    return 0;
}

static constexpr auto kMaxMember = 256;

/* A size helper to get the size of a struct. */
template <aggregate_type _Tp>
inline consteval auto member_size_aux(auto &&...args) -> std::size_t {
    constexpr std::size_t kSize = sizeof...(args);
    constexpr std::size_t kMax  = kMaxMember;
    if constexpr (kSize > kMax) {
        return member_size_fallback<_Tp>();
    } else if constexpr (!requires { _Tp{args...}; }) {
        return kSize - 1;
    } else {
        return member_size_aux<_Tp>(args..., any_init{});
    }
}

template <aggregate_type _Tp>
inline consteval auto member_size(const _Tp &) -> std::size_t {
    return member_size_aux<_Tp>();
}

template <aggregate_type _Tp>
inline consteval auto member_size() -> std::size_t {
    return member_size_aux<std::remove_cvref_t<_Tp>>();
}

template <aggregate_type _Tp>
inline constexpr auto tuplify_fallback(_Tp &) {
    static_assert(false, "The struct has too many members.");
}

template <aggregate_type _Tp>
constexpr auto tuplify(_Tp &value) {
    constexpr auto size = member_size_aux<_Tp>();

    if constexpr (size == 0)
        return std::tuple<>();

#define _UNFOLD0(x) x
#define _UNFOLD1(x) _UNFOLD0(x##0), _UNFOLD0(x##1)
#define _UNFOLD2(x) _UNFOLD1(x##0), _UNFOLD1(x##1)
#define _UNFOLD3(x) _UNFOLD2(x##0), _UNFOLD2(x##1)
#define _UNFOLD4(x) _UNFOLD3(x##0), _UNFOLD3(x##1)
#define _UNFOLD5(x) _UNFOLD4(x##0), _UNFOLD4(x##1)
#define _UNFOLD6(x) _UNFOLD5(x##0), _UNFOLD5(x##1)
#define _UNFOLD7(x) _UNFOLD6(x##0), _UNFOLD6(x##1)

#define _TUPLIFY_HELPER_0(n, ...)                                                        \
    else if constexpr (constexpr auto m = (n) + 1; size == m) {                          \
        auto &&[__VA_ARGS__ _y] = value;                                                 \
        static_assert(decltype(arg_counter{}(__VA_ARGS__ _y))::value == m);              \
        static_assert(m <= kMaxMember);                                                  \
        return std::forward_as_tuple(__VA_ARGS__ _y);                                    \
    }

#define _TUPLIFY_HELPER_1(n, ...)                                                        \
    _TUPLIFY_HELPER_0(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_0(n | 1, _UNFOLD0(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_2(n, ...)                                                        \
    _TUPLIFY_HELPER_1(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_1(n | 2, _UNFOLD1(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_3(n, ...)                                                        \
    _TUPLIFY_HELPER_2(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_2(n | 4, _UNFOLD2(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_4(n, ...)                                                        \
    _TUPLIFY_HELPER_3(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_3(n | 8, _UNFOLD3(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_5(n, ...)                                                        \
    _TUPLIFY_HELPER_4(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_4(n | 16, _UNFOLD4(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_6(n, ...)                                                        \
    _TUPLIFY_HELPER_5(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_5(n | 32, _UNFOLD5(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_7(n, ...)                                                        \
    _TUPLIFY_HELPER_6(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_6(n | 64, _UNFOLD6(x), __VA_ARGS__)

#define _TUPLIFY_HELPER_8(n, ...)                                                        \
    _TUPLIFY_HELPER_7(n | 0, __VA_ARGS__)                                                \
    _TUPLIFY_HELPER_7(n | 128, _UNFOLD7(x), __VA_ARGS__)

    // This is the unfold implementation of the above macros.
    _TUPLIFY_HELPER_8(0)

#undef _TUPLIFY_HELPER_0
#undef _TUPLIFY_HELPER_1
#undef _TUPLIFY_HELPER_2
#undef _TUPLIFY_HELPER_3
#undef _TUPLIFY_HELPER_4
#undef _TUPLIFY_HELPER_5
#undef _TUPLIFY_HELPER_6
#undef _TUPLIFY_HELPER_7
#undef _TUPLIFY_HELPER_8

#undef _UNFOLD0
#undef _UNFOLD1
#undef _UNFOLD2
#undef _UNFOLD3
#undef _UNFOLD4
#undef _UNFOLD5
#undef _UNFOLD6
#undef _UNFOLD7

    else return tuplify_fallback<_Tp>(value);
}

} // namespace __detail

template <aggregate_type _Tp>
constexpr auto tuplify(_Tp &&value) -> decltype(auto) {
    auto ref_tuple = __detail::tuplify(value);
    if constexpr (std::is_reference_v<_Tp>)
        return ref_tuple;
    else // move to a value tuple
        return std::apply(
            [](auto &&...args) { return std::tuple(std::move(args)...); }, ref_tuple
        );
}

template <aggregate_type _Tp, typename... _Arg>
constexpr auto untuplify(const std::tuple<_Arg...> &tuple) -> _Tp {
    return [&]<std::size_t... _I>(std::index_sequence<_I...>) {
        return _Tp{std::get<_I>(tuple)...};
    }(std::make_index_sequence<sizeof...(_Arg)>{});
}

template <aggregate_type _Tp, typename... _Arg>
constexpr auto untuplify(std::tuple<_Arg...> &&tuple) -> _Tp {
    return [&]<std::size_t... _I>(std::index_sequence<_I...>) {
        return _Tp{(std::get<_I>(std::move(tuple)))...};
    }(std::make_index_sequence<sizeof...(_Arg)>{});
}

using __detail::member_size;

} // namespace reflect
