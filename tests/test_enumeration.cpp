#include "enumeration.hpp"

#include <cassert>
#include <iostream>

int main() {
    lab::RootComplex root;

    root.attach({0, 1, 0}, lab::ConfigSpace(0x10DE, 0x2684));
    root.attach({0, 2, 0}, lab::ConfigSpace(0x8086, 0x1234));
    root.attach({1, 0, 0}, lab::ConfigSpace(0x10DE, 0x1E04));

    const auto devices = root.enumerate();

    assert(devices.size() == 3);

    assert(devices[0].address.bus == 0);
    assert(devices[0].address.device == 1);
    assert(devices[0].vendor_id == 0x10DE);

    assert(devices[1].address.device == 2);
    assert(devices[1].vendor_id == 0x8086);

    assert(devices[2].address.bus == 1);
    assert(devices[2].vendor_id == 0x10DE);

    std::cout << "PASS: discovered "
              << devices.size()
              << " PCIe devices\n";
}
