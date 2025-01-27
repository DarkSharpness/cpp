#include <chrono>
#include <coroutine>
#include <vector>

struct AwaitSleep {
private:
    using _Clock_t    = std::chrono::steady_clock;
    using _Time_t     = decltype(_Clock_t::now());
    using _Duration_t = decltype(std::declval<_Time_t>() - std::declval<_Time_t>());

public:
    auto await_ready() const -> bool {
        return false;
    }
    auto await_resume() const -> void {}
    auto await_suspend(std::coroutine_handle<>) const -> void {}
    AwaitSleep(_Duration_t d) : _M_finish(_Clock_t::now() + d) {}

private:
    _Time_t _M_finish;
};

struct Promise;

struct MyCoro : std::coroutine_handle<Promise> {
    using promise_type = ::Promise;
    using super        = std::coroutine_handle<promise_type>;
    using super::coroutine_handle;

    MyCoro() = default;
    MyCoro(super handle) : super(handle) {}

    auto &get_handle() {
        return *this;
    }
};

struct Promise {
    auto initial_suspend() noexcept -> std::suspend_always {
        return {};
    }
    auto get_return_object() -> MyCoro {
        return MyCoro::from_promise(*this);
    }
    auto final_suspend() noexcept -> std::suspend_always {
        return {};
    }
    auto return_void() noexcept -> void {}
    auto unhandled_exception() noexcept -> void {}
};

static auto pseudo_io(int n) -> MyCoro {
    co_await AwaitSleep{std::chrono::seconds(n)};
    std::puts("I/O operation completed.");
}

auto main() -> int {
    auto vec = std::vector<MyCoro>{};
    vec.reserve(5);
    for (int i = 0; i < 5; ++i)
        vec.push_back(pseudo_io(50));
    auto tmp = std::vector<MyCoro>{};
    while (!vec.empty()) {
        for (auto &coro : vec) {
            if (!coro.done()) {
                coro.resume();
                tmp.push_back(coro);
            }
        }
        vec.swap(tmp);
        tmp.clear();
    }
}