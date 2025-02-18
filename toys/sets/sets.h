#pragma once
#include <compare>
#include <concepts>
#include <stdexcept>
#include <type_traits>

inline namespace set_operation {

template <typename _Tp>
struct set_op_traits : std::false_type {};

template <typename _Tp>
concept set_like = set_op_traits<_Tp>::value;

namespace __detail {

template <typename _Tp>
concept set_has_default_and = requires(const _Tp &a) {
    { std::move(a) & std::move(a) } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept set_has_default_or = requires(const _Tp &a) {
    { std::move(a) | std::move(a) } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept set_has_default_xor = requires(const _Tp &a) {
    { std::move(a) ^ std::move(a) } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept set_has_default_sub = requires(const _Tp &a) {
    { std::move(a) / std::move(a) } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept set_has_default_cmp = requires(const _Tp &a) {
    { std::move(a) <=> std::move(a) } -> std::same_as<std::partial_ordering>;
};

template <typename _Tp>
concept set_has_default_not = requires(const _Tp &a) {
    { ~a } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept set_has_default_eq = std::equality_comparable<_Tp>;

template <set_like _Tp>
struct set_op_impl : set_op_traits<_Tp> {
private:
    static constexpr auto has_and = set_has_default_and<_Tp>;
    static constexpr auto has_or  = set_has_default_or<_Tp>;
    static constexpr auto has_xor = set_has_default_xor<_Tp>;
    static constexpr auto has_sub = set_has_default_sub<_Tp>;
    static constexpr auto has_cmp = set_has_default_cmp<_Tp>;

    static constexpr auto has_not = set_has_default_not<_Tp>;
    static constexpr auto has_eq  = set_has_default_eq<_Tp>;

public:

    static constexpr auto op_and(const _Tp &a, const _Tp &b) -> decltype(auto)
        requires(has_sub || (has_or && (has_xor || has_not)))
    {
        if constexpr (has_and) {
            return a & b;
        } else if constexpr (has_sub) {
            return a / (a / b);
        } else if constexpr (has_or && has_xor) {
            return (a | b) ^ (a ^ b);
        } else if constexpr (has_or && has_not) {
            return ~((~a) | (~b));
        } else {
            static_assert(false);
        }
    }

    static constexpr auto op_or(const _Tp &a, const _Tp &b) -> decltype(auto)
        requires((has_and || has_sub) && (has_xor || has_not))
    {
        if constexpr (has_or) {
            return a | b;
        } else if constexpr (has_sub && has_xor) {
            return (a / b) ^ b;
        } else if constexpr (has_and && has_xor) {
            return (a & b) ^ (a ^ b);
        } else if constexpr (has_sub && has_not) {
            return ~((~a) / b);
        } else if constexpr (has_and && has_not) {
            return ~((~a) & (~b));
        } else {
            static_assert(false);
        }
    }

    static constexpr auto op_xor(const _Tp &a, const _Tp &b) -> decltype(auto)
        requires((has_not && (has_sub || has_or || has_and)) || (has_sub && has_or))
    {
        if constexpr (has_xor) {
            return a ^ b;
        } else if constexpr (has_sub && has_or) {
            return (a / b) | (b / a);
        } else if constexpr (has_not && has_sub) {
            const auto tmp = ~b;
            return (~(tmp / a)) / (a / tmp);
        } else if constexpr (has_not && has_or) {
            return (~(a | ~b)) | (~(~a | b));
        } else if constexpr (has_not && has_and) {
            return (~(a & b)) & (~((~a) & (~b)));
        } else {
            static_assert(false);
        }
    }

    static constexpr auto op_sub(const _Tp &a, const _Tp &b) -> decltype(auto)
        requires((has_and || has_or) && (has_xor || has_not))
    {
        if constexpr (has_sub) {
            return a / b;
        } else if constexpr (has_and && has_xor) {
            return (a & b) ^ a;
        } else if constexpr (has_or && has_xor) {
            return (a | b) ^ b;
        } else if constexpr (has_and && has_not) {
            return a & (~b);
        } else if constexpr (has_or && has_not) {
            return ~(~a | b);
        } else {
            static_assert(false);
        }
    }

    static constexpr auto op_cmp(const _Tp &a, const _Tp &b) -> std::partial_ordering
        requires(
            has_eq && (has_and || has_or || (has_sub && std::constructible_from<_Tp>))
        )
    {
        if constexpr (has_cmp) {
            return a <=> b;
        } else {
            const auto [pred_le, pred_ge] = [&a, &b] {
                struct Pred {
                    bool le, ge;
                };
                if constexpr (has_and) {
                    const auto set_intersect = a & b;
                    return Pred{a == set_intersect, b == set_intersect};
                } else if constexpr (has_or) {
                    const auto set_union = a | b;
                    return Pred{b == set_union, a == set_union};
                } else {
                    const auto empty = _Tp{};
                    return Pred{a / b == empty, b / a == empty};
                }
            }();

            if (pred_ge) {
                return pred_le ? std::partial_ordering::equivalent
                               : std::partial_ordering::greater;
            } else {
                return pred_le ? std::partial_ordering::less
                               : std::partial_ordering::unordered;
            }
        }
    }
};

template <typename _Tp>
concept set_generate_and_ =
    set_like<_Tp> && !set_has_default_and<_Tp> && requires { set_op_impl<_Tp>::op_and; };

template <typename _Tp>
concept set_generate_or_ =
    set_like<_Tp> && !set_has_default_or<_Tp> && requires { set_op_impl<_Tp>::op_or; };

template <typename _Tp>
concept set_generate_xor_ =
    set_like<_Tp> && !set_has_default_xor<_Tp> && requires { set_op_impl<_Tp>::op_xor; };

template <typename _Tp>
concept set_generate_sub_ =
    set_like<_Tp> && !set_has_default_sub<_Tp> && requires { set_op_impl<_Tp>::op_sub; };

template <typename _Tp>
concept set_generate_cmp_ =
    set_like<_Tp> && !set_has_default_cmp<_Tp> && requires { set_op_impl<_Tp>::op_cmp; };

template <typename _Tp>
concept set_generate_and = set_generate_and_<std::remove_cvref_t<_Tp>>;

template <typename _Tp>
concept set_generate_or = set_generate_or_<std::remove_cvref_t<_Tp>>;

template <typename _Tp>
concept set_generate_xor = set_generate_xor_<std::remove_cvref_t<_Tp>>;

template <typename _Tp>
concept set_generate_sub = set_generate_sub_<std::remove_cvref_t<_Tp>>;

template <typename _Tp>
concept set_generate_cmp = set_generate_cmp_<std::remove_cvref_t<_Tp>>;

// can't be 2 const &&types, yet must be the same type
template <typename _Tp, typename _Up>
concept base_same = std::same_as<std::remove_cvref_t<_Tp>, std::remove_cvref_t<_Up>> &&
                    !(std::is_const_v<_Tp> && std::is_const_v<_Up>);

} // namespace __detail

template <typename _Tp>
concept basic_set_like = set_like<_Tp> && requires(_Tp a) {
    { a &a } -> std::same_as<_Tp>;
    { a | a } -> std::same_as<_Tp>;
    { a ^ a } -> std::same_as<_Tp>;
};

template <typename _Tp>
concept full_set_like = set_like<_Tp> && std::constructible_from<_Tp> && requires(_Tp a) {
    { ~a } -> std::same_as<_Tp>;
};

template <typename _Tp, __detail::base_same<_Tp> _Up>
    requires(__detail::set_generate_and<_Tp>)
inline constexpr auto operator&(_Tp &&a, _Up &&b) -> decltype(auto) {
    return __detail::set_op_impl<std::remove_cvref_t<_Tp>>::op_and(a, b);
}

template <typename _Tp, __detail::base_same<_Tp> _Up>
    requires(__detail::set_generate_or<_Tp>)
inline constexpr auto operator|(_Tp &&a, _Up &&b) -> decltype(auto) {
    return __detail::set_op_impl<std::remove_cvref_t<_Tp>>::op_or(a, b);
}

template <typename _Tp, __detail::base_same<_Tp> _Up>
    requires(__detail::set_generate_xor<_Tp>)
inline constexpr auto operator^(_Tp &&a, _Up &&b) -> decltype(auto) {
    return __detail::set_op_impl<std::remove_cvref_t<_Tp>>::op_xor(a, b);
}

template <typename _Tp, __detail::base_same<_Tp> _Up>
    requires(__detail::set_generate_sub<_Tp>)
inline constexpr auto operator/(_Tp &&a, _Up &&b) -> decltype(auto) {
    return __detail::set_op_impl<std::remove_cvref_t<_Tp>>::op_sub(a, b);
}

template <typename _Tp, __detail::base_same<_Tp> _Up>
    requires(__detail::set_generate_cmp<_Tp>)
inline constexpr auto operator<=>(_Tp &&a, _Up &&b) -> std::partial_ordering {
    return __detail::set_op_impl<std::remove_cvref_t<_Tp>>::op_cmp(a, b);
}

template <std::constructible_from<> _Tp>
inline constexpr auto empty_set() -> _Tp {
    return _Tp{};
}

template <full_set_like _Tp>
inline constexpr auto full_set() -> _Tp {
    return ~_Tp{};
}

template <basic_set_like _Tp>
    requires(std::constructible_from<_Tp> && std::equality_comparable<_Tp>)
inline constexpr auto operator+(const _Tp &a, const _Tp &b) -> decltype(auto) {
    if ((a & b) != empty_set<_Tp>())
        throw std::invalid_argument{"Sets must be disjoint"};
    return a | b;
}

template <basic_set_like _Tp>
    requires(std::constructible_from<_Tp> && std::equality_comparable<_Tp>)
inline constexpr auto operator-(const _Tp &a, const _Tp &b) -> decltype(auto) {
    if ((a & b) != b)
        throw std::invalid_argument{"Sets must be disjoint"};
    return a / b;
}

} // namespace set_operation
