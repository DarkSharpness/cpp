#pragma once
#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace std {

namespace __detail {

template <typename _Tp>
inline constexpr auto is_std_optional_v = false;

template <typename _Tp>
inline constexpr auto is_std_optional_v<std::optional<_Tp>> = true;

template <typename _To, typename _Tp>
inline constexpr auto opt_wrap(_Tp &&result) -> decltype(auto) {
    if constexpr (std::is_void_v<_To>)
        return void();
    else
        return std::forward<_Tp>(result);
}

} // namespace __detail

template <typename _Tp>
struct optional<_Tp &> {
public:
    constexpr optional(std::nullopt_t = std::nullopt) noexcept : _M_ptr(nullptr) {}
    constexpr optional(_Tp *value) noexcept : _M_ptr(value) {}
    constexpr optional(_Tp &value) noexcept : _M_ptr(std::addressof(value)) {}
    constexpr optional(_Tp &&) = delete; // disallow rvalue reference
    // move is as cheap as copy
    constexpr optional(const optional &other)                     = default;
    constexpr auto operator=(const optional &other) -> optional & = default;

    template <typename _Up>
        requires(!std::same_as<_Tp, _Up> && std::convertible_to<_Up, _Tp &>)
    constexpr optional(const optional<_Up> &other) noexcept : _M_ptr(other._M_ptr) {}

    template <typename _Up>
        requires(!std::same_as<_Tp, _Up> && !std::is_reference_v<_Up>)
    constexpr optional(optional<_Up> &&other) = delete;

    constexpr auto operator*() const noexcept -> _Tp & {
        return *_M_ptr;
    }

    constexpr auto operator->() const noexcept -> _Tp * {
        return _M_ptr;
    }

    constexpr explicit operator bool() const noexcept {
        return this->has_value();
    }

    constexpr auto has_value() const noexcept -> bool {
        return _M_ptr != nullptr;
    }

    constexpr auto value() const -> _Tp & {
        if (!this->has_value())
            throw std::bad_optional_access();
        return *_M_ptr;
    }

    template <typename _Up>
    constexpr auto value_or(_Up &&default_value) const -> decltype(auto) {
        return this->has_value() ? *_M_ptr : std::forward<_Up>(default_value);
    }

    template <typename _Fn>
    constexpr auto transform(_Fn &&fn) const {
        using _Result = optional<std::invoke_result_t<_Fn, _Tp &>>;
        return this->has_value() ? _Result{std::forward<_Fn>(fn)(*_M_ptr)} : _Result{};
    }

    template <typename _Fn>
    constexpr auto and_then(_Fn &&fn) const {
        using _Result = std::invoke_result_t<_Fn, _Tp &>;
        static_assert(__detail::is_std_optional_v<_Result>);
        return this->has_value() ? std::forward<_Fn>(fn)(*_M_ptr) : _Result{};
    }

    template <typename _Fn>
    constexpr auto or_else(_Fn &&fn) const {
        static_assert(std::is_same_v<std::invoke_result_t<_Fn>, optional>);
        return this->has_value() ? *this : std::forward<_Fn>(fn)();
    }

    constexpr auto swap(optional &other) noexcept -> void {
        std::swap(_M_ptr, other._M_ptr);
    }

    constexpr auto reset() noexcept -> void {
        *this = optional{};
    }

    constexpr auto emplace(_Tp &value) noexcept -> _Tp & {
        _M_ptr = std::addressof(value);
        return *_M_ptr;
    }

private:
    _Tp *_M_ptr;
};

template <typename _Tp, std::invocable _Fn>
inline constexpr auto operator||(std::optional<_Tp> &&opt, _Fn &&fn) -> decltype(auto) {
    using _Result_t = std::invoke_result_t<_Fn>;
    return opt.has_value() ? __detail::opt_wrap<_Result_t>(*std::move(opt))
                           : std::forward<_Fn>(fn)();
}

template <typename _Tp, std::invocable _Fn>
inline constexpr auto operator||(const std::optional<_Tp> &opt, _Fn &&fn)
    -> decltype(auto) {
    using _Result_t = std::invoke_result_t<_Fn>;
    return opt.has_value() ? __detail::opt_wrap<_Result_t>(*opt)
                           : std::forward<_Fn>(fn)();
}

template <typename _Tp, std::invocable<_Tp &&> _Fn>
inline constexpr auto operator&&(std::optional<_Tp> &&opt, _Fn &&fn) {
    using _Result_t = std::invoke_result_t<_Fn, _Tp>;
    return opt.has_value() ? std::forward<_Fn>(fn)(*std::move(opt))
                           : __detail::opt_wrap<_Result_t>(std::nullopt);
}

template <typename _Tp, std::invocable<const _Tp &> _Fn>
inline constexpr auto operator&&(const std::optional<_Tp> &opt, _Fn &&fn) {
    using _Result_t = std::invoke_result_t<_Fn, _Tp>;
    return opt.has_value() ? std::forward<_Fn>(fn)(*opt)
                           : __detail::opt_wrap<_Result_t>(std::nullopt);
}

template <typename _Tp, std::invocable _Fn>
    requires std::constructible_from<std::optional<_Tp>, std::invoke_result_t<_Fn>>
inline constexpr auto operator|=(std::optional<_Tp> &opt, _Fn &&fn) -> auto & {
    if (!opt.has_value())
        opt = std::forward<_Fn>(fn)();
    return opt;
}

template <typename _Tp, std::invocable<_Tp &&> _Fn>
    requires std::constructible_from<std::optional<_Tp>, std::invoke_result_t<_Fn, _Tp>>
inline constexpr auto operator&=(std::optional<_Tp> &opt, _Fn &&fn) -> auto & {
    if (opt.has_value())
        opt = std::forward<_Fn>(fn)(*std::move(opt));
    return opt;
}

template <typename _Tp, typename _Up>
inline constexpr auto operator|(std::optional<_Tp> &&opt, _Up &&value) -> decltype(auto) {
    return std::move(opt).value_or(std::forward<_Up>(value));
}

template <typename _Tp, typename _Up>
inline constexpr auto operator|(const std::optional<_Tp> &opt, _Up &&value)
    -> decltype(auto) {
    return opt.value_or(std::forward<_Up>(value));
}

template <typename _Tp, typename _Up = std::optional<_Tp>>
    requires std::constructible_from<std::optional<_Tp>, _Up>
inline constexpr auto operator|=(std::optional<_Tp> &opt, _Up &&value) -> auto & {
    if (!opt.has_value())
        opt = std::forward<_Up>(value);
    return opt;
}

template <typename _Tp, typename _Up = std::optional<_Tp>>
    requires std::constructible_from<std::optional<_Tp>, _Up>
inline constexpr auto operator&=(std::optional<_Tp> &opt, _Up &&value) -> auto & {
    if (opt.has_value())
        opt = std::forward<_Up>(value);
    return opt;
}

} // namespace std
