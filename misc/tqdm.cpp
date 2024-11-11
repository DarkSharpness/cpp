#include <cstddef>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <utility>

inline auto get_terminal_size() -> std::pair<int, int> {
    static const auto _S_value = []() {
        struct ::winsize w;
        ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return std::make_pair<int, int>(w.ws_col, w.ws_row);
    }(); // Construct on first initialization
    return _S_value;
}

struct status_bar {
private:
    static auto get_length(std::size_t total) -> std::size_t {
        const auto kWin = get_terminal_size().first;
        // reserve enough space for the progress bar
        // [ =====>   ] last/total
        return kWin - (7 + std::to_string(total).size() * 2) - 4;
    }

public:
    status_bar(std::size_t total) : _M_iterations(total), _M_length(get_length(total)) {
        _M_buffer                = std::string(_M_length + 4, ' ');
        _M_buffer[0]             = '[';
        _M_buffer[_M_length + 3] = ']';
    }

    ~status_bar() {
        std::cout << std::endl;
    }

    auto update(std::size_t current) -> void {
        const auto percentage = static_cast<double>(current) / _M_iterations;
        const auto cur_length = static_cast<std::size_t>(percentage * _M_length);
        // bar = [ =====>   ]
        for (std::size_t i = 0; i < cur_length; ++i)
            _M_buffer[2 + i] = '=';
        if (cur_length < _M_length)
            _M_buffer[2 + cur_length] = '>';
        std::cout << "\r " << _M_buffer << " " << current << "/" << _M_iterations
                  << std::flush;
    }

    struct iterator {
    public:
        iterator(status_bar &bar) : _M_bar(bar), _M_index(0) {}
        iterator &operator++() {
            _M_bar.update(++_M_index);
            return *this;
        }
        auto operator!=(std::size_t rhs) const -> bool {
            return _M_index != rhs;
        }
        auto operator*() const -> std::size_t {
            return _M_index;
        }

    private:
        status_bar &_M_bar;
        std::size_t _M_index;
    };

    auto begin() -> iterator {
        return iterator{*this};
    }
    auto end() -> std::size_t {
        return _M_iterations;
    }

private:
    std::string _M_buffer;
    const std::size_t _M_iterations;
    const std::size_t _M_length;
};

auto main() -> int {
    using namespace std::chrono_literals;
    for (auto _ [[maybe_unused]] : status_bar(100)) {
        std::this_thread::sleep_for(100ms);
    }
}
