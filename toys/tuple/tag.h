#pragma once

namespace {

template <typename _Tag>
struct tag {
    using type = _Tag;
};

// a universal concept for type tagging
template <typename _Tp>
concept tag_like = requires { typename _Tp::type; };

template <tag_like _Tag>
using untag = typename _Tag::type;

} // namespace
