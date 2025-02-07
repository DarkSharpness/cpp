#include "common.h"
#include <random>

constexpr std::size_t kBUFSIZE = 256;

char buffer[kBUFSIZE + 1];
constexpr char *buffer_end        = buffer + kBUFSIZE;
std::atomic<char *> head          = buffer;
std::atomic<char *> tail          = buffer;
std::atomic<size_t> release_times = 0;
std::atomic<size_t> acquire_times = 0;

std::counting_semaphore sem{0};

void producer(char c) {
    std::mt19937 gen(std::random_device{}());
    using namespace std::chrono_literals;
    for (char *pos; (pos = tail.fetch_add(1)) < buffer_end;) {
        *pos = c;
        sem.release();
        release_times.fetch_add(1);

        double ratio  = (gen() - gen.min()) / double(gen.max() - gen.min());
        double factor = std::lerp(0.1, 0.5, ratio);
        std::this_thread::sleep_for(1s * factor);
    }
    std::cout << std::format("Producer \"{}\" is over\n", c);
    sem.release();
    release_times.fetch_add(1);
}

void consumer(char c) {
    std::mt19937 gen(std::random_device{}());
    using namespace std::chrono_literals;
    while (true) {
        sem.acquire();
        char *pos = head.fetch_add(1);

        acquire_times.fetch_add(1);
        if (pos >= buffer_end)
            break;

        std::cout << std::format("[{:2} -> {}]\n", pos - buffer, *pos);

        double ratio  = (gen() - gen.min()) / double(gen.max() - gen.min());
        double factor = std::lerp(0.1, 0.5, ratio);
        std::this_thread::sleep_for(1s * factor);
    }
    std::cout << std::format("Consumer \"{}\" is over\n", c);
}

signed main() {
    int size;
    std::cin >> size;
    std::vector<std::thread> consumer(size);
    std::vector<std::thread> producer(size);

    std::size_t i = 0;
    for (auto &c : consumer) {
        c = std::thread(::consumer, char('a' + i));
        i = (i + 1) % 26;
    }
    i = 0;
    for (auto &p : producer) {
        p = std::thread(::producer, char('a' + i));
        i = (i + 1) % 26;
    }

    for (auto &c : consumer)
        c.join();
    for (auto &p : producer)
        p.join();

    std::cout << std::format("{:-<30}\n", "");
    std::cout << std::format(
        "Release times: {}\n"
        "Acquire times: {}\n",
        release_times.load(), acquire_times.load()
    );

    // std::cout << buffer << std::endl;
    return 0;
}
