// a thread-safe map
#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

template <typename _Key, typename _Val>
struct shared_map {
private:
    static_assert(
        std::is_default_constructible_v<_Val>, "Value type must be default constructible"
    );

    struct entry {
        std::once_flag flag;
        _Val value;
        auto get(const std::function<_Val()> &f) -> _Val {
            std::call_once(flag, [&] { value = f(); });
            return value;
        }
    };

public:

    shared_map(std::function<_Val()> constructor) noexcept :
        _M_constructor(std::move(constructor)) {}

    auto get(const _Key &key) -> _Val {
        // Try to find the key in the map
        {
            auto lock = std::shared_lock{_M_mutex};
            auto iter = _M_map.find(key);
            if (iter != _M_map.end()) {
                auto &entry = iter->second;
                lock.unlock();
                // lock free path
                return entry.get(_M_constructor);
            }
        }
        // Try constructing the value
        {
            auto lock   = std::unique_lock{_M_mutex};
            auto &entry = _M_map[key];
            lock.unlock();
            // lock free path
            return entry.get(_M_constructor);
        }
    }

private:
    std::shared_mutex _M_mutex;
    std::unordered_map<_Key, entry> _M_map;
    const std::function<_Val()> _M_constructor;
};

inline std::atomic_size_t _S_counter = 0;

auto main() -> int {
    constexpr auto kThreads = 100;
    shared_map<int, int> map([] { return _S_counter++; });
    // start 100 futures
    std::vector<std::future<std::size_t>> futures;
    futures.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i)
        futures.push_back(std::async(std::launch::async, [&map] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::size_t sum = 0;
            for (int i = 0; i < 100; ++i)
                sum += map.get(i);
            return sum;
        }));
    // wait for all futures to finish
    std::size_t sum = 0;
    for (auto &future : futures)
        sum += future.get();

    std::cout << "Sum:      " << sum << std::endl;
    auto value = _S_counter.load();
    std::cout << "Expected: " << kThreads * value * (value - 1) / 2 << std::endl;
}
