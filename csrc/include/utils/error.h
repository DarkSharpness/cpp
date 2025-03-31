#pragma once
#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <source_location>
#include <syncstream>
#include <type_traits>
#include <utility>

namespace dark {

inline constexpr auto IN_DEBUG =
#ifdef _DARK_DEBUG
    true
#else
    false
#endif
    ;

template <std::invocable _Fn>
[[gnu::always_inline]]
inline auto call_in_debug_mode(_Fn &&fn) -> void {
    if constexpr (IN_DEBUG)
        std::invoke(std::forward<_Fn>(fn));
}

namespace __detail {

template <typename _Str>
concept string_like =
    requires(_Str str) { static_cast<std::string>(str); } || std::same_as<_Str, void>;

template <typename _Fn, typename... _Args>
concept callable =
    std::invocable<_Fn, _Args...> && string_like<std::invoke_result_t<_Fn, _Args...>>;

template <typename _Fn, typename... _Args>
static auto invoke_callable(_Fn &&fn, _Args &&...args) -> std::string {
    if constexpr (std::same_as<std::invoke_result_t<_Fn, _Args...>, void>) {
        std::invoke(std::forward<_Fn>(fn), std::forward<_Args>(args)...);
        return std::string{};
    } else {
        return static_cast<std::string>(
            std::invoke(std::forward<_Fn>(fn), std::forward<_Args>(args)...)
        );
    }
}

template <typename _Tp>
concept condition = requires(_Tp &&cond) { static_cast<bool>(cond); };

} // namespace __detail

struct panic_handler {
    [[noreturn]]
    static auto trap(const std::string &, std::source_location, const char *) -> void;
    using Hook_t = std::function<decltype(trap)>;

    static auto add_hook(Hook_t) -> void;
    static auto pop_hook(void) -> Hook_t;
};

template <typename... _Args>
    requires(sizeof...(_Args) > 0)
[[noreturn, gnu::cold]]
inline auto panic(
    std::format_string<_Args...> fmt, _Args &&...args,
    std::source_location loc = std::source_location::current(),
    const char *detail       = "program panicked"
) -> void {
    panic_handler::trap(std::format(fmt, std::forward<_Args>(args)...), loc, detail);
}

template <typename... _Args>
    requires(sizeof...(_Args) == 0)
[[noreturn, gnu::cold]]
inline auto panic(
    const std::string &msg   = "", _Args &&...,
    std::source_location loc = std::source_location::current(),
    const char *detail       = "program panicked"
) -> void {
    panic_handler::trap(msg, loc, detail);
}

template <__detail::condition _Cond = bool, typename... _Args>
struct assume {
private:
    using _Src_t = std::source_location;

public:
    [[gnu::always_inline]]
    explicit assume(_Cond &&cond, _Src_t loc = _Src_t::current())
        requires(sizeof...(_Args) == 0)
    {
        if (cond) [[likely]]
            return;
        if constexpr (IN_DEBUG)
            std::unreachable();
        panic(loc.function_name(), loc, "assumption failed");
    }

    [[gnu::always_inline]]
    explicit assume(
        _Cond &&cond, std::format_string<_Args...> fmt, _Args &&...args,
        _Src_t loc = _Src_t::current()
    ) {
        if (cond) [[likely]]
            return;
        if constexpr (IN_DEBUG)
            std::unreachable();
        if constexpr (sizeof...(args) == 0)
            panic(std::string(fmt.get()), loc, "assumption failed");
        else
            panic<_Args...>(fmt, std::forward<_Args>(args)..., loc, "assumption failed");
    }

    template <__detail::callable<_Args...> _Fn>
    [[gnu::always_inline]]
    explicit assume(_Cond &&cond, _Fn &&fn, _Args &&...args, _Src_t loc = _Src_t::current()) {
        if (cond) [[likely]]
            return;
        if constexpr (IN_DEBUG)
            std::unreachable();
        panic(
            __detail::invoke_callable(std::forward<_Fn>(fn), std::forward<_Args>(args)...), loc,
            "assumption failed"
        );
    }
};

template <typename _Cond>
explicit assume(_Cond &&) -> assume<_Cond>;

template <typename _Cond, typename... _Args>
explicit assume(_Cond &&, std::format_string<_Args...>, _Args &&...) -> assume<_Cond, _Args...>;

template <typename _Cond, typename... _Args, __detail::callable<_Cond, _Args...> _Fn>
explicit assume(_Cond &&, _Fn &&, _Args &&...) -> assume<_Cond, _Args...>;

auto debugger(std::optional<bool> = std::nullopt) -> std::osyncstream;

} // namespace dark
