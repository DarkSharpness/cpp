#include <experimental/memory>
#include <memory>

auto main() -> int {
    std::unique_ptr<int> q;
    std::experimental::observer_ptr<int> p{q.get()};
}
