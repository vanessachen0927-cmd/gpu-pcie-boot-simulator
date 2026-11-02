#include "config_space.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    lab::ConfigSpace gpu(0x10DE, 0x2684);

    // Device identity
    assert(gpu.read16(0x00) == 0x10DE);
    assert(gpu.read16(0x02) == 0x2684);
    assert(gpu.read32(0x00) == 0x268410DE);

    // Device starts with memory access disabled
    assert(!gpu.memory_enabled());
    assert(!gpu.bus_master_enabled());

    // Host enables memory and bus mastering
    gpu.write_command(0x0006);
    assert(gpu.memory_enabled());
    assert(gpu.bus_master_enabled());

    // Unsupported command bits are masked
    gpu.write_command(0xFFFF);
    assert(gpu.read16(0x04) == 0x0006);

    // Invalid access must be rejected
    bool rejected = false;
    try {
        gpu.read32(4094);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "PASS: PCIe configuration-space tests\n";
}
