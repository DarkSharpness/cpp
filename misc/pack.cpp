template <typename... Args>
constexpr auto func(Args &&...args) -> auto {
    return args...[0];
}

auto main() -> int {
    constexpr auto x = func(1, 2, 3);
    static_assert(x == 1);
}
