#include "common.h" // IWYU pragma: keep

std::mutex lock;
std::condition_variable cv;

std::size_t count;
const std::size_t N = 9;

void consumer() {
    while (true) {
        std::unique_lock lock(::lock);
        while (count == 0)
            cv.wait(lock);
        --count;
        std::cout << std::format("Consumer: {} -> {}\n", count + 1, count);
        cv.notify_all();
    }
}

void producer() {
    while (true) {
        std::unique_lock lock(::lock);
        while (count == N)
            cv.wait(lock);
        ++count;
        std::cout << std::format("Producer: {} -> {}\n", count - 1, count);
        cv.notify_all();
    }
}

signed main(int, char **) {
    int m;
    std::cin >> m;
    std::vector<std::thread> threads;
    threads.reserve(2 * m);
    for (int i = 0; i < m; ++i) {
        threads.push_back(std::thread(consumer));
        threads.push_back(std::thread(producer));
    }
    for (auto &thread : threads)
        thread.join();
    return 0;
}
