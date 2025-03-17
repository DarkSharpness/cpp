#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace details {

// since C++17 don't have std::type_identity, we implement it here
template <typename T>
struct type_identity {
    using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

template <typename Key, typename Value>
struct LockedMap {
private:
    std::unordered_map<Key, Value> map;
    mutable std::shared_mutex mutex;

public:
    template <typename F0, typename F1, typename F2, typename F3>
    auto try_insert(const Key &key, F0 &&exist, F1 &&prepare, F2 &&init, F3 &&exit) {
        {
            auto lock = std::shared_lock(mutex);
            if (auto it = map.find(key); it != map.end()) {
                return std::forward<F0>(exist)(*it);
            }
        }
        auto tmp = std::forward<F1>(prepare)();
        {
            auto lock          = std::unique_lock(mutex);
            auto [it, success] = map.try_emplace(key);
            if (success == false) {
                return std::forward<F0>(exist)(*it);
            }
            auto tmp2 = std::forward<F2>(init)(*it, tmp);
            lock.unlock();
            return std::forward<F3>(exit)(tmp, tmp2);
        }
    }

    auto erase(const Key &key) -> void {
        auto lock = std::unique_lock(mutex);
        map.erase(key);
    }
};

template <typename Value, typename... Keys>
class ThreadSafeCacheImpl {
protected:
    struct LRUnode;
    using KeySet      = std::variant<const Keys *...>;
    using LRUlist     = std::list<LRUnode>;
    using LRUiterator = typename LRUlist::iterator;

    struct Entry {
    public:
        Entry()                         = default;
        Entry(const Value &)            = delete;
        Entry &operator=(const Value &) = delete;

    private:
        Value value;
        LRUiterator iterator;
        friend class ThreadSafeCacheImpl;
    };

    struct LRUnode {
        KeySet key;
        Entry &entry;
        LRUnode(KeySet k, Entry &e) : key{k}, entry{e} {}
    };

protected:
    // do not hold lock when calling any of these functions

    template <typename Key>
    auto get_cache_lock() -> LockedMap<Key, Entry> & {
        static_assert(
            (std::is_same_v<Key, Keys> || ...), "Key type not found in the cache"
        );
        return std::get<LockedMap<Key, Entry>>(storage);
    }

    template <typename Key>
    auto lru_visit(const std::pair<const Key, Entry> &pair) -> const Value & {
        static_assert(
            (std::is_same_v<Key, Keys> || ...), "Key type not found in the cache"
        );
        const auto guard = std::lock_guard{lru_mutex};
        /** \attention The dereference of the iterator must be done within the lock */
        const auto &entry   = pair.second;
        const auto iterator = entry.iterator;
        if (iterator != LRUiterator{}) { // if not to be evicted
            lru_list.splice(lru_list.end(), lru_list, iterator);
        }
        return entry.value;
    }

    template <typename Key>
    auto lru_init(std::pair<const Key, Entry> &pair, const Value &init) -> void {
        static_assert(
            (std::is_same_v<Key, Keys> || ...), "Key type not found in the cache"
        );
        const auto guard = std::lock_guard{lru_mutex};
        /** \attention This must be done in the lock to ensure the iterator is valid */
        auto &[key, entry] = pair;
        entry.value        = init;
        entry.iterator     = lru_list.emplace(lru_list.end(), KeySet{&key}, entry);
    }

