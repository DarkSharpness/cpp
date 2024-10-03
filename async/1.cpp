#include <future>
#include <iostream>

auto main() -> int {
    using namespace std::chrono_literals;
    auto future = std::async([] {
        std::this_thread::sleep_for(1s);
        std::cout << "Hello, World!" << std::endl;
        return 100;
    });
    auto result = future.share();
    auto status = result.wait_for(0.5s);
    if (status == std::future_status::ready) {
        std::cout << "The future is ready!" << std::endl;
    } else {
        std::cout << "The future is not ready!" << std::endl;
    }
    std::cout << "The result is " << result.get() << std::endl;
    return 0;
}
