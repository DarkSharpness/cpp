#include <iostream>

template <typename _Tp, typename _Vp>
struct my_template {
    _Tp val;
};

template <typename _Tp>
using partial_t = my_template<_Tp, void>;

template <template <typename> class _Tp, typename _Vp>
struct A {
    _Tp<_Vp> val;
};

int main() {
    A<partial_t, int> a{};
    a.val.val = 10;
    std::cout << a.val.val << std::endl;
    return 0;
}
