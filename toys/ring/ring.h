#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace algebra {

template <typename _Tp>
struct structure_traits : std::false_type {
    static constexpr auto is_commutative = false;
};

template <typename _Tp>
concept group = structure_traits<_Tp>::is_commutative && requires(_Tp a, _Tp b) {
    { -a } -> std::convertible_to<_Tp>;
    { a + b } -> std::convertible_to<_Tp>;
    { a - b } -> std::convertible_to<_Tp>;
    { a += b } -> std::same_as<_Tp &>;
    { a -= b } -> std::same_as<_Tp &>;
    { structure_traits<_Tp>::unit() } -> std::convertible_to<_Tp>;
};

template <typename _Tp>
concept ring = group<_Tp> && requires(_Tp a, _Tp b) {
    { a *b } -> std::convertible_to<_Tp>;
    { a *= b } -> std::same_as<_Tp &>;
    { structure_traits<_Tp>::unit() } -> std::convertible_to<_Tp>;
    { structure_traits<_Tp>::one() } -> std::convertible_to<_Tp>;
};

template <typename _Tp>
concept field = ring<_Tp> && requires(_Tp a, _Tp b) {
    { a / b } -> std::convertible_to<_Tp>;
    { a /= b } -> std::same_as<_Tp &>;
};

template <std::size_t _Nm>
consteval auto default_elem() {
    if constexpr (_Nm <= 1 + std::numeric_limits<std::uint32_t>::max() / 2)
        return std::uint32_t{};
    else if constexpr (_Nm <= 1 + std::numeric_limits<std::uint64_t>::max() / 2)
        return std::uint64_t{};
    else
        static_assert(false, "No suitable type found");
}

template <std::unsigned_integral _Int>
inline constexpr auto is_prime(_Int n) -> bool {
    for (_Int i = 2; i * i <= n; ++i)
        if (n % i == 0)
            return false;
    return true;
}

template <std::size_t _Nm>
concept prime_number = is_prime(_Nm);

template <typename _Tp, typename _To>
concept static_castable_to = requires(_Tp &&a) { static_cast<_To>(a); };

template <std::size_t _Mod, typename _Tp, typename _Up>
inline constexpr auto mul_mod(const _Tp &a, const _Up &b) -> _Tp {
    if constexpr (std::integral<_Tp>) {
        if constexpr (sizeof(_Tp) < 8 || // no worry of overflow
                      _Mod <= std::numeric_limits<std::uint32_t>::max())
            return (std::uint64_t{a} * std::uint64_t{b}) % _Mod;
        else
            return (__uint128_t{a} * __uint128_t{b}) % _Mod;
    } else {
        return (a * b) % _Mod;
    }
}

template <std::size_t _Mod, typename _Tp, typename _Up>
inline constexpr auto safe_pow(const _Tp &base, _Up exp) -> _Tp {
    _Tp result = 1;
    auto base_ = std::uint64_t{base};
    while (exp != 0) {
        if (exp % 2 == 1)
            result = mul_mod<_Mod>(result, base_);
        base_ = mul_mod<_Mod>(base_, base_);
        exp /= 2;
    }
    return result;
}

template <std::size_t _Nm, typename _Storage = decltype(default_elem<_Nm>())>
struct integer_module_group {
public:
    static_assert(_Nm > 0, "Modulo must be positive");

    constexpr integer_module_group(_Storage value = 0) : _M_value(value % _Nm) {}

    template <static_castable_to<_Storage> _Tp>
    constexpr integer_module_group(_Tp &&value) :
        _M_value(static_cast<_Storage>(std::forward<_Tp>(value) % _Nm)) {}

    constexpr auto get() const -> const _Storage & {
        return _M_value;
    }

    constexpr auto get() -> _Storage & {
        return _M_value;
    }

    constexpr auto operator+() const -> integer_module_group {
        return this->get();
    }

    constexpr auto operator-() const -> integer_module_group {
        return {_Nm - this->get()};
    }

    constexpr auto operator+(integer_module_group b) -> integer_module_group {
        return {this->get() + b.get()};
    }

    constexpr auto operator-(integer_module_group b) -> integer_module_group {
        return {this->get() - b.get()};
    }

    constexpr auto operator*(integer_module_group b) -> integer_module_group
        requires prime_number<_Nm>
    {
        return {mul_mod<_Nm>(this->get(), b.get())};
    }

    constexpr auto operator+=(integer_module_group b) -> integer_module_group & {
        return (*this = *this + b);
    }

    constexpr auto operator-=(integer_module_group b) -> integer_module_group & {
        return (*this = *this - b);
    }

    constexpr auto operator*=(integer_module_group b) -> integer_module_group &
        requires prime_number<_Nm>
    {
        return (*this = *this * b);
    }

    constexpr auto operator==(const integer_module_group &b) const -> bool = default;

private:
    _Storage _M_value;
};

template <std::size_t _Nm, typename _Storage = decltype(default_elem<_Nm>())>
    requires prime_number<_Nm>
struct integer_module_ring : private integer_module_group<_Nm> {
public:
    static_assert(_Nm > 1, "Modulo must be at least 1");

    constexpr integer_module_ring(_Storage value = 1) :
        integer_module_group<_Nm>{value} {}

    template <static_castable_to<_Storage> _Tp>
    constexpr integer_module_ring(_Tp &&value) :
        integer_module_group<_Nm>{std::forward<_Tp>(value)} {}

    constexpr auto operator+() const -> integer_module_ring {
        return this->get();
    }

    constexpr auto operator-() const -> integer_module_ring {
        return {safe_pow<_Nm>(this->get(), _Nm - 2)};
    }

    constexpr auto operator+(integer_module_ring b) -> integer_module_ring {
        return {mul_mod<_Nm>(this->get(), b.get())};
    }

    constexpr auto operator-(integer_module_ring b) -> integer_module_ring {
        return (*this + -b);
    }

    constexpr auto operator+=(integer_module_ring b) -> integer_module_ring & {
        return (*this = *this + b);
    }

    constexpr auto operator-=(integer_module_ring b) -> integer_module_ring & {
        return (*this = *this - b);
    }

    constexpr auto operator==(const integer_module_ring &b) const -> bool = default;
};

template <std::size_t _Nm>
struct structure_traits<integer_module_group<_Nm>> : std::true_type {
    static constexpr auto is_commutative = true;

    static constexpr auto unit() -> integer_module_group<_Nm> {
        return integer_module_group<_Nm>{0};
    }

    static constexpr auto one() -> integer_module_group<_Nm> {
        return integer_module_group<_Nm>{1};
    }
};

template <std::size_t _Nm>
struct structure_traits<integer_module_ring<_Nm>> : std::true_type {
    static constexpr auto is_commutative = true;

    static constexpr auto unit() -> integer_module_ring<_Nm> {
        return integer_module_ring<_Nm>{0};
    }
};

static_assert(ring<integer_module_group<7>>);
static_assert(!ring<integer_module_group<6>>);

static_assert(
    integer_module_ring<3>(2) + integer_module_ring<3>(2) == integer_module_ring<3>(1)
);

static_assert(-integer_module_ring<7>(3) == integer_module_ring<7>(5));

} // namespace algebra