    template <typename Predicate, typename Evict>
    auto lru_evict(const Predicate &predicate, const Evict &evict) -> void {
        if (!predicate()) // short circuit if the predicate is false
            return;

        // only hold the lru lock when doing the eviction
        auto free_list = [&]() -> LRUlist {
            const auto guard = std::lock_guard{lru_mutex};
            auto iter        = lru_list.begin();
            if (iter == lru_list.end())
                return {};
            auto evicted = LRUlist{};
            auto waiting = LRUlist{};
            do {
                auto old_iter    = iter++;
                auto &[_, entry] = *old_iter;
                if (!evict(entry.value)) {
                    waiting.splice(waiting.end(), lru_list, old_iter);
                    continue;
                }
                entry.iterator = LRUiterator{}; // to be evicted
                evicted.splice(evicted.end(), lru_list, old_iter);
            } while (predicate() && iter != lru_list.end());
            // move the waiting list to the end of the lru list
            lru_list.splice(lru_list.end(), waiting);
            return evicted;
        }();

        // free the memory of the map outside the lock
        for (auto &[keyset, _] : free_list) {
            std::visit(
                [&](auto *key) {
                    using Key_t      = std::decay_t<decltype(*key)>;
                    auto &locked_map = get_cache_lock<Key_t>();
                    locked_map.erase(*key);
                },
                keyset
            );
        }
    }

private:
    std::tuple<LockedMap<Keys, Entry>...> storage;
    LRUlist lru_list;
    std::mutex lru_mutex;
};

template <typename Value>
struct SizedValue {
    Value value;
    std::size_t size;
    template <typename Fn>
    SizedValue(Value value, const Fn &size_fn) : value{value}, size{size_fn(value)} {}
};

template <typename Value, typename... Keys>
using ThreadSafeCacheSized =
    ThreadSafeCacheImpl<std::shared_future<SizedValue<Value>>, Keys...>;

} // namespace details

template <typename Policy, typename Value, typename... Keys>
class ThreadSafeCacheSized2 : private Policy,
                              details::ThreadSafeCacheSized<Value, Keys...> {
private:
    using Impl   = details::ThreadSafeCacheSized<Value, Keys...>;
    using Sized  = details::SizedValue<Value>;
    using Future = std::shared_future<Sized>;

public:
    using Policy::Policy;

    template <typename Key>
    auto Get(const details::type_identity_t<Key> &key) -> Value {
        // We use type_identity_t to force the user to provide the key type
        static_assert(
            (std::is_same_v<Key, Keys> || ...), "Key type not found in the cache"
        );
        auto future = GetFuture<Key>(key);
        Pop();
        return future.get().value;
    }

private:
    template <typename Key>
    auto GetFuture(const Key &key) -> Future {
        using Task       = std::packaged_task<Sized()>;
        auto &locked_map = Impl::template get_cache_lock<Key>();
        const auto exist = [this](const auto &pair) {
            return Impl::lru_visit(pair); // simply move the entry to the end
        };
        const auto prepare = [&] {
            return Task{[this, &key] {
                return Sized{
                    Policy::template compute<Key>(key), // compute the value
                    [&](const Value &value) {
                        auto size = Policy::size(value);
                        current_size_ += size;
                        return size;
                    }
                };
            }};
        };
        const auto init = [this](auto &pair, Task &task) {
            auto future = task.get_future().share();
            Impl::lru_init(pair, future);
            return future;
        };
        const auto exit = [](Task &task, Future future) {
            task(); // only perform task on first access
            return future;
        };

        return locked_map.try_insert(key, exist, prepare, init, exit);
    }

    auto Pop() -> void {
        Impl::lru_evict(
            [&] { return Policy::should_evict(current_size_); },
            [&](const Future &value) {
                using namespace std::chrono_literals;
                // if not ready, then do not wait and block here
                if (value.wait_for(0s) != std::future_status::ready)
                    return false;
                auto size = value.get().size;
                current_size_ -= size;
                return true;
            }
        );
    }

private:
    std::atomic_size_t current_size_{0};
};

struct Policy {
    auto should_evict(std::size_t size) const -> bool {
        return size > 2;
    }
    auto size(const double &) const -> std::size_t {
        return 1;
    }
    template <typename Key>
    auto compute(const Key &k) const -> double {
        std::cout << "Computing: " << k << '\n';
        return k * sizeof(Key);
    }
};

