#include "formatter.h" // IWYU pragma: keep
#include "tag.h"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace {

template <typename _Tp>
inline constexpr auto is_tuple_v = false;

template <typename... _Args>
inline constexpr auto is_tuple_v<std::tuple<_Args...>> = true;

template <typename... _Args>
inline constexpr auto is_tuple_v<std::tuple<_Args...> &> = true;

// atomic concept
template <typename _Tp>
concept tuple_like = is_tuple_v<_Tp>;

template <typename... _Args>
using tag_tuple = tag<std::tuple<_Args...>>;

template <tuple_like... _Tuples>
struct tuple_product_aux;

template <tuple_like... _Tuples>
using tuple_product_t = typename tuple_product_aux<_Tuples...>::type;

template <typename _Tp, typename... _Others>
consteval auto tag_add(tag<std::tuple<_Others...>>) -> std::tuple<_Tp, _Others...>;

template <tuple_like... _Contents>
consteval auto tag_mul(tag_tuple<>, tag_tuple<_Contents...>) -> tag_tuple<>;

template <typename _Tp, typename... _Args, tuple_like... _Contents>
consteval auto tag_mul(tag_tuple<_Tp, _Args...>, tag_tuple<_Contents...> rest) {
    using _This = std::tuple<decltype(tag_add<_Tp>(tag<_Contents>{}))...>;
    using _Prev = untag<decltype(tag_mul(tag_tuple<_Args...>{}, rest))>;
    return tag<decltype(std::tuple_cat(std::declval<_This>(), std::declval<_Prev>()))>{};
}

template <>
struct tuple_product_aux<> {
    using type = std::tuple<std::tuple<>>;
};

template <tuple_like _Tp, tuple_like... _Others>
struct tuple_product_aux<_Tp, _Others...> {
private:
    using _Rest = untag<tuple_product_aux<_Others...>>;

public:
    using type = untag<decltype(tag_mul(tag<_Tp>{}, tag<_Rest>{}))>;
};

template <typename _Tp>
struct reference {
public:
    using value_type = std::remove_reference_t<_Tp>;
    constexpr reference() : _M_value{nullptr} {}
    constexpr reference(value_type &value) : _M_value{&value} {}
    constexpr operator value_type() const {
        if (!_M_value) // this is actually unreachable
            std::terminate();
        return *_M_value;
    }
    constexpr auto set(const value_type &value) -> void {
        _M_value = &value;
    }

private:
    const value_type *_M_value;
};

template <tuple_like _Tp>
struct tuple_ctor_aux {};

template <typename... _Args>
struct tuple_ctor_aux<std::tuple<_Args...>> {
    struct ref_tuple : std::tuple<reference<_Args>...> {
        using std::tuple<reference<_Args>...>::tuple;
        constexpr operator std::tuple<_Args...>() const {
            return [this]<std::size_t... _I>(std::index_sequence<_I...>) {
                return std::tuple<_Args...>{std::get<_I>(*this)...};
            }(std::make_index_sequence<sizeof...(_Args)>{});
        }
    };
    using type = ref_tuple;
};

template <tuple_like _Tp>
using tuple_ctor_t = untag<tuple_ctor_aux<_Tp>>;

template <std::size_t... _Dim, typename _Ctor, tuple_like... _Tuples>
constexpr auto fill_ctor(_Ctor &ctor, const _Tuples &...args) -> void {
    return [&]<std::size_t... _I>(std::index_sequence<_I...>) {
        constexpr auto _Dims = std::array{_Dim...};
        // the i-th element of ctor is a reference to the i-th element of args
        ((std::get<_I>(ctor) = reference{std::get<_Dims[_I]>(std::get<_I>(args))}), ...);
    }(std::index_sequence_for<_Tuples...>{});
}

template <std::size_t _Idx, std::size_t... _Dims>
constexpr auto make_ctor_array() -> std::array<std::size_t, sizeof...(_Dims)> {
    auto sizes  = std::array{_Dims...};
    auto result = std::array<std::size_t, sizeof...(_Dims)>{};
    auto index  = _Idx;
    std::ranges::reverse(sizes);
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        result[i] = index % sizes[i];
        index /= sizes[i];
    }
    std::ranges::reverse(result);
    return result;
}

