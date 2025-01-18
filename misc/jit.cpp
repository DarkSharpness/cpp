#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/mman.h>

auto main() -> int {
    // mmap a page
    auto *page = static_cast<unsigned char *>(
        ::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    );

    // fill a mov command: mov eax, 1
    unsigned char mov[] = {0xb8, 0x01, 0x00, 0x00, 0x00};
    std::memcpy(page, mov, sizeof(mov));

    // fill a ret command: ret
    unsigned char ret[] = {0xc3};
    std::memcpy(page + sizeof(mov), ret, sizeof(ret));

    // remap the page as executable
    assert(::mprotect(page, 4096, PROT_READ | PROT_EXEC) == 0);

    using int_fn_t = int (*)(void);
    auto fn        = reinterpret_cast<int_fn_t>(page);
    int result     = fn();

    // check if we can execute the ret command
    std::cout << result << std::endl;
    return 0;
}
