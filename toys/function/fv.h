#pragma once
#include <bit>
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

template <class _Signature>
struct function_view;

template <typename _Fn>
inline constexpr auto is_function_view = false;
template <typename _Signature>
inline constexpr auto is_function_view<function_view<_Signature>> = true;

template <typename _Tp, typename _Up>
concept match_decayed = std::same_as<std::decay_t<_Tp>, std::decay_t<_Up>>;

template <typename _Fn, typename _Ret, typename... _Args>
concept can_invoke_with = // can be invoked with _Args... and return _Ret
    requires { static_cast<_Ret>(std::declval<_Fn>()(std::declval<_Args>()...)); };

template <typename _Ret2, typename _Ret>
concept can_convert_from = // can convert from _Ret to _Ret2 and not identical
    requires { static_cast<_Ret2>(std::declval<_Ret>()); } && !std::same_as<_Ret2, _Ret>;

struct unsafe {};

inline constexpr unsafe unsafe{};

template <class _Ret, class... _Args>
struct function_view<_Ret(_Args...)> {
private:
    template <typename _Signature>
    friend struct function_view;

    using self_t = function_view;

    using ptr_t = _Ret (*)(void *, _Args...);
    using raw_t = _Ret (*)(_Args...);
    using ctx_t = void *;

    template <can_invoke_with<_Ret, _Args...> _Fn>
        requires(!std::convertible_to<_Fn, raw_t>)
    static auto _S_make_ptr(_Fn &) -> ptr_t {
        return [](void *ctx, _Args... args) -> _Ret {
            return static_cast<_Ret>( // force cast to _Ret
                (*static_cast<_Fn *>(ctx))(std::forward<_Args>(args)...)
            );
        };
    }

    static auto _S_wrap_raw(raw_t) -> ptr_t {
        return [](void *ctx, _Args... args) -> _Ret {
            return static_cast<_Ret>( // force cast to _Ret
                reinterpret_cast<raw_t>(ctx)(std::forward<_Args>(args)...)
            );
        };
    }

    template <typename _Fn>
    static auto _S_addr(_Fn &fn) -> void * {
        return std::bit_cast<void *>(std::addressof(fn));
    }

public:
    // default constructor
    function_view()  = default;
    ~function_view() = default;

    // allow trivial copy and move
    function_view(const function_view &)                     = default;
    auto operator=(const function_view &) -> function_view & = default;

    function_view(const function_view &rhs, struct unsafe) : function_view(rhs) {}

    // construct from non raw pointer unsafely

    template <can_invoke_with<_Ret, _Args...> _Fn>
        requires(!std::convertible_to<_Fn, raw_t> && !match_decayed<_Fn, self_t>)
    function_view(_Fn &fn) : _M_ptr(_S_make_ptr(fn)), _M_ctx(_S_addr(fn)) {}

    template <can_invoke_with<_Ret, _Args...> _Fn>
        requires(!std::convertible_to<_Fn, raw_t> && !match_decayed<_Fn, self_t>)
    function_view(_Fn &&fn, struct unsafe) :
        _M_ptr(_S_make_ptr(fn)), _M_ctx(_S_addr(fn)) {}

    // construct from raw pointer safely

    function_view(raw_t fn, struct unsafe = {}) :
        _M_ptr(_S_wrap_raw(fn)), _M_ctx(std::bit_cast<void *>(fn)) {}

    auto operator()(_Args &&...args) const -> _Ret {
        return static_cast<_Ret>(_M_ptr(_M_ctx, std::forward<_Args>(args)...));
    }

private:
    ptr_t _M_ptr{}; // new function pointer
    ctx_t _M_ctx{}; // context pointer
};

template <typename _Fn>
struct std_function_signature;
template <typename _Signature>
struct std_function_signature<std::function<_Signature>> {
    using type = _Signature;
};

template <typename _Fn>
using std_function_deduce_t =
    typename std_function_signature<decltype(std::function{std::declval<_Fn>()})>::type;

template <typename _Fn>
function_view(_Fn) -> function_view<std_function_deduce_t<_Fn>>;

template <typename _Fn>
function_view(_Fn, struct unsafe) -> function_view<std_function_deduce_t<_Fn>>;