template <std::size_t _Idx, typename _Ctor, tuple_like... _Tuples>
constexpr auto fill_ctor_aux(_Ctor &ctor, const _Tuples &...args) -> void {
    // factor _Idx into a sequence of indices as ..._Dim
    return [&]<std::size_t... _I>(std::index_sequence<_I...>) {
        constexpr auto _Dims = make_ctor_array<_Idx, std::tuple_size_v<_Tuples>...>();
        // the i-th element of ctor is a reference to the i-th element of args
        (std::get<_I>(ctor).set(std::get<_Dims[_I]>(args)), ...);
    }(std::index_sequence_for<_Tuples...>{});
}

template <tuple_like... _Tuples>
constexpr auto init_ctor(tag<std::tuple<_Tuples...>>) {
    return std::tuple<tuple_ctor_t<_Tuples>...>{};
}

template <tuple_like... _Tuples>
constexpr auto tuple_product(const _Tuples &...args) {
    using _Result_t = tuple_product_t<_Tuples...>;
    if constexpr (std::same_as<_Result_t, std::tuple<std::tuple<>>>) {
        return _Result_t{};
    } else {
        auto result = init_ctor(tag<_Result_t>{});
        // need to make all the possible combinations
        [&]<std::size_t... _I>(std::index_sequence<_I...>) {
            (fill_ctor_aux<_I>(std::get<_I>(result), args...), ...);
        }(std::make_index_sequence<std::tuple_size_v<_Result_t>>{});
        return static_cast<_Result_t>(result);
    }
}

} // namespace

static auto unit_test0() -> void;
static auto unit_test1() -> void;
static auto unit_test2() -> void;

struct no_ctor {
    int v;
    constexpr no_ctor() = delete;
    constexpr no_ctor(int v) : v{v} {}
    friend auto operator==(const no_ctor &, const no_ctor &) -> bool = default;
};

auto main() -> int {
    unit_test0();
    unit_test1();
    unit_test2();
}

static auto unit_test0() -> void {
    using T = std::tuple<int, long>;
    using U = std::tuple<bool, char>;
    using V = std::tuple<float, double>;
    using W = tuple_product_t<T, U, V>;

    using STD = std::tuple< // the answer
        std::tuple<int, bool, float>, std::tuple<int, bool, double>,
        std::tuple<int, char, float>, std::tuple<int, char, double>,
        std::tuple<long, bool, float>, std::tuple<long, bool, double>,
        std::tuple<long, char, float>, std::tuple<long, char, double>>;

    static_assert(std::is_same_v<W, STD>);

    constexpr auto result = tuple_product(T{1, 2}, U{true, 'a'}, V{3.1f, 4.2});
    static_assert(
        result == STD{std::tuple{1, true, 3.1f}, std::tuple{1, true, 4.2},
                      std::tuple{1, 'a', 3.1f}, std::tuple{1, 'a', 4.2},
                      std::tuple{2, true, 3.1f}, std::tuple{2, true, 4.2},
                      std::tuple{2, 'a', 3.1f}, std::tuple{2, 'a', 4.2}}
    );
    std::cout << std::format("{}\n", result);
}

static auto unit_test1() -> void {
    const auto x   = std::tuple{0ll, 1ll, 2ll};
    const auto y   = std::tuple{std::string{"hello"}, std::tuple{}};
    const auto z   = std::tuple{3.14};
    const auto w   = std::tuple{false, true};
    const auto v   = tuple_product(x, y, z, w);
    const auto msg = std::format("{}\n", v);
    const auto result =
        "<<0, hello, 3.14, false>, <0, hello, 3.14, true>, <0, <>, 3.14, false>, <0, <>, "
        "3.14, true>, <1, hello, 3.14, false>, <1, hello, 3.14, true>, <1, <>, 3.14, "
        "false>, <1, <>, 3.14, true>, <2, hello, 3.14, false>, <2, hello, 3.14, true>, "
        "<2, <>, 3.14, false>, <2, <>, 3.14, true>>\n";
    if (msg != result)
        std::terminate();
    std::cout << msg;
}

static auto unit_test2() -> void {
    constexpr auto x = std::tuple{0ll, no_ctor{0}};
    constexpr auto y = std::tuple{no_ctor{1}};
    constexpr auto z = tuple_product(x, y);
    static_assert(
        z == std::tuple{std::tuple{0ll, no_ctor{1}}, std::tuple{no_ctor{0}, no_ctor{1}}}
    );
}
