#include "gpu_boot_controller.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace lab;

Endpoint make_gpu() {
    Endpoint gpu{};
    gpu.vendor_id = 0x10DE;
    gpu.device_id = 0x2684;
    gpu.bus = 0;
    gpu.device = 1;
    gpu.function = 0;
    gpu.bars = {{0x1000, 0, false}, {0x2000, 0, true}};
    return gpu;
}

CapabilitySpace make_capabilities(bool pcie = true) {
    CapabilitySpace caps;
    caps.set_head(0x40);
    caps.add(0x40, pcie ? 0x10 : 0x01, 0);
    return caps;
}

int main() {
    // 1. Successful integrated boot
    {
        PcieFabric fabric;
        fabric.attach({0, 1, 0}, ConfigSpace(0x10DE, 0x2684));

        RootComplex root;
        GpuBootController controller(fabric, root);

        auto gpu = make_gpu();
        const auto result = controller.boot(gpu, make_capabilities());

        assert(result.success);
        assert(result.state == BootState::Ready);
        assert(result.trace.size() >= 7);
        assert(gpu.bars[0].address == 0x80000000ULL);
        assert(gpu.bars[1].address == 0x80002000ULL);

        const auto* config = fabric.find({0, 1, 0});
        assert(config != nullptr);
        assert(config->memory_enabled());
        assert(config->bus_master_enabled());
    }

    // 2. Missing GPU
    {
        PcieFabric fabric;
        RootComplex root;
        GpuBootController controller(fabric, root);

        auto gpu = make_gpu();
        const auto result = controller.boot(gpu, make_capabilities());

        assert(!result.success);
        assert(result.state == BootState::Failed);
    }

    // 3. PCIe link failure
    {
        PcieFabric fabric;
        fabric.attach({0, 1, 0}, ConfigSpace(0x10DE, 0x2684));

        RootComplex root;
        GpuBootController controller(fabric, root);

        auto gpu = make_gpu();
        gpu.link_up = false;

        const auto result = controller.boot(gpu, make_capabilities());

        assert(!result.success);
        assert(result.message == "PCIe link down");
    }

    // 4. Missing PCI Express capability
    {
        PcieFabric fabric;
        fabric.attach({0, 1, 0}, ConfigSpace(0x10DE, 0x2684));

        RootComplex root;
        GpuBootController controller(fabric, root);

        auto gpu = make_gpu();
        const auto result = controller.boot(gpu, make_capabilities(false));

        assert(!result.success);
        assert(result.message == "PCI Express capability missing");
    }

    // 5. MMIO aperture exhaustion
    {
        PcieFabric fabric;
        fabric.attach({0, 1, 0}, ConfigSpace(0x10DE, 0x2684));

        RootComplex root(0x80000000ULL, 0x80001000ULL);
        GpuBootController controller(fabric, root);

        auto gpu = make_gpu();
        const auto result = controller.boot(gpu, make_capabilities());

        assert(!result.success);
        assert(result.message == "MMIO aperture exhausted");

        const auto* config = fabric.find({0, 1, 0});
        assert(config != nullptr);
        assert(!config->memory_enabled());
        assert(!config->bus_master_enabled());
    }

    std::cout << "PASS: integrated GPU boot scenarios\n";
    return 0;
}
