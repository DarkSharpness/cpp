#pragma once
#include "../reflect/rf.h"
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace __detail {

struct empty_t {};

template <typename _Tp>
inline constexpr auto into_bytes(const _Tp &t, std::byte *dst) -> void {
    if consteval {
        using Array    = std::array<std::byte, sizeof(_Tp)>;
        const auto arr = std::bit_cast<Array>(t);
        std::ranges::copy(arr, dst);
    } else {
        dst = std::assume_aligned<alignof(_Tp)>(dst);
        std::memcpy(dst, &t, sizeof(_Tp));
    }
}

template <typename _Tp>
inline constexpr auto from_bytes(_Tp &t, const std::byte *src) -> void {
    if consteval {
        auto arr = std::array<std::byte, sizeof(_Tp)>{};
        std::ranges::copy_n(src, sizeof(_Tp), arr.begin());
        t = std::bit_cast<_Tp>(arr);
    } else {
        src = std::assume_aligned<alignof(_Tp)>(src);
        std::memcpy(&t, src, sizeof(_Tp));
    }
}

template <typename _Tp>
inline constexpr auto from_bytes(const std::byte *src) -> decltype(auto) {
    src      = std::assume_aligned<alignof(_Tp)>(src);
    auto arr = std::array<std::byte, sizeof(_Tp)>{};
    std::ranges::copy_n(src, sizeof(_Tp), arr.begin());
    return std::bit_cast<_Tp>(arr);
}

} // namespace __detail

using serialize_t = std::vector<std::byte>;

template <typename _Tp>
struct serializer;

template <typename _Tp>
    requires std::integral<_Tp> || std::floating_point<_Tp>
struct serializer<_Tp> {
    static constexpr auto static_size() -> std::size_t {
        return sizeof(_Tp);
    }

    static constexpr auto alignment() -> std::size_t {
        return alignof(_Tp);
    }

    static constexpr auto to_binary(_Tp v) -> std::vector<std::byte> {
        std::vector<std::byte> result(sizeof(_Tp));
        __detail::into_bytes(v, result.data());
        return result;
    }

    static constexpr auto
    from_binary(_Tp &t, const std::byte *p, std::size_t n = sizeof(_Tp))
        -> const std::byte * {
        if (n != sizeof(_Tp))
            throw std::invalid_argument("Invalid size");
        __detail::from_bytes(t, p);
        return p + sizeof(_Tp);
    }
};

namespace __detail {

inline constexpr auto kConstexprValue = std::size_t(-1);

template <typename _Tp>
inline constexpr auto type_hash() -> std::size_t {
    if consteval {
        return kConstexprValue;
    } else { // each thread acquire at most once
        static const thread_local auto v = typeid(_Tp).hash_code();
        return v;
    }
}

template <typename _Tp>
inline consteval auto try_static_size() -> std::optional<std::size_t> {
    using _Up = serializer<std::remove_cvref_t<_Tp>>;
    if constexpr (requires { _Up::static_size(); })
        return _Up::static_size();
    else
        return std::nullopt;
}

template <typename _Tp>
inline consteval auto alignment() -> std::size_t {
    return serializer<std::remove_cvref_t<_Tp>>::alignment();
}

template <typename _Tp>
inline constexpr auto to_binary(const _Tp &t) -> serialize_t {
    return serializer<_Tp>::to_binary(t);
}

template <typename _Tp>
inline constexpr auto from_binary(_Tp &t, const std::byte *p) {
    return serializer<std::remove_cvref_t<_Tp>>::from_binary(t, p);
}

template <typename _Tp>
inline constexpr auto from_binary(_Tp &t, const std::byte *p, std::size_t n) {
    return serializer<std::remove_cvref_t<_Tp>>::from_binary(t, p, n);
}

inline constexpr auto kMaxPos = std::numeric_limits<std::size_t>::max();

template <std::size_t _Nm = kMaxPos>
struct serialize_header;

template <>
struct serialize_header<kMaxPos> {
public:
    template <std::size_t _Nm>
    [[nodiscard]]
    auto to_n() const -> const serialize_header<_Nm> &;

    constexpr auto to_span() const -> std::span<const std::size_t, 0> {
        return {};
    }

    constexpr auto equal_hash(std::size_t h) const -> bool {
        return h == this->type_hash ||
               (h == kConstexprValue || this->type_hash == kConstexprValue);
    }

    constexpr auto size_bytes() const -> std::size_t {
        return this->overall_size;
    }

protected:
    constexpr serialize_header(std::size_t n, std::size_t c, std::size_t h) noexcept :
        overall_size(n), member_count(c), type_hash(h) {}

