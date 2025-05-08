#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <vector>

auto main() -> int {
    static constexpr auto kInsert = 1 << 20;
    static constexpr auto kFind   = 1 << 18;
    constexpr auto kDistance      = std::array{
        1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20,
    };
    constexpr auto kStep = std::array{64, 128, 256, 512};

    const auto file     = std::string_view{std::source_location::current().file_name()};
    const auto basefile = std::filesystem::path{file}.parent_path();
    const auto values   = [] {
        auto result = std::vector<std::string>{};
        result.reserve(kInsert);
        for (const auto v : std::views::iota(0, kInsert))
            result.emplace_back(std::to_string(v));
        std::ranges::sort(result);
        return result;
    }();

    std::println("path = {}", basefile.string());

    auto generate_input = [&](int i) {
        auto input  = std::ofstream{basefile / std::format("{}.in", i)};
        auto output = std::ofstream{basefile / std::format("{}.out", i)};
        std::println(input, "{}", kInsert);
        for (const auto &v : values)
            std::println(input, "insert {0} {0}", v);
    };

    auto generate_testpoint_0 = [&](int i, int dist) {
        auto input  = std::ofstream{basefile / std::format("{}.in", i)};
        auto output = std::ofstream{basefile / std::format("{}.out", i)};
        std::println(input, "{}", kFind * kStep.size());
        for (const auto step : kStep) {
            for (const auto j : std::views::iota(0, kInsert / dist)) {
                for (const auto v : std::views::iota(0, step / (kInsert / kFind))) {
                    for (const auto l : std::views::iota(0, dist / step)) {
                        const auto index = j * dist + l * step + v % step;
                        if (index >= kInsert) {
                            throw std::runtime_error(
                                std::format("index {} out of range", index)
                            );
                        }
                        std::println(input, "find {}", values[index]);
                        std::println(output, "{}", values[index]);
                    }
                }
            }
        }
    };

    auto generate_testpoint_1 = [&](int i, int dist) {
        auto input  = std::ofstream{basefile / std::format("{}.in", i)};
        auto output = std::ofstream{basefile / std::format("{}.out", i)};
        std::println(input, "{}", kFind * kStep.size());
        for (const auto step : kStep) {
            for (const auto j : std::views::iota(0, kInsert / dist)) {
                for (const auto v : std::views::iota(0, step / (kInsert / kFind))) {
                    for (const auto l : std::views::iota(0, dist / step)) {
                        const auto index = l * step + (j + v) % step;
                        if (index >= kInsert) {
                            throw std::runtime_error(
                                std::format("index {} out of range", index)
                            );
                        }
                        std::println(input, "find {}", values[index]);
                        std::println(output, "{}", values[index]);
                    }
                }
            }
        }
    };

    for (const auto [i, dist] : kDistance | std::views::enumerate) {
        std::println(" Cache distance = {}: ", dist);
        generate_input(i * 3 + 1);
        std::println("  Input generated");
        generate_testpoint_0(i * 3 + 3, dist);
        std::println("  Testpoint generated, memory footprint = {}", kInsert);
        generate_testpoint_1(i * 3 + 2, dist);
        std::println("  Testpoint generated, memory footprint = {}", dist);
    }

    std::println("done");
}
