#include "sl.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <tuple>

namespace {

struct test_type {
    std::tuple<int, int, int> x;
    std::array<int, 4> y;
};

auto write() -> void {
    std::fstream out("/tmp/tmp.bin", std::ios::binary | std::ios::out);
    constexpr auto tmp = test_type{.x = {1, 2, 3}, .y = {4, 5, 6, 7}};
    auto vec           = serialize(tmp);
    std::ranges::copy(
        vec | std::views::transform([](std::byte c) { return static_cast<char>(c); }),
        std::ostreambuf_iterator(out)
    );
    out.close();
}

auto read() -> void {
    const auto file_size = std::filesystem::file_size("/tmp/tmp.bin");
    auto vec             = serialize_t(file_size);

    std::fstream in("/tmp/tmp.bin", std::ios::binary | std::ios::in);
    in.read(reinterpret_cast<char *>(vec.data()), file_size);

    const auto [result, rest] = deserialize<test_type>(vec);
    const auto vec2           = {
        std::get<0>(result.x),
        std::get<1>(result.x),
        std::get<2>(result.x),
        result.y[0],
        result.y[1],
        result.y[2],
        result.y[3]
    };
    std::ranges::copy(vec2, std::ostream_iterator<int>(std::cout, " "));
    std::cout << std::endl;
}

struct simple {
    int x;
    long y;
};

[[maybe_unused]]
constexpr auto convert(simple s) -> simple {
    auto v              = serialize(s);
    auto o              = sizeof(__detail::serialize_header<0>);
    v[o]                = std::byte{233};
    v[o + sizeof(long)] = std::byte{66}; // atomically aligned.
    return deserialize<simple>(v).value;
}

[[maybe_unused]]
constexpr auto sp = convert(simple{1, 2});

} // namespace

auto main() -> int {
    write();
    read();
    static_assert(sp.x == 233 && sp.y == 66);
    return 0;
}
