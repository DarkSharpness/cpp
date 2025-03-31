#include "utils/error.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace dark::__detail::string_set {

struct normal_node;

struct base_node {
public:
    enum class Type : bool {
        Leaf,   // Not other data
        Normal, // Normal node
    };

    auto match(std::string_view v) const -> std::size_t {
        // find the longest common prefix
        const auto target = std::string_view{prefix};
        const auto len    = std::min(v.size(), target.size());
        for (std::size_t i = 0; i < len; ++i)
            if (v[i] != target[i])
                return i;
        return len;
    }

    auto length() const -> std::size_t {
        return prefix.length();
    }

    auto split(std::size_t n) -> std::string {
        auto result = prefix.substr(0, n);
        prefix.erase(0, n);
        return result;
    }

    auto front() const -> char {
        return prefix.front();
    }

protected:
    base_node(std::string &&s, normal_node *p, Type t) noexcept :
        prefix(std::move(s)), parent(p), type(t) {}
    std::string prefix; // prefix string

public:
    normal_node *parent;    // parent node
    const Type type;        // node type
    bool has_value = false; // has value

    static auto s_destroy(base_node *) noexcept -> void;
    struct deleter {
        auto operator()(base_node *ptr) const -> void {
            s_destroy(ptr);
        }
    };
};

using node_ptr = std::unique_ptr<base_node, base_node::deleter>;

struct leaf_node final : base_node {
    leaf_node(std::string &&v, normal_node *p) : base_node(std::move(v), p, Type::Leaf) {}
};

struct normal_node final : base_node {
public:
    normal_node(std::string &&v, normal_node *p) : base_node(std::move(v), p, Type::Normal) {}

    auto operator[](unsigned char n) -> node_ptr & {
        return children[n];
    }
    auto operator[](unsigned char n) const -> const node_ptr & {
        return children[n];
    }
    auto operator[](std::string_view s) -> node_ptr & {
        return children[s[0]];
    }
    auto operator[](std::string_view s) const -> const node_ptr & {
        return children[s[0]];
    }

private:
    std::array<node_ptr, 256> children; // 256 children
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
        iterator() = default;
        auto has_value() const -> bool {
            return m_node != nullptr;
        }

    private:
        iterator(const base_node *node) : m_node(node) {}
        const base_node *m_node;
        friend struct set;
    };

    struct insert_result_t {
        iterator it;
        bool inserted;
    };

    using erase_result_t = bool;

    auto insert(std::string_view sv) -> insert_result_t {
        return s_try_insert(&m_root, sv);
    }

    auto erase(std::string_view sv) -> erase_result_t {
        return s_try_remove(&m_root, sv);
    }

    auto find(std::string_view sv) const -> iterator {
        if (sv.empty())
            return iterator(m_root.has_value ? &m_root : nullptr);
        return iterator{s_try_locate(&m_root, sv)};
    }

private:
    static auto s_make_leaf(std::string_view sv, normal_node *p) -> node_ptr {
        return node_ptr{new leaf_node{std::string{sv}, p}};
    }

    static auto s_make_normal(std::string &&v, normal_node *p) -> node_ptr {
        return node_ptr{new normal_node{std::move(v), p}};
    }

    static auto s_split_at(
        normal_node *parent, node_ptr &child_entry, const std::size_t pos, std::string_view sv
    ) -> insert_result_t {
        sv.remove_prefix(pos);
        auto old_entry    = std::move(child_entry);
        auto new_entry    = s_make_normal(old_entry->split(pos), parent);
        auto &middle      = static_cast<normal_node &>(*new_entry.get());
        old_entry->parent = &middle;
        child_entry       = std::move(new_entry);
        const auto idx    = old_entry->front();
        middle[idx]       = std::move(old_entry);
        if (sv.empty()) {
            middle.has_value = true;
            return insert_result_t{&middle, true};
        } else {
            auto &result = middle[sv];
            result       = s_make_leaf(sv, &middle);
            return insert_result_t{result.get(), true};
        }
    }

    static auto s_try_insert(normal_node *parent, std::string_view sv) -> insert_result_t {
        do {
            if (sv.empty())
                return insert_result_t{parent, !std::exchange(parent->has_value, true)};

            auto &child_entry = (*parent)[sv];
            if (child_entry == nullptr) {
                child_entry = s_make_leaf(sv, parent);
                return insert_result_t{child_entry.get(), true};
            }

            if (auto l = child_entry->match(sv); l != child_entry->length())
                return s_split_at(parent, child_entry, l, sv);

            if (child_entry->type == base_node::Type::Leaf)
                return insert_result_t{child_entry.get(), false};

            parent = static_cast<normal_node *>(child_entry.get());
            sv.remove_prefix(parent->length());
        } while (true);
    }

    static auto s_try_remove(normal_node *parent, std::string_view sv) -> erase_result_t {
        do {
            if (sv.empty())
                return std::exchange(parent->has_value, false);

            auto &child_entry = (*parent)[sv];
            if (child_entry == nullptr)
                return false;

            if (child_entry->match(sv) != child_entry->length())
                return false;

            if (child_entry->type == base_node::Type::Leaf) {
                child_entry = nullptr;
                return true;
            }

            parent = static_cast<normal_node *>(child_entry.get());
            sv.remove_prefix(parent->length());
        } while (true);
    }

    static auto s_try_locate(const normal_node *node, std::string_view sv) -> const base_node * {
        do {
            if (sv.empty())
                return node->has_value ? node : nullptr;

            auto &child_entry = (*node)[sv];
            if (child_entry == nullptr)
                return nullptr;

            if (child_entry->match(sv) != child_entry->length())
                return nullptr;

            if (child_entry->type == base_node::Type::Leaf)
                return child_entry.get();

            node = static_cast<const normal_node *>(child_entry.get());
            sv.remove_prefix(node->length());
        } while (true);
    }

    normal_node m_root{"", nullptr};
};

} // namespace dark::__detail::string_set

namespace dark {

using radix_tree = __detail::string_set::set;

} // namespace dark
