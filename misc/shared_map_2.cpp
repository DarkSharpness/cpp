#include <cstddef>
#include <format>
#include <future>
#include <iostream>
#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <variant>

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
    std::unordered_map<Key, Value> map;
    std::shared_mutex mutex;
};

template <typename Value, typename... Keys>
class ThreadSafeCacheImpl {
private:
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
        return std::get<LockedMap<Key, Entry>>(storage);
    }

    auto lru_visit(const Entry &entry) -> const Value & {
        const auto guard = std::lock_guard{lru_mutex};
        /** \attention The dereference of the iterator must be done within the lock */
        const auto iterator = entry.iterator;
        if (iterator != LRUiterator{})
            lru_list.splice(lru_list.end(), lru_list, iterator);
        return entry.value;
    }

    template <typename Key>
    auto lru_init(std::pair<const Key, Entry> &pair, const Value &init) -> void {
        const auto guard   = std::lock_guard{lru_mutex};
        auto &[key, entry] = pair;
        /** \attention This must be done in the lock to ensure the iterator is valid */
        entry.value    = init;
        entry.iterator = lru_list.emplace(lru_list.end(), KeySet{&key}, entry);
    }

    template <typename Predicate, typename Evict>
    auto lru_evict(const Predicate &predicate, const Evict &evict) -> void {
        if (!predicate()) // short circuit if the predicate is false
            return;

        // only hold the lru lock when doing the eviction
        auto free_list = [&]() -> LRUlist {
            const auto guard = std::lock_guard{lru_mutex};
            auto iter        = lru_list.begin();
            do {
                auto &[_, entry] = *(iter++);
                entry.iterator   = LRUiterator{};
                evict(entry.value);
            } while (predicate());
            auto result = LRUlist{};
            result.splice(result.begin(), lru_list, lru_list.begin(), iter);
            return result;
        }();

        // free the memory of the map outside the lock
        for (auto &[keyset, _] : free_list) {
            std::visit(
                [&](auto *key) {
                    using Key_t        = std::decay_t<decltype(*key)>;
                    auto &[map, mutex] = get_cache_lock<Key_t>();
                    const auto guard   = std::lock_guard{mutex};
                    map.erase(*key);
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
class ThreadSafeCacheSized2 : Policy, details::ThreadSafeCacheSized<Value, Keys...> {
private:
    using Impl   = details::ThreadSafeCacheSized<Value, Keys...>;
    using Sized  = details::SizedValue<Value>;
    using Future = std::shared_future<Sized>;

public:
    using Policy::Policy;

    template <typename Key>
    auto Get(const details::type_identity_t<Key> &key) -> Value {
        auto future = GetFuture<Key>(key);
        Pop();
        return future.get().value;
    }

private:
    template <typename Key>
    auto GetFuture(const details::type_identity_t<Key> &key) -> Future {
        auto &[map, mutex] = Impl::template get_cache_lock<Key>();

        {
            auto lock = std::shared_lock{mutex};
            if (auto it = map.find(key); it != map.end()) {
                /** \attention We must hold the reference here */
                Impl::lru_visit(it->second);
            }
        }

        auto task   = std::packaged_task<Sized()>{[this, &key] {
            return Sized{
                Policy::template compute<Key>(key), // compute the value
                [&](const Value &value) {
                    auto size = Policy::size(value);
                    current_size_ += size;
                    return size;
                }
            };
        }};
        auto future = task.get_future().share();

        {
            auto lock          = std::unique_lock{mutex};
            auto [it, success] = map.try_emplace(key);
            if (!success) // some other thread has inserted the key
                return Impl::lru_visit(it->second);
            Impl::lru_init(*it, future);
        }

        task();
        return future;
    }

    auto Pop() -> void {
        Impl::lru_evict(
            [this] { return Policy::should_evict(current_size_); },
            [this](const Future &value) {
                std::cout << std::format("evict {}\n", value.get().value);
                current_size_ -= value.get().size;
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
        std::cout << std::format("compute {}\n", k);
        return k * sizeof(Key);
    }
};

auto main() -> int {
    ThreadSafeCacheSized2<Policy, double, int, long long> cache;
    std::cout << cache.Get<int>(1) << '\n';
    std::cout << cache.Get<long long>(2) << '\n';
    std::cout << cache.Get<long long>(1) << '\n';
    std::cout << cache.Get<long long>(2) << '\n';
    std::cout << cache.Get<int>(1) << '\n';
    return 0;
}
