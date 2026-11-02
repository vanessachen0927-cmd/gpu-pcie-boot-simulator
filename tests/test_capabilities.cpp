#include "capabilities.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    // Normal linked list
    lab::CapabilitySpace config;
    config.set_head(0x40);
    config.add(0x40, 0x05, 0x60); // MSI
    config.add(0x60, 0x10, 0x00); // PCI Express

    const auto caps = config.discover();

    assert(caps.size() == 2);
    assert(caps[0].id == 0x05);
    assert(caps[1].id == 0x10);
    assert(caps[1].next == 0);

    // Circular list must be rejected
    lab::CapabilitySpace cyclic;
    cyclic.set_head(0x40);
    cyclic.add(0x40, 0x05, 0x60);
    cyclic.add(0x60, 0x10, 0x40);

    bool cycle_detected = false;
    try {
        cyclic.discover();
    } catch (const std::runtime_error&) {
        cycle_detected = true;
    }
    assert(cycle_detected);

    // Invalid head pointer must be rejected
    lab::CapabilitySpace invalid;
    invalid.set_head(0x41);

    bool invalid_detected = false;
    try {
        invalid.discover();
    } catch (const std::out_of_range&) {
        invalid_detected = true;
    }
    assert(invalid_detected);

    std::cout << "PASS: PCIe capability discovery tests\n";
}