    std::size_t overall_size = 0;
    std::size_t member_count = 0;
    std::size_t type_hash    = 0;
};

inline constexpr auto kAlign = alignof(std::max_align_t);

template <std::size_t _Nm>
inline constexpr auto align_up(std::size_t n) -> std::size_t {
    static_assert(std::has_single_bit(kAlign));
    return (n + _Nm - 1) & ~(_Nm - 1);
}

template <std::size_t _Nm>
    requires(_Nm != kMaxPos)
struct alignas(kAlign) serialize_header<_Nm> : private serialize_header<> {
private:
    static constexpr auto kSpace = sizeof(serialize_header<>) + _Nm * sizeof(std::size_t);
    static constexpr auto kArray = kAlign - kSpace % kAlign;
    static constexpr auto kNoPad = kSpace % kAlign == 0;
    using Members_t = std::conditional_t<_Nm == 0, empty_t, std::array<std::size_t, _Nm>>;
    using Padding_t = std::conditional_t<kNoPad, empty_t, std::array<std::byte, kArray>>;

    [[no_unique_address]]
    Members_t members = {};
    [[no_unique_address]]
    Padding_t padding = {};

public:
    using serialize_header<>::equal_hash;
    using serialize_header<>::size_bytes;

    constexpr serialize_header(
        std::size_t all, std::span<const std::size_t, _Nm> sizes, std::size_t h
    ) noexcept : serialize_header<>(all, _Nm, h) {
        if constexpr (_Nm != 0)
            std::ranges::copy(sizes, this->members);
    }

    template <std::size_t _I>
        requires(_I < _Nm)
    constexpr auto get() const -> std::size_t {
        return this->members[_I];
    }

    constexpr auto arr() const -> decltype(auto) {
        if constexpr (_Nm == 0)
            return std::array<std::size_t, 0>{};
        else
            return this->members;
    }
};

template <std::size_t _Nm>
auto serialize_header<>::to_n() const -> const serialize_header<_Nm> & {
    if (this->member_count != _Nm)
        throw std::invalid_argument("Invalid member count");
    return *std::launder(std::bit_cast<const serialize_header<_Nm> *>(this));
}

template <typename... _Args>
inline consteval auto dynamic_count() -> std::size_t {
    return (std::size_t(!try_static_size<_Args>().has_value()) + ...);
}

template <typename... _Args>
inline consteval auto dynamic_count(const std::tuple<_Args &...> *) {
    return (std::size_t(!try_static_size<_Args>().has_value()) + ...);
}

struct option {
    bool is_static;  // whether the size is static
    std::size_t raw; // if static, then the value is the size
                     // otherwise, the value is the index

    consteval auto static_size() const -> std::size_t {
        if (!is_static)
            throw; // should not be called
        return raw;
    }

    consteval auto dynamic_index() const -> std::size_t {
        if (is_static)
            throw; // should not be called
        return raw;
    }
};

template <typename... _Args>
inline constexpr auto make_pack_aux() -> const auto & {
    static constexpr auto alignments_ = std::array{alignment<_Args>()..., kAlign};
    static constexpr auto needed_cnt_ = dynamic_count<_Args...>();
    static constexpr auto size_arr_   = std::array{try_static_size<_Args>()...};
    static constexpr auto prefix_arr_ = [] {
        auto result = std::array<std::size_t, needed_cnt_>{};
        auto which  = std::size_t{};
        for (std::size_t i = 0; i < size_arr_.size(); ++i)
            if (!size_arr_[i].has_value())
                result[which++] = i;
        return result;
    }();
    static constexpr auto option_arr_ = [] {
        auto result = std::array<option, sizeof...(_Args)>{};
        auto which  = std::size_t{};
        for (std::size_t i = 0; i < size_arr_.size(); ++i) {
            if (auto size = size_arr_[i]; size.has_value())
                result[i] = {.is_static = true, .raw = *size};
            else // is dynamic size
                result[i] = {.is_static = false, .raw = which++};
        }
        return result;
    }();

    struct Impl {
        decltype(needed_cnt_) &needed_cnt;
        decltype(prefix_arr_) &prefix_arr;
        decltype(alignments_) &alignments;
        decltype(option_arr_) &option_arr;
    };

    static constexpr auto result = Impl{
        .needed_cnt = needed_cnt_,
        .prefix_arr = prefix_arr_,
        .alignments = alignments_,
        .option_arr = option_arr_,
    };

    return result;
}

template <typename... _Args>
inline constexpr auto make_sizes_aux(std::span<const serialize_t> s) {
    static constexpr auto &pack = make_pack_aux<_Args...>();
    constexpr auto &prefix_arr  = pack.prefix_arr;
    constexpr auto &needed_cnt  = pack.needed_cnt;
    // use indexing method to set the size of each member
    auto result = decltype(prefix_arr){};
    [&]<std::size_t... _Is>(std::index_sequence<_Is...>) {
        ((result[_Is] = s[prefix_arr[_Is]].size()), ...);
    }(std::make_index_sequence<needed_cnt>{});
    return result;
}

template <bool _Copy, typename _Header, typename... _Args, std::size_t _Nm>
inline constexpr auto copy_value_aux(
    const std::array<std::size_t, _Nm> &sizes, std::span<const serialize_t> s,
    std::byte *output
) -> decltype(auto) {
    const auto ptr_out          = std::assume_aligned<kAlign>(output);
    static constexpr auto &pack = make_pack_aux<_Args...>();
    constexpr auto &option_arr  = pack.option_arr;
    constexpr auto &alignments  = pack.alignments;
    const auto helper_func      = [&]<std::size_t _Is>(std::size_t size) {
        constexpr auto &opt = option_arr[_Is];
        constexpr auto last = alignments[_Is];
        constexpr auto next = alignments[_Is + 1]; // next align
        if constexpr (_Copy)
            std::ranges::copy(s[_Is], std::assume_aligned<last>(ptr_out + size));
        if constexpr (opt.is_static)
            size += opt.static_size();
        else
            size += sizes.at(opt.dynamic_index());
        if constexpr (last % next == 0)
            return size; // last one is larger, no need to align
        else
            return align_up<next>(size);
    };
    return [&]<std::size_t... _Is>(std::index_sequence<_Is...>) {
        auto result = sizeof(_Header);
        ((result = helper_func.template operator()<_Is>(result)), ...);
        if constexpr (_Copy)
            return output + result;
        else
            return result;
    }(std::make_index_sequence<sizeof...(_Args)>{});
}

template <typename... _Args, std::size_t _Nm>
inline constexpr auto copy_value_aux(
    const std::array<std::size_t, _Nm> &sizes, const std::tuple<_Args &...> &args,
    const std::byte *input
) -> const std::byte * {
    const auto ptr_in           = std::assume_aligned<kAlign>(input);
    static constexpr auto &pack = make_pack_aux<_Args...>();
    constexpr auto &option_arr  = pack.option_arr;
    constexpr auto &alignments  = pack.alignments;
    const auto helper_func      = [&]<std::size_t _Is>(auto &arg, std::size_t size) {
        constexpr auto &opt = option_arr[_Is];
        constexpr auto last = alignments[_Is];
        constexpr auto next = alignments[_Is + 1]; // next align
        if constexpr (opt.is_static) {
            from_binary(arg, ptr_in + size);
            size += opt.static_size();
        } else {
            const auto this_size = sizes.at(opt.dynamic_index());
            from_binary(arg, ptr_in + size, this_size);
            size += this_size;
        }
        if constexpr (last % next == 0)
            return size; // last one is larger, no need to align
        else
            return align_up<next>(size);
    };
    return [&]<std::size_t... _Is>(std::index_sequence<_Is...>) {
        auto result = sizeof(serialize_header<_Nm>);
        ((result = helper_func.template operator()<_Is>(std::get<_Is>(args), result)),
         ...);
        return input + result;
    }(std::make_index_sequence<sizeof...(_Args)>{});
}

} // namespace __detail

