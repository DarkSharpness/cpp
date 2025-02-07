#include "assume.h"
#include <iostream>

auto main() -> int {
    try {
        dark::assume(false);
    } catch (const std::exception &e) {
        // assumption failed as expected
        std::cout << e.what() << " as expected\n";
    }

    try {
        dark::assume(false, [] { return "it's still false"; });
    } catch (const std::exception &e) {
        // assumption failed as expected
        std::cout << e.what() << " as expected\n";
    }

    dark::assume(!!std::cout);

    try {
        dark::assume(1 + 1 == 3, "{1:} + {1:} != {0:}", 3, 1);
    } catch (const std::exception &e) {
        // assumption failed as expected
        std::cout << e.what() << " as expected\n";
    }
}
