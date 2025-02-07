#include <iostream>

signed main() {
    int x;
    std::cin >> x;
    [[assume(x > 0)]];
    std::cout << x / 2 << std::endl;
    return 0;
}
