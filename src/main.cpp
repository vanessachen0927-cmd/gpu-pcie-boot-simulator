#include "gpu_boot_controller.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    lab::Endpoint gpu{
        0x10DE, 0x2684, 0, 1, 0,
        {{0x1000, 0, false}, {0x2000, 0, true}},
        {0x01, 0x10}
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--link-down") {
            gpu.link_up = false;
        } else if (arg == "--bad-firmware") {
            gpu.firmware_valid = false;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    lab::PcieFabric fabric;
    fabric.attach(
        {gpu.bus, gpu.device, gpu.function},
        lab::ConfigSpace(gpu.vendor_id, gpu.device_id)
    );

    lab::CapabilitySpace capabilities;
    capabilities.set_head(0x40);
    capabilities.add(0x40, 0x01, 0x50); // Power Management
    capabilities.add(0x50, 0x10, 0);    // PCI Express

    lab::RootComplex root;
    lab::GpuBootController controller(fabric, root);

    const auto start = std::chrono::steady_clock::now();
    const auto result = controller.boot(gpu, capabilities);
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start
        ).count();

    std::cout << "=== Simulated GPU PCIe Boot Trace ===\n";

    for (const auto& event : result.trace) {
        std::cout << "[" << event.stage << "] "
                  << event.detail << "\n";
    }

    std::cout << "state=" << lab::state_name(result.state)
              << " success=" << result.success
              << " message=\"" << result.message << "\""
              << " elapsed_us=" << elapsed_us << "\n";

    for (size_t i = 0; i < gpu.bars.size(); ++i) {
        std::cout << "BAR" << i
                  << " address=0x" << std::hex
                  << gpu.bars[i].address << std::dec
                  << " size=" << gpu.bars[i].size << "\n";
    }

    return result.success ? 0 : 2;
}
