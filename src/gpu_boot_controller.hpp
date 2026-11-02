#pragma once

#include "pcie.hpp"
#include "enumeration.hpp"
#include "capabilities.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace lab {

struct BootTrace {
    std::string stage;
    std::string detail;
};

struct IntegratedBootReport {
    bool success = false;
    std::string message;
    BootState state = BootState::Reset;
    uint64_t assigned_bytes = 0;
    std::vector<BootTrace> trace;
};

class GpuBootController {
public:
    GpuBootController(PcieFabric& fabric, RootComplex& root)
        : fabric_(fabric), root_(root) {}

    IntegratedBootReport boot(
        Endpoint& gpu,
        const CapabilitySpace& capabilities
    ) {
        IntegratedBootReport report;
        gpu.state = BootState::Reset;

        auto record = [&](const std::string& stage,
                          const std::string& detail) {
            report.trace.push_back({stage, detail});
        };

        auto fail = [&](const std::string& reason) {
            gpu.state = BootState::Failed;
            report.state = gpu.state;
            report.message = reason;
            record("FAILED", reason);
            return report;
        };

        record("RESET", "GPU initialization started");

        // Simplified link-state model, not LTSSM simulation.
        if (!gpu.link_up)
            return fail("PCIe link down");

        record("LINK", "PCIe link operational");

        // BDF-based discovery through simulated config space.
        const BDF address{gpu.bus, gpu.device, gpu.function};
        ConfigSpace* config = fabric_.find(address);

        if (!config)
            return fail("GPU not found at expected BDF");

        const uint16_t vendor = config->read16(0x00);
        const uint16_t device = config->read16(0x02);

        if (vendor == 0xFFFF)
            return fail("No PCIe device present");

        if (vendor != gpu.vendor_id || device != gpu.device_id)
            return fail("PCIe device identity mismatch");

        record("ENUMERATION", "Vendor and device IDs verified");

        // The v0.2 capability model is still a separate
        // simulated capability space.
        bool has_pcie = false;

        try {
            for (const auto& cap : capabilities.discover()) {
                if (cap.id == 0x10)
                    has_pcie = true;
            }
        } catch (const std::exception& error) {
            return fail(
                std::string("Capability traversal failed: ") +
                error.what()
            );
        }

        if (!has_pcie)
            return fail("PCI Express capability missing");

        record("CAPABILITIES", "PCI Express capability found");

        if (!gpu.firmware_valid)
            return fail("Firmware verification failed");

        record("FIRMWARE", "Simulated firmware validation passed");

        // Existing allocator validates BAR sizes, alignment,
        // and available MMIO aperture.
        const BootReport allocation = root_.initialize(gpu);

        if (!allocation.success)
            return fail(allocation.message);

        report.assigned_bytes = allocation.assigned_bytes;
        record("MMIO", "BAR resources allocated");

        // Enable simulated Memory Space and Bus Master bits.
        config->write_command(
            static_cast<uint16_t>(config->read16(0x04) | 0x0006)
        );

        if (!config->memory_enabled() ||
            !config->bus_master_enabled()) {
            return fail("PCIe command register configuration failed");
        }

        record("COMMAND", "Memory decoding and bus mastering enabled");

        gpu.state = BootState::Ready;
        report.state = gpu.state;
        report.success = true;
        report.message = "Integrated GPU boot completed";

        record("READY", report.message);

        return report;
    }

private:
    PcieFabric& fabric_;
    RootComplex& root_;
};

} // namespace lab
