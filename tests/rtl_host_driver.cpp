#include "Vgpu_boot_regs.h"
#include "verilated.h"

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
