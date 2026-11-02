#include "Vgpu_boot_regs.h"
#include "verilated.h"
#include "gpu_boot_controller.hpp"
#include "tlp_model.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

class GpuHardware {
public:
    Vgpu_boot_regs dut;

    GpuHardware() {
        dut.clk = 0;
        dut.rst_n = 0;
        dut.bus_valid = 0;
        dut.bus_write = 0;
        dut.bus_addr = 0;
        dut.bus_wdata = 0;
        dut.fw_valid = 0;
        dut.fw_load_done = 0;
        dut.eval();
    }

    void tick() {
        dut.clk = 0;
        dut.eval();
        dut.clk = 1;
        dut.eval();
        dut.clk = 0;
        dut.eval();
    }

    void reset() {
        dut.rst_n = 0;
        dut.bus_valid = 0;
        dut.fw_valid = 0;
        dut.fw_load_done = 0;
        tick();
        dut.rst_n = 1;
        dut.eval();
    }

    void write(uint8_t address, uint32_t value) {
        dut.bus_addr = address;
        dut.bus_wdata = value;
        dut.bus_write = 1;
        dut.bus_valid = 1;

        tick();

        dut.bus_valid = 0;
        dut.bus_write = 0;
        dut.eval();
    }

    uint32_t read(uint8_t address) {
        dut.bus_addr = address;
        dut.bus_write = 0;
        dut.bus_valid = 1;
        dut.eval();

        const uint32_t value = dut.bus_rdata;

        dut.bus_valid = 0;
        dut.eval();
        return value;
    }
};

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    constexpr uint8_t BOOT_CONTROL = 0x00;
    constexpr uint8_t BOOT_STATUS  = 0x04;
    constexpr uint8_t ERROR_CODE   = 0x08;

    GpuHardware gpu;

    try {
        // Scenario 1: C++ starts a successful RTL boot.
        gpu.reset();
        gpu.write(BOOT_CONTROL, 1);

        gpu.dut.fw_load_done = 1;
        gpu.dut.fw_valid = 1;
        gpu.tick();
        gpu.tick();

        require((gpu.read(BOOT_STATUS) & 1u) != 0,
                "RTL did not report READY");
        require(gpu.read(ERROR_CODE) == 0,
                "Unexpected hardware error");

        std::cout
            << "PASS: C++ initiated RTL GPU boot\n";

        // Scenario 2: Invalid firmware rejected by RTL.
        gpu.reset();
        gpu.write(BOOT_CONTROL, 1);

        gpu.dut.fw_load_done = 1;
        gpu.dut.fw_valid = 0;
        gpu.tick();
        gpu.tick();

        require((gpu.read(BOOT_STATUS) & 2u) != 0,
                "RTL did not report ERROR");
        require(gpu.read(ERROR_CODE) == 1,
                "Wrong firmware error code");

        std::cout
            << "PASS: C++ observed RTL firmware rejection\n";

        // Scenario 3: Hardware timeout.
        gpu.reset();
        gpu.write(BOOT_CONTROL, 1);

        for (int i = 0; i < 10; ++i)
            gpu.tick();

        require((gpu.read(BOOT_STATUS) & 2u) != 0,
                "RTL timeout was not detected");
        require(gpu.read(ERROR_CODE) == 2,
                "Wrong timeout error code");

        std::cout
            << "PASS: C++ observed RTL boot timeout\n";

        // Scenario 4: Hardware reset clears failure.
        gpu.reset();

        require(gpu.read(ERROR_CODE) == 0,
                "Reset did not clear error code");
        require((gpu.read(BOOT_STATUS) & 3u) == 0,
                "Reset did not clear READY/ERROR");

        std::cout
            << "PASS: C++ reset RTL hardware\n";

        // End-to-end PCIe host to RTL device integration.
        lab::Endpoint endpoint{
            0x10DE, 0x2684, 0, 1, 0,
            {{0x1000, 0, false}, {0x2000, 0, true}},
            {0x01, 0x10}
        };

        lab::PcieFabric fabric;
        fabric.attach(
            {0, 1, 0},
            lab::ConfigSpace(endpoint.vendor_id, endpoint.device_id)
        );

        lab::CapabilitySpace capabilities;
        capabilities.set_head(0x40);
        capabilities.add(0x40, 0x01, 0x50);
        capabilities.add(0x50, 0x10, 0);

        lab::RootComplex root;
        lab::GpuBootController controller(fabric, root);

        const auto boot = controller.boot(endpoint, capabilities);

        require(boot.success, "PCIe host initialization failed");

        auto* config = fabric.find({0, 1, 0});
        require(config != nullptr, "GPU config space missing");
        require(config->memory_enabled(), "MMIO decoding disabled");
        require(config->bus_master_enabled(), "Bus mastering disabled");

        // BAR0 is 32-bit. BAR1/BAR2 are a 64-bit pair.
        require(
            (config->read_bar(0) & 0xFFFFFFF0u) ==
                endpoint.bars[0].address,
            "BAR0 configuration address mismatch"
        );

        const uint64_t bar1_address =
            (static_cast<uint64_t>(config->read_bar(2)) << 32) |
            (config->read_bar(1) & 0xFFFFFFF0u);

        require(
            bar1_address == endpoint.bars[1].address,
            "64-bit BAR address mismatch"
        );

        // Reset RTL so the PCIe-discovered host can initiate boot.
        gpu.reset();

        const uint64_t mmio_base = endpoint.bars[0].address;
        const uint64_t mmio_size = endpoint.bars[0].size;

        // v2.0 packet-level teaching model. C++ TLPs route through
        // simulated BAR decoding to the Verilator-backed registers.
        lab::TlpEndpoint tlp_device({0,1,0}, *config, mmio_base, mmio_size,
            [&](uint32_t offset) { return gpu.read(static_cast<uint8_t>(offset)); },
            [&](uint32_t offset, uint32_t value) {
                gpu.write(static_cast<uint8_t>(offset), value);
            });
        lab::TlpLink link(tlp_device);
        lab::TlpRequest request{
            lab::TlpKind::ConfigRead0, {0,1,0}, 0x0100, 0x2a, 0x00
        };

        const auto discovery = link.transact(request);
        require(discovery.completion &&
                discovery.completion->status == lab::CompletionStatus::Successful &&
                discovery.completion->payload == 0x268410deu,
                "TLP config read did not find GPU");

        request.kind = lab::TlpKind::MemoryWrite32;
        request.address = mmio_base + BOOT_CONTROL;
        request.payload = 1;
        const auto started = link.transact(request);
        require(started.completion &&
                started.completion->status == lab::CompletionStatus::Successful,
                "MMIO TLP boot-control write rejected");

        gpu.dut.fw_load_done = 1;
        gpu.dut.fw_valid = 1;
        gpu.tick();
        gpu.tick();

        request.kind = lab::TlpKind::MemoryRead32;
        request.address = mmio_base + BOOT_STATUS;
        const auto status_tlp = link.transact(request);
        require(status_tlp.completion &&
                status_tlp.completion->status == lab::CompletionStatus::Successful &&
                (status_tlp.completion->payload & 1u) != 0,
                "PCIe TLP-driven RTL boot did not reach READY");

        request.address = mmio_base + ERROR_CODE;
        const auto error_tlp = link.transact(request);
        require(error_tlp.completion && error_tlp.completion->payload == 0,
                "PCIe TLP-driven boot reported an error");

        // Exercise modeled completion loss and invalid MMIO aperture.
        const auto timeout = link.transact(request, lab::TlpFault::DropCompletion);
        require(!timeout.completion && timeout.latency_cycles == 32,
                "Missing completion did not cause a model timeout");
        request.address = mmio_base + mmio_size;
        const auto unsupported = link.transact(request);
        require(unsupported.completion &&
                unsupported.completion->status == lab::CompletionStatus::UnsupportedRequest,
                "Out-of-aperture TLP was accepted");

        const auto& metrics = link.statistics();
        require(metrics.requests == 6 && metrics.successful == 4 &&
                metrics.rejected == 1 && metrics.timeouts == 1,
                "Unexpected PCIe TLP diagnostic counters");
        std::cout << "PASS: PCIe TLP config and MMIO transactions drive RTL\n";
        std::cout << "TLP metrics: requests=" << metrics.requests
                  << " successful=" << metrics.successful
                  << " rejected=" << metrics.rejected
                  << " timeouts=" << metrics.timeouts
                  << " synthetic_cycles=" << metrics.total_cycles
                  << " payload_bytes=" << metrics.payload_bytes << "\n";

        std::cout
            << "PASS: PCIe enumeration, BAR programming, MMIO-to-RTL boot\n";

        std::cout
            << "ALL C++ / RTL COSIMULATION TESTS PASSED\n";

        gpu.dut.final();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        gpu.dut.final();
        return 1;
    }
}
