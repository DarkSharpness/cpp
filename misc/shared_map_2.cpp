#include <cstddef>
#include <format>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace dark {

template <typename T>
struct type_identity {
    using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

template <typename Key, typename Value>
struct base_map {
    std::unordered_map<Key, Value> map_;
    std::shared_mutex mutex_;
};

template <typename Value, typename... Keys>
class cache_impl {
private:
    struct LRUnode;
    using KeySet      = std::variant<const Keys *...>;
    using LRUlist     = std::list<LRUnode>;
    using LRUiterator = typename LRUlist::iterator;

    struct Entry {
        std::shared_future<Value> future;
        LRUiterator iterator;
    };
    struct LRUnode {
        KeySet key;
        const Entry &entry;
        LRUnode(KeySet k, const Entry &e) : key{k}, entry{e} {}
    };

    template <typename Key>
    using EntryMap = base_map<Key, Entry>;

protected:
    // do not hold lock when calling any of these functions

    template <typename Key>
    auto get_cache() -> EntryMap<Key> & {
        return std::get<EntryMap<Key>>(storage);
    }

    auto lru_visit(LRUiterator it) -> void {
        const auto guard = std::lock_guard{lru_mutex};
        lru_list.splice(lru_list.begin(), lru_list, it);
    }

    template <typename Pair>
    auto lru_emplace(const Pair &pair) -> LRUiterator {
        const auto guard   = std::lock_guard{lru_mutex};
        auto &[key, value] = pair;
        return lru_list.emplace(lru_list.begin(), &key, value);
    }

    template <typename Predicate, typename Policy>
    auto lru_evict(const Predicate &pred, const Policy &policy) -> void {
        if (!pred()) // short circuit if the predicate is false
            return;
        auto lock = std::unique_lock{lru_mutex};
        do {
            auto [keyset, value] = lru_list.back();
            lru_list.pop_back();
            std::visit(
                [&](auto key) {
                    using Key_t        = std::decay_t<decltype(*key)>;
                    auto &[map, mutex] = get_cache<Key_t>();
                    const auto guard   = std::lock_guard{mutex};
                    map.erase(*key);
                },
                keyset
            );
            policy(value);
        } while (pred());
    }

private:
    std::tuple<EntryMap<Keys>...> storage;
    LRUlist lru_list;
    std::mutex lru_mutex;
};

template <typename Value>
struct sized_value {
    Value value;
    std::size_t size;
    template <typename Fn>
    sized_value(Value value, const Fn &size_fn) : value{value}, size{size_fn(value)} {}
};

template <typename Policy, typename Value, typename... Keys>
class multicache : private Policy, cache_impl<sized_value<Value>, Keys...> {
private:
    using Impl = cache_impl<sized_value<Value>, Keys...>;

public:
    using Policy::Policy;

    template <typename Key>
    auto Get(const type_identity_t<Key> &key) -> Value {
        auto &[map, mutex] = Impl::template get_cache<Key>();
        {
            auto lock = std::shared_lock{mutex};
            if (auto it = map.find(key); it != map.end()) {
                // copy a new future and iterator
                auto [future, iterator] = it->second;
                lock.unlock();
                Impl::lru_visit(iterator);
                return future.get().value;
            }
        }

        auto task   = std::packaged_task<sized_value<Value>()>{[&] {
            return sized_value<Value>{
                Policy::template compute<Key>(key),
                [&](const Value &value) { return Policy::size(value); }
            };
        }};
        auto future = task.get_future().share();

        {
            auto lock            = std::unique_lock{mutex};
            auto [iter, success] = map.try_emplace(key);
            if (!success) { // some other thread has inserted the key
                auto [future, iterator] = iter->second;
                lock.unlock();
                Impl::lru_visit(iterator);
                return future.get().value;
            }
            // insert a new future and iterator
            iter->second.future   = future;
            iter->second.iterator = Impl::lru_emplace(*iter);
        }

        task();
        return future.get().value;
    }
};

} // namespace dark

struct Policy {
    template <typename T>
    auto compute(T key) -> double {
        if constexpr (std::is_same_v<T, int>) {
            std::cout << std::format("compute int {}\n", key);
            return key * 2;
        } else if constexpr (std::is_same_v<T, float>) {
            std::cout << std::format("compute float {}\n", key);
            return key / 2;
        } else {
            static_assert(false);
            return 0;
        }
    }
    auto size(int value) -> std::size_t {
        return sizeof(value);
    }
};

auto main() -> int {
    dark::multicache<Policy, double, int, float> cache;
    std::cout << std::format("{}\n", cache.Get<int>(1));
    std::cout << std::format("{}\n", cache.Get<float>(1.0));
    std::cout << std::format("{}\n", cache.Get<int>(1));
    std::cout << std::format("{}\n", sizeof(cache));
}
