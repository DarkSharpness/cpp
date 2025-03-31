#include "utils/error.h"
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
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
    auto start() const -> std::size_t {
        return prefix << bit;
    }
    auto contains(std::size_t v) const -> bool {
        return (v >> bit) == prefix;
    }
    auto _debug_bit() const -> int {
        return bit;
    }

    base_node(std::size_t o, normal_node *p, int b, Type t) noexcept :
        parent(p), prefix(o), bit(b), type(t) {
        assume(std::bit_width(prefix) + bit <= 64, "Prefix out of range: {}", prefix);
        assume(bit % 4 == 0, "Bit index must be multiple of 4: {}", bit);
    }

    normal_node *parent; // parent node

    const std::size_t prefix; // prefix bits
    const int bit;            // bit index
    const Type type;          // node type

    static auto s_destroy(base_node *) noexcept -> void;
    struct deleter {
        auto operator()(base_node *ptr) const -> void {
            s_destroy(ptr);
        }
    };
};

using node_ptr = std::unique_ptr<base_node, base_node::deleter>;

struct leaf_node final : base_node {
    leaf_node(std::size_t v, normal_node *p) : base_node(v, p, 0, Type::Leaf) {}
};

struct normal_node final : base_node {
public:
    normal_node(std::size_t v, normal_node *p, int b) : base_node(v, p, b, Type::Normal) {
        assume(b > 0 && b % 4 == 0, "Invalid bit index {}", b);
    }

    auto operator[](std::size_t i) -> node_ptr & {
        return children[(i >> (bit - 4)) & kMask];
    }

    auto operator[](std::size_t i) const -> const node_ptr & {
        return children[(i >> (bit - 4)) & kMask];
    }

    inline static constexpr int kMask = (1 << 4) - 1;

    const auto &_debug(const base_node *parent, bool verbose = true) const {
        if (!verbose)
            return children;
        call_in_debug_mode([&] {
            assume(parent == this->parent, "Parent mismatch");
            debugger() << "Normal: Prefix=" << prefix << " Bit=" << bit << "\n";
        });
        return children;
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

using _Int = std::size_t;

struct set {
public:
    set() = default;

    struct iterator {
    public:
        iterator() = default;
        auto operator*() const -> _Int {
            return m_node->start();
        }
        auto has_value() const -> bool {
            return m_node != nullptr;
        }

    private:
        iterator(base_node *node) : m_node(node) {}
        base_node *m_node;
        friend struct set;
    };

    struct insert_result_t {
        iterator it;
        bool inserted;
    };

    using erase_result_t = bool;

    auto insert(_Int v) -> insert_result_t {
        return s_try_insert(&m_root, v);
    }

    auto erase(_Int v) -> erase_result_t {
        auto result = s_try_locate(&m_root, v);
        if (result == nullptr)
            return false;
        const_cast<node_ptr &>(*result) = nullptr;
        return true;
    }

    auto find(_Int v) const -> iterator {
        auto result = s_try_locate(&m_root, v);
        return iterator(result ? result->get() : nullptr);
    }

    auto _debug() const -> void {
        call_in_debug_mode([this] {
            s_debug_print(&m_root);
            debugger() << "End of debug\n";
        });
    }

    auto _debug_check() const -> void {
        call_in_debug_mode([this] {
            auto dfs = [](auto &&dfs, const base_node *node, std::size_t depth) -> void {
                assume(depth < kLevel, "Depth out of range");
                const auto bit = node->_debug_bit();
                assume(bit % 4 == 0, "Bit index must be multiple of 4");
                if (node->type == base_node::Type::Normal) {
                    auto *const n = static_cast<const normal_node *>(node);
                    for (auto &child : n->_debug(node, false)) {
                        if (child == nullptr)
                            continue;
                        assume(child->parent == node, "Parent mismatch");
                        assume(
                            child->_debug_bit() < bit, "Invalid bit index {} >= {}",
                            child->_debug_bit(), bit
                        );
                        dfs(dfs, child.get(), depth + 1);
                    }
                }
            };
            dfs(dfs, &m_root, 0);
        });
    }

private:
    static auto s_debug_print(const base_node *const node) -> void {
        if (!node)
            return;
        if (node->type == base_node::Type::Leaf) {
            auto *const n = static_cast<const leaf_node *>(node);
            debugger() << "Leaf: " << n->start() << "\n";
        } else {
            auto *const n = static_cast<const normal_node *>(node);
            for (auto &child : n->_debug(node)) {
                if (child == nullptr)
                    continue;
                s_debug_print(child.get());
            }
        }
    }

    static auto s_make_leaf(std::size_t v, normal_node *p) -> node_ptr {
        return node_ptr{new leaf_node{v, p}};
    }

    static auto s_make_normal(std::size_t v, normal_node *p, int b) -> node_ptr {
        return node_ptr{new normal_node{v >> b, p, b}};
    }

    static auto s_split_at(normal_node *parent, node_ptr &child_entry, const std::size_t v)
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
        return insert_result_t{result.get(), true};
    }

    static auto s_try_insert(normal_node *parent, const std::size_t v) -> insert_result_t {
        do {
            auto &child_entry = (*parent)[v];
            if (child_entry == nullptr) {
                child_entry = s_make_leaf(v, parent);
                return insert_result_t{child_entry.get(), true};
            }

            if (!child_entry->contains(v))
                return s_split_at(parent, child_entry, v);

            if (child_entry->type == base_node::Type::Leaf)
                return insert_result_t{child_entry.get(), false};

            parent = static_cast<normal_node *>(child_entry.get());
        } while (true);
    }

    static auto s_try_locate(const normal_node *parent, std::size_t view) -> const node_ptr * {
        do {
            auto &child_entry = (*parent)[view];
            if (child_entry == nullptr)
                return nullptr;

            if (!child_entry->contains(view))
                return nullptr;

            if (child_entry->type == base_node::Type::Leaf)
                return &child_entry;

            parent = static_cast<normal_node *>(child_entry.get());
        } while (true);
    }

    inline constexpr static int kBitLen = std::bit_width(normal_node::kMask + 0u);
    inline constexpr static auto kLevel = std::numeric_limits<std::size_t>::digits / kBitLen;
    normal_node m_root{0, nullptr, kLevel *kBitLen};
};

} // namespace dark::__detail::intset

namespace dark {

using int_set = __detail::intset::set;

} // namespace dark
