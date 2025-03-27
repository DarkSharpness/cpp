#include <cstdio>
#include <source_location>
#include <utility>

struct A {
    A() {
        std::puts("A()");
    }
    A(const A &) {
        std::puts("A(const A&)");
    }
    A(A &&) {
        std::puts("A(A&&)");
    }
    auto operator=(const A &) -> A & {
        std::puts("operator=(const A&)");
        return *this;
    }
    auto operator=(A &&) -> A & {
        std::puts("operator=(A&&)");
        return *this;
    }
    ~A() {
        std::puts("~A()");
    }
};

struct B {
    B() {
        std::puts("B()");
    }
    B(const A &) {
        std::puts("B(const A&)");
    }
    B(A &&) {
        std::puts("B(A&&)");
    }
    B(const B &) {
        std::puts("B(const B&)");
    }
    B(B &&) {
        std::puts("B(B&&)");
    }
    auto operator=(const B &) -> B & {
        std::puts("operator=(const B&)");
        return *this;
    }
    auto operator=(B &&) -> B & {
        std::puts("operator=(B&&)");
        return *this;
    }
    ~B() {
        std::puts("~B()");
    }
};

struct C {
    C() {
        std::puts("C()");
    }
    C(const C &) {
        std::puts("C(const C&)");
    }
    C(C &&) {
        std::puts("C(C&&)");
    }
    C(A) {
        std::puts("C(A)");
    }
    auto operator=(const C &) -> C & {
        std::puts("operator=(const C&)");
        return *this;
    }
    auto operator=(C &&) -> C & {
        std::puts("operator=(C&&)");
        return *this;
    }
    ~C() {
        std::puts("~C()");
    }
};

auto print_line(std::source_location loc = std::source_location::current()) {
    std::printf("%s:%d\n", loc.file_name(), loc.line());
}

auto main() -> int {
    [] {
        A a;
        return a;
    }();
    print_line();

    [] {
        A a;
        B b{a};
        return b;
    }();
    print_line();

    [] {
        A a;
        B b{std::move(a)};
        return b;
    }();
    print_line();

    [] -> B {
        A a;
        return a;
    }();
    print_line();

    /*! NRVO don't allow redundant move */
    // [] -> B {
    //     A a;
    //     return std::move(a);
    // }();
    // print_line();

    [] -> B {
        A a;
        return B{a};
    }();
    print_line();

    [] -> B {
        A a;
        return B{std::move(a)};
    }();
    print_line();

    [] -> C {
        A a;
        return a;
    }();
    print_line();

    [] -> C {
        A a;
        return C{a};
    }();
    print_line();

    [] -> C {
        A a;
        return C{std::move(a)};
    }();
}
