#include "bar_model.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    lab::BarAllocator allocator(
        0x80000000ULL,
        0x80010000ULL
    );

    const auto bar0 = allocator.allocate(4096);
    const auto bar1 = allocator.allocate(8192);
    const auto bar2 = allocator.allocate(16384);

    assert(bar0 == 0x80000000ULL);
    assert(bar1 == 0x80002000ULL);
    assert(bar2 == 0x80004000ULL);

    // Allocation must reject invalid sizes.
    bool invalid_rejected = false;
    try {
        allocator.allocate(3000);
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    assert(invalid_rejected);

    // Allocation must reject exhausted windows.
    lab::BarAllocator small(
        0x90000000ULL,
        0x90001000ULL
    );

    small.allocate(4096);

    bool exhausted = false;
    try {
        small.allocate(4096);
    } catch (const std::overflow_error&) {
        exhausted = true;
    }
    assert(exhausted);

    std::cout << "PASS: BAR allocation tests\n";
}
