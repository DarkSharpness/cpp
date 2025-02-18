#include "sets.h"
#include <algorithm>
#include <cstdint>
#include <ios>
#include <iostream>
#include <iterator>
#include <set>
#include <type_traits>
#include <vector>

enum class key_set : std::uint64_t {
    KEY0 = 1 << 0,
    KEY1 = 1 << 1,
    KEY2 = 1 << 2,
    KEY3 = 1 << 3,
};

inline constexpr auto operator~(key_set x) -> key_set {
    return static_cast<key_set>(~static_cast<std::underlying_type_t<key_set>>(x));
}

inline constexpr auto operator&(key_set x, key_set y) -> key_set {
    using U = std::underlying_type_t<key_set>;
    return static_cast<key_set>(static_cast<U>(x) & static_cast<U>(y));
}

template <>
struct set_op_traits<key_set> : std::true_type {};

namespace std {

template <typename _Tp>
inline auto operator&(const std::set<_Tp> &x, const std::set<_Tp> &y) -> std::set<_Tp> {
    std::vector<_Tp> buffer;
    buffer.reserve(std::min(x.size(), y.size()));
    std::ranges::set_intersection(
        x.begin(), x.end(), y.begin(), y.end(), std::back_inserter(buffer)
    );
    return std::set<_Tp>{
        std::make_move_iterator(buffer.begin()), std::make_move_iterator(buffer.end())
    };
}

template <typename _Tp>
inline auto operator^(const std::set<_Tp> &x, const std::set<_Tp> &y) -> std::set<_Tp> {
    std::vector<_Tp> buffer;
    buffer.reserve(x.size());
    std::ranges::set_symmetric_difference(
        x.begin(), x.end(), y.begin(), y.end(), std::back_inserter(buffer)
    );
    return std::set<_Tp>{
        std::make_move_iterator(buffer.begin()), std::make_move_iterator(buffer.end())
    };
}

} // namespace std

auto display_set(const std::set<int> &set) -> void {
    std::cout << "{ ";
    for (auto &elem : set)
        std::cout << elem << ' ';
    std::cout << "}";
    std::cout << std::endl;
}

template <typename _Tp>
struct set_op_traits<std::set<_Tp>> : std::true_type {};

struct custom_set : private std::set<int> {
    using std::set<int>::set;
    using std::set<int>::begin;
    using std::set<int>::end;
    custom_set(std::set<int> set) : std::set<int>{std::move(set)} {}
    auto to_base() const -> const std::set<int> & {
        return *this;
    }
    auto to_base() && -> std::set<int> {
        return std::move(*this);
    }
};

inline auto operator|(const custom_set &x, const custom_set &y) -> custom_set {
    return custom_set{x.to_base() | y.to_base()};
}

inline auto operator/(const custom_set &x, const custom_set &y) -> custom_set {
    return custom_set{x.to_base() / y.to_base()};
}

inline auto operator==(const custom_set &x, const custom_set &y) -> bool {
    return x.to_base() == y.to_base();
}

template <>
struct set_op_traits<custom_set> : std::true_type {};

auto display_set(const custom_set &set) -> void {
    return display_set(set.to_base());
}

auto main() -> int {
    static_assert(
        (key_set::KEY0 + key_set::KEY1) <= (key_set::KEY0 | key_set::KEY3 / key_set::KEY2)
    );

    auto set0 = std::set<int>{1, 2};
    auto set1 = std::set<int>{2, 3};
    display_set(set0 | set1);
    display_set(set0 / set1);
    display_set(set1 / set0);

    auto set2 = custom_set{1, 2};
    auto set3 = custom_set{2, 3};
    display_set(set2 & set3);
    display_set(set2 / set3);
    display_set(set3 / set2);

    std::cout << std::boolalpha;

    std::cout << (set3 <= set3) << std::endl;
    std::cout << (set2 >= set3) << std::endl;
    std::cout << ((set2 & set3) < set3) << std::endl;
    std::cout << ((set2 / set3) > set2) << std::endl;
}
