#include "pcie.hpp"
#include "enumeration.hpp"
#include "config_space.hpp"

#include <cassert>
#include <iostream>

int main() {
    lab::PcieFabric fabric;

    fabric.attach(
        {0, 1, 0},
        lab::ConfigSpace(0x10DE, 0x2684)
    );

    const auto devices = fabric.enumerate();
    assert(devices.size() == 1);
    assert(devices[0].vendor_id == 0x10DE);

    lab::RootComplex root;
    assert(root.next_mmio() == 0x80000000ULL);

    std::cout
        << "PASS: PCIe fabric and root complex coexist\n";
}
