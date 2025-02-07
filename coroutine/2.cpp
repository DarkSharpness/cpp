// Modified from https://zhuanlan.zhihu.com/p/497224333
#include <any>
#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>

inline namespace Coroutine {

struct suspend_always {
    auto await_ready() const noexcept -> bool {
        std::cout << "suspend_always::await_ready\n";
        return false;
    }

    auto await_suspend(std::coroutine_handle<>) const noexcept -> void {
        std::cout << "suspend_always::await_suspend\n";
    }

    auto await_resume() const noexcept -> void {
        std::cout << "suspend_always::await_resume\n";
    }
};

struct suspend_never {
    auto await_ready() const noexcept -> bool {
        std::cout << "suspend_never::await_ready\n";
        return true;
    }

    auto await_suspend(std::coroutine_handle<>) const noexcept -> void {
        std::cout << "suspend_never::await_suspend\n";
    }

    auto await_resume() const noexcept -> void {
        std::cout << "suspend_never::await_resume\n";
    }
};

struct promise_type;

struct task {
public:
    using promise_type = ::Coroutine::promise_type;

    task() noexcept = default;
    task(std::coroutine_handle<task::promise_type> handle) noexcept : _M_handle(handle) {}

    auto &get_handle() noexcept {
        return _M_handle;
    }

private:
    std::coroutine_handle<task::promise_type> _M_handle;
};

struct promise_type {
    promise_type() noexcept {
        std::cout << "1.create promise object\n";
    }
    auto get_return_object() noexcept -> task {
        std::cout
            << "2.create coroutine return object, and the coroutine is created now\n";
        return {std::coroutine_handle<task::promise_type>::from_promise(*this)};
    }
    auto initial_suspend() noexcept -> suspend_never {
        std::cout << "3.do you want to susupend the current coroutine?\n";
        std::cout << "4.don't suspend because return std::suspend_never, so continue to "
                     "execute coroutine body\n";
        return {};
    }
    auto final_suspend() noexcept -> std::suspend_never {
        std::cout << "13.coroutine body finished, do you want to suspend the current "
                     "coroutine?\n";
        std::cout
            << "14.don't suspend because return std::suspend_never, and the continue "
               "will be automatically destroyed, bye\n";
        return {};
    }
    auto return_value(std::any) noexcept -> void {
        std::cout << "12.coroutine return value, so return_value is called\n";
    }
    auto unhandled_exception() noexcept -> void {
        std::cout << "12.an unhandled exception is thrown\n";
    }
    ~promise_type() noexcept {
        std::cout << "15.promise object destroyed\n";
    }
};

std::thread t;

struct awaiter {
    task::promise_type *promise_ref{};
    auto await_ready() noexcept -> bool {
        std::cout << "6.do you want to suspend current coroutine?\n";
        std::cout << "7.yes, suspend becase awaiter.await_ready() return false\n";
        return false;
    }
    auto await_suspend(std::coroutine_handle<task::promise_type> handle) noexcept
        -> bool {
        promise_ref = &handle.promise();
        std::cout << "8.execute awaiter.await_suspend()\n";
        // In another thread, carrying the handle to resume the coroutine
        t = std::thread([handle]() {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            handle.resume();
        });
        std::cout << "9.a new thread lauched, and will return back to caller\n";
        return true; // Equal to return void
    }
    [[nodiscard]]
    auto await_resume() noexcept -> int {
        if (promise_ref == nullptr) {
            std::cout << "wrong resume called\n";
            std::terminate();
        }
        return 123456;
    }
};

struct Awaitable {};

[[maybe_unused]]
static auto operator co_await(Awaitable) noexcept -> awaiter {
    return {};
}

static auto test() -> task {
    std::cout << "5.begin to execute coroutine body, the thread id="
              << std::this_thread::get_id() << "\n";
    std::cout << co_await Awaitable{}
              << '\n'; // if await_ready return false, the coroutine will be suspended.
    std::cout
        << "11.coroutine resumed, continue execute coroutine body now, the thread id="
        << std::this_thread::get_id() << "\n";
    co_return 0;
    // throw std::runtime_error("test");
}

} // namespace Coroutine

auto main() -> int {
    [[maybe_unused]]
    auto c = test();
    std::cout << "10.come back to caller becuase of co_await awaiter\n";
    if (t.joinable()) {
        t.join();
    }
    return 0;
}
