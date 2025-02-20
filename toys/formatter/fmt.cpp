#include "fmt.h"
#include <array>
#include <format>
#include <forward_list>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <utility>

auto main() -> int {
    std::cout << std::format(
        "pair: {}\n", std::pair<int, std::pair<long, std::string>>{1, {2, "three"}}
    );
    std::cout << std::format(
        "tuple: {}\n", std::tuple<int, long, std::string, std::tuple<>>{1, 2, "three", {}}
    );
    std::cout << std::format(
        "optional: {}, {}\n", std::optional<int>{666}, std::optional<std::string>{}
    );
    std::cout << std::format(
        "containers:\n"
        "  vector: {}\n"
        "  map   : {}\n",
        std::vector{1, 2, 4, 8, 16, 32, 64},
        std::map<int, std::string>{{1, "one"}, {2, "two"}, {3, "three"}}
    );
    const std::array<int, 5> arr[2] = {{1, 2, 3, 4, 5}, {6, 7, 8, 9}};
    std::cout << std::format(
        "nested container: {}\n {}\n",
        std::vector<std::forward_list<int>>{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, arr
    );
    struct MyTest {
        std::array<std::string, 2> name = {"dark", "sharpness"};
        std::size_t age;
    };
    // support aggregate type
    std::cout << std::format("struct: {}\n", MyTest{.age = 1});
    std::cout << std::format("bitset: {}\n", std::bitset<8>{0b10001110});
    using namespace std::string_literals;
}
