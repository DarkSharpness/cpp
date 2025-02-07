#include "assume.h"
#include <exception>
#include <source_location>
#include <stacktrace>

template <typename _Fn>
auto catch_call(_Fn &&fn) -> void {
    try {
        std::forward<_Fn>(fn)();
    } catch (const std::exception &e) {
        std::cout << "  why panic: " << e.what() << std::endl;
    }
}

auto main() -> int {
    static bool do_stop = false;

    dark::panic_handler::add_hook([](const std::string &, auto &&...) {
        std::cerr << "stacktrace:\n";
        std::cerr << std::stacktrace::current(1, 10) << '\n';
    });

    dark::panic_handler::add_hook([](const std::string &msg, auto &&...) {
        std::cerr << "\033[1;33m"
                     "Custom hook is reached"
                     "\033[0m";
        if (do_stop) {
            std::cerr << ": Aborting the program\n";
            std::cerr << "  Reason: " << msg << '\n';
            // std::abort();
        } else {
            std::cerr << '\n';
        }
    });

    catch_call([] { dark::panic("This is a panic message."); });

    do_stop = true;
    catch_call([] { dark::panic("This is another panic message."); });

    // pop all hooks
    while (dark::panic_handler::pop_hook())
        ;

    dark::panic_handler::add_hook([](const std::string &msg, std::source_location src,
                                     const char *type) {
        std::cerr << "This is the last message you will see!\n";
        std::cerr << std::format("  Error Type:         {}\n", type);
        std::cerr << std::format("  Error Message:      {}\n", msg);
        // clang-format off
        std::cerr << std::format("  Error Location:     {}:{}:{}\n",
            src.file_name(), src.line(), src.column());
        // clang-format on
        std::cerr << "Bye!\n";
    });

    // since no hook throw error (including the default one), std::terminate() is called
    catch_call([] { dark::panic("This is the end of the story."); });

    return 0;
}
