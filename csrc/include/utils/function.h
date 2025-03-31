#pragma once
#include "utils/error.h"
#include <bit>
#include <concepts>
#include <type_traits>
#include <utility>

namespace dark::__detail::function {

template <class _Signature>
struct view_impl;

template <typename _Tp, typename _Up>
concept match_base = std::same_as<std::remove_cvref_t<_Tp>, std::remove_cvref_t<_Up>>;

template <typename _From, typename _To>
concept static_castable = requires { static_cast<_To>(std::declval<_From>()); };

template <typename _Fn, typename _Ret, typename... _Args>
concept invocable_with = // can invoke _Fn with _Args and return _Ret
    std::invocable<_Fn, _Args...> && static_castable<std::invoke_result_t<_Fn, _Args...>, _Ret>;

template <typename _Fn>
concept is_function_like = std::is_function_v<std::remove_reference_t<_Fn>>;

template <typename _Ret2, typename _Ret>
concept can_convert_from = // can convert from _Ret to _Ret2 and not identical
    requires { static_cast<_Ret2>(std::declval<_Ret>()); } && !std::same_as<_Ret2, _Ret>;

struct unsafe_tag {};

template <class _Ret, class... _Args>
struct view_impl<_Ret(_Args...)> {
private:
    using ctx_t = void *;
    using ptr_t = _Ret (*)(ctx_t, _Args...);
    using raw_t = _Ret (*)(_Args...);

    template <typename _Fn>
        requires(!is_function_like<_Fn>)
    static constexpr auto _S_make_ptr(_Fn *) -> ptr_t {
        return [](ctx_t ctx, _Args... args) -> _Ret {
            return static_cast<_Ret>( // force cast to _Ret
                (*static_cast<_Fn *>(ctx))(std::forward<_Args>(args)...)
            );
        };
    }

    template <typename _Fn>
        requires(!is_function_like<_Fn>)
    static constexpr auto _S_make_ctx(_Fn *fn) -> ctx_t {
        using _NoConst = std::remove_const_t<_Fn>;
        return static_cast<ctx_t>(const_cast<_NoConst *>(fn));
    }

    template <typename _Fn>
        requires(is_function_like<_Fn>)
    static constexpr auto _S_make_ptr(_Fn *) -> ptr_t {
        return +[](ctx_t ctx, _Args... args) -> _Ret {
            return static_cast<_Ret>( // force cast to _Ret
                std::bit_cast<_Fn *>(ctx)(std::forward<_Args>(args)...)
            );
        };
    }

    template <typename _Fn>
        requires(is_function_like<_Fn>)
    static constexpr auto _S_make_ctx(_Fn *fn) -> ctx_t {
        return std::bit_cast<ctx_t>(fn);
    }

public:
    using unsafe_t = unsafe_tag;
    // default constructor
    view_impl()  = default;
    ~view_impl() = default;

    // allow trivial copy and move
    view_impl(const view_impl &)                     = default;
    auto operator=(const view_impl &) -> view_impl & = default;

    template <invocable_with<_Ret, _Args...> _Fn>
        requires(static_castable<_Fn, raw_t> || is_function_like<_Fn>)
    view_impl(_Fn &&fn, unsafe_t = {}) {
        if constexpr (static_castable<_Fn, raw_t>) {
            auto fp = static_cast<raw_t>(fn);
            _M_ptr  = _S_make_ptr(fp);
            _M_ctx  = _S_make_ctx(fp);
        } else {
            auto fp = &fn;
            _M_ptr  = _S_make_ptr(fp);
            _M_ctx  = _S_make_ctx(fp);
        }
    }

    template <invocable_with<_Ret, _Args...> _Fn>
        requires(!static_castable<_Fn, raw_t> && !is_function_like<_Fn> &&
                 !match_base<_Fn, view_impl>)
    view_impl(_Fn &fn) : _M_ptr(_S_make_ptr(&fn)), _M_ctx(_S_make_ctx(&fn)) {}

    template <invocable_with<_Ret, _Args...> _Fn>
        requires(!static_castable<_Fn, raw_t> && !is_function_like<_Fn> &&
                 !match_base<_Fn, view_impl>)
    view_impl(_Fn &&fn) = delete;

    template <invocable_with<_Ret, _Args...> _Fn>
        requires(!static_castable<_Fn, raw_t> && !is_function_like<_Fn> && !match_base<_Fn, view_impl>)
    view_impl(_Fn &&fn, unsafe_t) : view_impl(fn) {}

    // for API compatibility
    view_impl(const view_impl &rhs, unsafe_t) : view_impl(rhs) {}

    auto operator()(_Args &&...args) const -> _Ret {
        assume(_M_ptr != nullptr);
        return static_cast<_Ret>(_M_ptr(_M_ctx, std::forward<_Args>(args)...));
    }

    template <typename _Signature = _Ret(_Args...)>
    auto get() const -> _Signature * {
        if (_M_ptr == _S_make_ptr<_Signature>(nullptr))
            return std::bit_cast<_Signature *>(_M_ctx);
        return nullptr;
    }

    operator bool() const {
        return _M_ctx != nullptr;
    }

private:
    ptr_t _M_ptr{}; // new function pointer
    ctx_t _M_ctx{}; // context pointer
};

// this guide is adapted from gcc

template <typename>
struct guide_helper {};

template <typename _Res, typename _Tp, bool _Nx, typename... _Args>
struct guide_helper<_Res (_Tp::*)(_Args...) noexcept(_Nx)> {
    using type = _Res(_Args...);
};

template <typename _Res, typename _Tp, bool _Nx, typename... _Args>
struct guide_helper<_Res (_Tp::*)(_Args...) & noexcept(_Nx)> {
    using type = _Res(_Args...);
};

template <typename _Res, typename _Tp, bool _Nx, typename... _Args>
struct guide_helper<_Res (_Tp::*)(_Args...) const noexcept(_Nx)> {
    using type = _Res(_Args...);
};

template <typename _Res, typename _Tp, bool _Nx, typename... _Args>
struct guide_helper<_Res (_Tp::*)(_Args...) const & noexcept(_Nx)> {
    using type = _Res(_Args...);
};

template <typename _StaticCallOp>
struct guide_static_helper {};

template <typename _Res, bool _Nx, typename... _Args>
struct guide_static_helper<_Res (*)(_Args...) noexcept(_Nx)> {
    using type = _Res(_Args...);
};

template <typename _Fn, typename _Op>
using guide_t = std::conditional_t<requires(const _Fn &__f) {
    (void)__f.operator();
}, guide_static_helper<_Op>, guide_helper<_Op>>::type;

template <typename _Res, typename... _ArgTypes>
view_impl(_Res (*)(_ArgTypes...)) -> view_impl<_Res(_ArgTypes...)>;

template <typename _Fn>
view_impl(_Fn) -> view_impl<guide_t<_Fn, decltype(&_Fn::operator())>>;

} // namespace dark::__detail::function

namespace dark {

using unsafe_function_view_tag = struct __detail::function::unsafe_tag;

template <typename _Signature>
using function_view = __detail::function::view_impl<_Signature>;

} // namespace dark