template <typename _Tp>
inline constexpr auto serialize(const _Tp &t) -> std::vector<std::byte> {
    using __detail::serialize_header, __detail::to_binary, __detail::make_sizes_aux,
        __detail::type_hash, __detail::into_bytes, __detail::copy_value_aux;
    return std::apply(
        []<typename... _Args>(const _Args &...args) {
            std::vector<std::byte> result;

            const auto data = std::array{to_binary(args)...};
            const auto size = make_sizes_aux<_Args...>(data);
            using Header    = serialize_header<std::tuple_size_v<decltype(size)>>;
            const auto need = copy_value_aux<false, Header, _Args...>(size, {}, {});
            result.resize(need);

            const auto header = Header{need, size, type_hash<_Tp>()};
            into_bytes(header, result.data());
            copy_value_aux<true, Header, _Args...>(size, data, result.data());

            return result;
        },
        reflect::flatten(t)
    );
}

template <typename _Tp>
struct deserialize_result {
    _Tp value;
    std::span<const std::byte> rest;
};

template <std::constructible_from<> _Tp>
inline constexpr auto deserialize(std::span<const std::byte> input)
    -> deserialize_result<_Tp> {
    using __detail::dynamic_count, __detail::copy_value_aux, __detail::type_hash,
        __detail::from_bytes, __detail::serialize_header, __detail::kAlign;
    using Meta = serialize_header<>;

    if !consteval { // check only in non-constexpr context
        if (std::bit_cast<std::size_t>(input.data()) % kAlign != 0)
            throw std::invalid_argument("Invalid input alignment");
    }

    if (input.size_bytes() < sizeof(Meta))
        throw std::invalid_argument("Invalid input size");

    const auto meta = __detail::from_bytes<Meta>(input.data());
    if (input.size_bytes() < meta.size_bytes())
        throw std::invalid_argument("Invalid input size");

    auto hash_code = type_hash<_Tp>(); // if const, gcc may evaluate it at compile time
    if (!meta.equal_hash(hash_code))
        throw std::invalid_argument("Invalid type hash");

    auto result = deserialize_result<_Tp>{};
    result.rest = input.subspan(input.size_bytes());

    const auto ref_tuple = reflect::flatten(result.value);
    constexpr auto Nm    = dynamic_count(decltype(&ref_tuple){});

    if consteval {
        const auto header = from_bytes<serialize_header<Nm>>(input.data());
        copy_value_aux(header.arr(), ref_tuple, input.data());
    } else {
        const auto &header = meta.to_n<Nm>();
        copy_value_aux(header.arr(), ref_tuple, input.data());
    }
    return result;
}
