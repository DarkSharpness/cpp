#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace dark {

template <std::integral _Int>
struct integer_range {
    struct iterator {
    public:
        iterator() = default;
        explicit iterator(_Int value) : m_value(value) {}
        auto operator*() const -> _Int {
            return m_value;
        }
        auto operator++() -> iterator & {
            ++m_value;
            return *this;
        }
        auto operator++(int) -> iterator {
            auto copy = *this;
            ++m_value;
            return copy;
        }
        auto operator--() -> iterator & {
            --m_value;
            return *this;
        }
        auto operator--(int) -> iterator {
            auto copy = *this;
            --m_value;
            return copy;
        }

        auto operator==(const iterator &other) const -> bool = default;

    private:
        _Int m_value;
    };

    integer_range(_Int begin, _Int end) : m_begin(begin), m_end(end) {}

    auto begin() const -> iterator {
        return iterator{m_begin};
    }

    auto end() const -> iterator {
        return iterator{m_end};
    }

private:
    _Int m_begin;
    _Int m_end;
};

template <std::integral _Int = std::size_t>
auto irange(std::type_identity_t<_Int> to) -> integer_range<_Int> {
    return integer_range<_Int>{0, to};
}

template <std::integral _Int = std::size_t>
auto irange(std::type_identity_t<_Int> from, std::type_identity_t<_Int> to) -> integer_range<_Int> {
    return integer_range<_Int>{from, to};
}

} // namespace dark
