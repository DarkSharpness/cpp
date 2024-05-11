#include <iostream>
#include <coroutine>
#include <format>

struct promise;

struct coroutine : std::coroutine_handle <promise> {
    using promise_type = ::promise;
};

struct promise {
    coroutine get_return_object() {
        return { coroutine::from_promise(*this) };
    }
    std::suspend_always initial_suspend() noexcept {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }
    // void return_void() {}
    auto yield_value(auto x) { return x; }
    void return_value(int x) {}
    void unhandled_exception() {}
};

struct X {
    ~X() { std::puts("~X()"); }
};

coroutine f() {
    std::puts("f() start");
    X x;
    co_yield std::suspend_always{};
    co_yield std::suspend_never {};
    co_return 1;
}

signed main() {
    std::cout << std::boolalpha;

    coroutine c = f();

    std::puts("main() start");

    std::cout << std::format("c.done() = {}\n", c.done());

    c.resume();
    c.resume();
    // c.resume();

    std::cout << std::format("c.done() = {}\n", c.done());

    return 0;
}