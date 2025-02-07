#include <iostream>
#include <print>

signed main() {
    std::print(std::cout, "Hello, {}!\n", "World"); // "Hello, World!\n
    std::print("Hello World {}\n", 114514);
    return 0;
}
