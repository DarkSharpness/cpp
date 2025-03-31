#include "utils/error.h"
#include <array>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace dark::__detail::intset {

struct normal_node;

struct base_node {
public:
    enum class Type : bool {
        Leaf,   // Not other data
        Normal, // Normal node
    };
    auto start() const -> std::uint64_t {
        return prefix << bit;
    }
    auto contains(std::uint64_t v) const -> bool {
        return (v >> bit) == prefix;
    }

    base_node(std::uint64_t o, normal_node *p, int b, Type t) noexcept :
        parent(p), prefix(o), bit(b), type(t) {
        assume(std::bit_width(prefix) + bit <= 64, "Prefix out of range: {}", prefix);
        assume(bit % 4 == 0, "Bit index must be multiple of 4: {}", bit);
    }

    normal_node *parent; // parent node

    const std::uint64_t prefix; // prefix bits
    const int bit;              // bit index
    const Type type;            // node type

    static auto s_destroy(base_node *) noexcept -> void;
    struct deleter {
        auto operator()(base_node *ptr) const -> void {
            s_destroy(ptr);
        }
    };
};

using node_ptr = std::unique_ptr<base_node, base_node::deleter>;

struct leaf_node final : base_node {
public:
    inline static constexpr int kMask = (1 << 12) - 1;
    leaf_node(std::uint64_t v, normal_node *p) : base_node(v, p, 12, Type::Leaf) {}

    auto try_set(std::uint64_t v) -> bool {
        if (auto ref = bitset[v & kMask]) {
            return false;
        } else {
            ++count;
            ref = true;
            return true;
        }
    }

    auto try_unset(std::uint64_t v) -> bool {
        if (auto ref = bitset[v & kMask]) {
            --count;
            ref = false;
            return true;
        } else {
            return false;
        }
    }

    auto test(std::uint64_t v) const -> bool {
        return bitset[v & kMask];
    }

private:
    std::size_t count = 0; // number of children
    std::bitset<1 << 12> bitset{};
};

struct normal_node final : base_node {
public:
    inline static constexpr int kMask = (1 << 4) - 1;
    normal_node(std::uint64_t v, normal_node *p, int b) : base_node(v, p, b, Type::Normal) {
        assume(b > 12 && b % 4 == 0, "Invalid bit index {}", b);
    }

    auto operator[](std::uint64_t i) -> node_ptr & {
        return children[(i >> (bit - 4)) & kMask];
    }

    auto operator[](std::uint64_t i) const -> const node_ptr & {
        return children[(i >> (bit - 4)) & kMask];
    }

private:
    std::array<node_ptr, (1 << 4)> children; // 16 children
};

inline auto base_node::s_destroy(base_node *ptr) noexcept -> void {
    switch (ptr->type) {
        using enum base_node::Type;
        case Leaf:   delete static_cast<leaf_node *>(ptr); break;
        case Normal: delete static_cast<normal_node *>(ptr); break;
        default:     panic("Should never reach here");
    }
}

struct set {
public:
    set() = default;

    struct iterator {
    public:
        iterator() : m_node(nullptr), m_offset(0) {}
        auto operator*() const -> std::uint64_t {
            return m_offset | m_node->start();
        }
        auto has_value() const -> bool {
            return m_node != nullptr;
        }

    private:
        iterator(base_node *node, std::uint64_t v) :
            m_node(static_cast<leaf_node *>(node)), m_offset(v & leaf_node::kMask) {
            assume(node != nullptr && node->type == base_node::Type::Leaf, "Invalid iterator");
        }
        leaf_node *m_node;
        int m_offset;
        friend struct set;
    };

    struct insert_result_t {
        [[no_unique_address]]
        iterator it;
        bool inserted;
    };

    using erase_result_t = bool;

    auto insert(std::uint64_t v) -> insert_result_t {
        return s_try_insert(&m_root, v);
    }

    auto erase(std::uint64_t v) -> erase_result_t {
        return s_try_remove(&m_root, v);
    }

    auto find(std::uint64_t v) const -> iterator {
        return s_try_locate(&m_root, v);
    }

private:

    static auto s_make_leaf(std::uint64_t v, normal_node *p) -> node_ptr {
        auto result = new leaf_node{v >> 12, p};
        result->try_set(v);
        return node_ptr{result};
    }

    static auto s_make_normal(std::uint64_t v, normal_node *p, int b) -> node_ptr {
        return node_ptr{new normal_node{v >> b, p, b}};
    }

    static auto s_split_at(normal_node *parent, node_ptr &child_entry, const std::uint64_t v)
        -> insert_result_t {
        const auto u      = child_entry->start();
        const auto topbit = std::bit_width(v ^ u);
        const auto bitlen = (topbit + kBitLen - 1u) / kBitLen * kBitLen;
        // insert a new node to the parent in replace of the child
        auto old_entry    = std::move(child_entry);
        auto new_entry    = s_make_normal(v, parent, bitlen);
        auto &middle      = static_cast<normal_node &>(*new_entry.get());
        old_entry->parent = &middle;
        middle[u]         = std::move(old_entry);
        child_entry       = std::move(new_entry);
        auto &result      = middle[v];
        result            = s_make_leaf(v, &middle);
        return insert_result_t{{result.get(), v}, true};
    }

    static auto s_try_insert(normal_node *parent, const std::uint64_t v) -> insert_result_t {
        do {
            auto &child_entry = (*parent)[v];
            if (child_entry == nullptr) {
                child_entry = s_make_leaf(v, parent);
                return insert_result_t{{child_entry.get(), v}, true};
            }

            if (!child_entry->contains(v))
                return s_split_at(parent, child_entry, v);

            if (child_entry->type == base_node::Type::Leaf) {
                auto leaf = static_cast<leaf_node &>(*child_entry.get());
                return insert_result_t{{&leaf, v}, leaf.try_set(v)};
            }

            assume(child_entry->type == base_node::Type::Normal, "Invalid node type");
            parent = static_cast<normal_node *>(child_entry.get());
        } while (true);
    }

    static auto s_try_remove(normal_node *parent, const std::uint64_t v) -> erase_result_t {
        do {
            auto &child_entry = (*parent)[v];
            if (child_entry == nullptr)
                return false;

            if (!child_entry->contains(v))
                return false;

            if (child_entry->type == base_node::Type::Leaf)
                return static_cast<leaf_node &>(*child_entry.get()).try_unset(v);

            assume(child_entry->type == base_node::Type::Normal, "Invalid node type");
            parent = static_cast<normal_node *>(child_entry.get());
        } while (true);
    }

    static auto s_try_locate(const normal_node *parent, const std::uint64_t v) -> iterator {
        do {
            auto &child_entry = (*parent)[v];
            if (child_entry == nullptr)
                return {};

            if (!child_entry->contains(v))
                return {};

            if (child_entry->type == base_node::Type::Leaf)
                return static_cast<leaf_node *>(child_entry.get())->test(v)
                         ? iterator{child_entry.get(), v}
                         : iterator{};

            assume(child_entry->type == base_node::Type::Normal, "Invalid node type");
            parent = static_cast<normal_node *>(child_entry.get());
        } while (true);
    }

    inline constexpr static int kBitLen = std::bit_width(normal_node::kMask + 0u);
    normal_node m_root{0, nullptr, 64}; // root node
};

} // namespace dark::__detail::intset

namespace dark {

using int_set = __detail::intset::set;

} // namespace dark