auto test_0() -> void {
    ThreadSafeCacheSized2<Policy, double, int, long long> cache;
    std::cout << cache.Get<int>(1) << '\n';
    std::cout << cache.Get<long long>(2) << '\n';
    std::cout << cache.Get<long long>(1) << '\n';
    std::cout << cache.Get<long long>(2) << '\n';
    std::cout << cache.Get<int>(1) << '\n';
    std::cout << cache.Get<int>(2) << '\n';
}

std::atomic_size_t counter{0};

struct EvictPolicy {
    std::size_t max_size;
    std::size_t sleep_time;
    EvictPolicy(std::size_t max_size = std::size_t(-1), std::size_t sleep_time = 0) :
        max_size{max_size}, sleep_time{sleep_time} {}
    auto should_evict(std::size_t) const -> bool {
        return counter > max_size;
    }
    auto size(const double &) const -> std::size_t {
        return 1;
    }
    template <typename Key>
    auto compute(const Key &) -> std::size_t {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        return counter++;
    }
};

auto test_1() -> void {
    counter = 0;
    ThreadSafeCacheSized2<EvictPolicy, std::size_t, long long> cache{};
    constexpr std::size_t kThreads = 256;

    // start 100 futures
    std::vector<std::future<std::size_t>> futures;
    futures.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i)
        futures.push_back(std::async(std::launch::async, [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::size_t sum = 0;
            for (int i = 0; i < 100; ++i)
                sum += cache.Get<long long>(i);
            return sum;
        }));

    // wait for all futures to finish
    std::size_t sum = 0;
    for (auto &future : futures)
        sum += future.get();

    std::cout << "Sum:      " << sum << std::endl;
    auto value = counter.load();
    std::cout << "Expected: " << kThreads * value * (value - 1) / 2 << std::endl;
}

auto test_2() -> void {
    counter = 0;
    ThreadSafeCacheSized2<EvictPolicy, std::size_t, int, long long> cache{1, 100};
    constexpr std::size_t kThreads = 256;
    // start 100 futures
    std::vector<std::future<std::size_t>> futures;
    futures.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i)
        futures.push_back(std::async(std::launch::async, [&, j = i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::size_t sum = 0;
            if (j < kThreads / 2)
                for (int i = 0; i < 20; ++i)
                    sum += cache.Get<int>(i + j * kThreads);
            else
                for (int i = 0; i < 20; ++i)
                    sum += cache.Get<long long>(i + j * kThreads);
            return sum;
        }));
    // wait for all futures to finish
    for (auto &future : futures)
        future.get();

    std::cout << "Insert count: " << counter.load() << std::endl;
    std::cout << "Expected:     " << kThreads * 20 << std::endl;
}

auto test_3() -> void {
    counter = 0;
    ThreadSafeCacheSized2<EvictPolicy, std::size_t, int, long long> cache{20, 20};
    constexpr std::size_t kThreads = 256;
    // start 100 futures
    std::vector<std::future<std::size_t>> futures;
    futures.reserve(kThreads);
    for (std::size_t i = 0; i < kThreads; ++i)
        futures.push_back(std::async(std::launch::async, [&, j = i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::size_t sum = 0;
            if (j < kThreads / 2)
                for (int i = 0; i < 100; ++i)
                    sum += cache.Get<int>(i);
            else
                for (int i = 0; i < 100; ++i)
                    sum += cache.Get<long long>(i);
            return sum;
        }));
    // wait for all futures to finish
    for (auto &future : futures)
        future.get();

    std::cout << "Insert count: " << counter.load() << std::endl;
    std::cout << "Expected:     " << kThreads * 20 << std::endl;
}

template <auto F>
auto benchmark() -> void {
    auto tic = std::chrono::steady_clock::now();
    F();
    auto toc = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic);
    std::cout << "Duration: " << dur.count() << "ms\n";
}

auto main() -> int {
    benchmark<test_0>();
    benchmark<test_1>();
    benchmark<test_2>();
    benchmark<test_3>();
    return 0;
}
