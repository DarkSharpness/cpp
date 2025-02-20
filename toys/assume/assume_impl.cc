#ifdef _LOCAL
// if local, include as a header
#pragma once
#endif

#include "assume.h"
#include <exception>
#include <forward_list>
#include <iostream>
#include <mutex>
#include <shared_mutex>

namespace dark {

static auto get_impl() noexcept -> decltype(auto) {
    struct Impl {
        std::shared_mutex hooks_mutex;
        std::forward_list<panic_handler::Hook_t> hooks;
    };
    static auto impl = Impl{
        .hooks_mutex = {},
        .hooks       = {[](const std::string &str, std::source_location loc,
                     const char *detail) {
            auto msg = std::format(
                "\033[1;31m{}\033[0m"
                      " at {}:{}: {}\n",
                detail, loc.file_name(), loc.line(), str
            );
            if (str.empty()) {
                msg.resize(msg.size() - 3);
                msg.back() = '\n';
            }
            std::cerr << msg;
            throw std::runtime_error(detail);
        }}
    };
    return (impl);
}

[[noreturn]]
auto panic_handler::trap(
    const std::string &str, std::source_location loc, const char *detail
) -> void {
    auto &[mutex, hooks] = get_impl();
    const auto lock      = std::shared_lock{mutex};
    for (const auto &hook : hooks)
        hook(str, loc, detail);
    std::terminate();
}

auto panic_handler::add_hook(Hook_t hook) -> void {
    auto &[mutex, hooks] = get_impl();
    const auto lock      = std::unique_lock{mutex};
    hooks.push_front(hook);
}

auto panic_handler::pop_hook() -> Hook_t {
    auto &[mutex, hooks] = get_impl();
    const auto lock      = std::unique_lock{mutex};
    if (hooks.empty())
        return {};
    auto last_hook = std::move(hooks.front());
    hooks.pop_front();
    return last_hook;
}

} // namespace dark
