#include <iostream>
#include <ranges>

void test_istream_view(std::istream &is) {
    // Input until 0
    // Note that there might be some bug when used with std::views::take
    for (auto x : std::ranges::istream_view<int>(is) |
                      std::views::take_while([](int x) { return x != 0; })) {
        std::cout << x << std::endl;
    }
}

void test_all_view(std::span<const int> view) {
    for (auto x : view | std::views::all) {
        std::cout << x << std::endl;
    }
}

template <typename _Iter>
void an_annoying_same_iter_API(_Iter begin, _Iter end) {
    for (auto it = begin; it != end; ++it) {
        std::cout << *it << std::endl;
    }
}

void test_common_view(size_t count) {
    auto view = std::views::iota(0ull);
    // an_annoying_same_iter_API(view.begin(), view.end());
    auto common_view = view | std::views::common;
    an_annoying_same_iter_API(common_view.begin(), common_view.end());
}

void test_counted_view(std::span<const int> view) {
    for (auto x : std::views::counted(view.begin(), 3)) {
        std::cout << x << std::endl;
    }
}

void test_drop_take_reverse_view(std::span<const int> view) {
    for (auto x : view | std::views::drop(2) | std::views::take(2)) {
        std::cout << x << std::endl;
    }
    std::cout.put('\n');
    for (auto x :
         view | std::views::reverse | std::views::drop(2) | std::views::take(2)) {
        std::cout << x << std::endl;
    }
    std::cout.put('\n');
    for (auto x : view | std::views::drop_while([](int x) { return x <= 2; }) |
                      std::views::take_while([](int x) { return x != 4; })) {
        std::cout << x << std::endl;
    }
}

#include <map>

void test_element_view(const std::map<size_t, std::string> &map) {
    for (auto &value : map | std::views::values) {
        std::cout << value << std::endl;
    }
    for (auto &key : map | std::views::keys) {
        std::cout << key << std::endl;
    }
    for (auto &key : map | std::views::elements<0>) {
        std::cout << key << std::endl;
    }
}

void test_filter_transform_view(std::span<const int> view) {
    for (auto x : view | std::views::filter([](int x) { return x % 2 == 0; }) |
                      std::views::transform([](int x) { return x * x; })) {
        std::cout << x << std::endl;
    }
}

#include <vector>

signed main() {
    // test_istream_view(std::cin);
    // test_all_view(std::vector{1, 2, 3, 4, 5});
    // test_common_view(10);
    // test_counted_view(std::vector{1, 2, 3, 4, 5});
    // test_drop_take_reverse_view(std::vector {1, 2, 3, 4, 5});
    // test_element_view({{1, "one"}, {2, "two"}, {3, "three"}});
    test_filter_transform_view(std::vector{1, 2, 3, 4, 5});
    return 0;
}
