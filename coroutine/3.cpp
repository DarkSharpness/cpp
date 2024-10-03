
#include <any>
#include <coroutine>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <ranges>

inline namespace dark {

struct promise;

struct coroutine : std::coroutine_handle<promise> {
    using promise_type = ::dark::promise;
    using super        = std::coroutine_handle<promise_type>;

    coroutine() = default;
    using super::coroutine_handle;
    coroutine(super handle) : super(handle) {}

    auto get_promise() const -> promise_type & {
        return std::coroutine_handle<promise_type>::promise();
    }

    struct iterator {
        using iterator_category = std::input_iterator_tag;
        using value_type        = std::any;
        using difference_type   = std::ptrdiff_t;

        std::reference_wrapper<coroutine> coro;
        auto operator==(std::nullptr_t) const -> bool {
            return coro.get().done();
        }
        auto operator*() -> std::any;
        auto operator++() -> iterator & {
            coro.get().resume();
            return *this;
        }
        template <void * = nullptr>
        auto operator++(int) -> void {
            static_assert(false, "Not implemented");
        }
    };

    auto begin() -> iterator {
        return iterator{*this};
    }

    auto end() -> std::nullptr_t {
        return nullptr;
    }

    auto operator*() -> std::any;
};

struct promise {
public:

    auto get_return_object() noexcept -> coroutine {
        return coroutine::from_promise(*this);
    }

    auto initial_suspend() noexcept -> std::suspend_never {
        return {};
    }

    auto final_suspend() noexcept -> std::suspend_always {
        return {};
    }

    auto return_void() noexcept -> void {}

    auto yield_void() noexcept -> std::suspend_always {
        this->value.reset();
        return {};
    }

    auto yield_value(std::any value) noexcept -> std::suspend_always {
        this->value = std::move(value);
        return std::suspend_always{};
    }

    auto unhandled_exception() noexcept -> void {}

public:

    auto get_value() noexcept -> std::any & {
        return value;
    }

    auto get_value() const noexcept -> const std::any & {
        return value;
    }

private:

    std::any value;
};

inline auto coroutine::iterator::operator*() -> std::any {
    return coro.get().get_promise().get_value();
}

struct awaiter {
public:
    auto await_ready() noexcept -> bool {
        return this->coro != nullptr;
    }
    auto await_suspend(coroutine coro) noexcept -> bool {
        this->coro = &coro;
        return false;
    }
    auto await_resume() noexcept -> std::any {
        if (coro) {
            return std::move(coro->get_promise().get_value());
        } else {
            return {};
        }
    }

private:
    coroutine *coro = {};
};

auto range_integer(int min, int max) -> coroutine {
    for (int i = min; i < max; ++i) {
        co_yield i;
    }
}

auto fib() -> coroutine {
    co_yield 0;
    co_yield 1;
    int a = 0;
    int b = 1;
    while (true) {
        int c = a + b;
        co_yield c;
        a = b;
        b = c;
    }
}

} // namespace dark

auto main() -> int {
    for (auto value : range_integer(0, 10)) {
        std::cout << std::any_cast<int>(value) << std::endl;
    }
    std::cout << "----------------" << std::endl;
    for (auto value : fib() | std::ranges::views::take(10)) {
        std::cout << std::any_cast<int>(value) << std::endl;
    }
}
