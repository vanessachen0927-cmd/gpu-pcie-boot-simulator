# GPU PCIe Boot Hardware/Software Co-Simulation

[![C++ Build and Validation](https://github.com/vanessachen0927-cmd/gpu-pcie-boot-simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/vanessachen0927-cmd/gpu-pcie-boot-simulator/actions/workflows/ci.yml)
[![RTL Hardware and Co-Simulation](https://github.com/vanessachen0927-cmd/gpu-pcie-boot-simulator/actions/workflows/rtl-ci.yml/badge.svg)](https://github.com/vanessachen0927-cmd/gpu-pcie-boot-simulator/actions/workflows/rtl-ci.yml)

An educational **C++17 + SystemVerilog** lab modeling PCIe GPU discovery, BAR probing and assignment, register-controlled boot, and transaction-level diagnostics. The design includes a synthesizable RTL boot FSM driven through Verilator by a C++ host.

**Not NVIDIA firmware, a hardware PCIe endpoint, or a PCIe specification-conformant implementation.** No real PCIe traffic or GPU hardware is required.

## Architecture

```text
C++ Host / Root Complex
    |-- BDF discovery and 4 KiB configuration-space model
    |-- 32-bit/64-bit memory BAR sizing and MMIO allocation
    |-- v2: single-DWORD transaction model with requester ID / tag
    |       + synthetic latency and completion fault injection
    v
BAR0 address decoding / simulated device register bus
    v
SystemVerilog register interface (BOOT_CONTROL, BOOT_STATUS,
ERROR_CODE, FW_STATUS)
    v
SystemVerilog boot FSM: IDLE -> FW_LOAD -> FW_VERIFY -> READY/ERROR
```

## Quick start

Dependencies: C++17 compiler, CMake, Python 3, and (for RTL tests) Verilator.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tools/validate.py build/gpu_boot
./build/gpu_boot
```

### Hardware/software co-simulation

```bash
mkdir -p build/cosim
verilator --cc --exe --build -Wall -Wno-UNUSEDSIGNAL \
  --top-module gpu_boot_regs --Mdir build/cosim \
  -CFLAGS "-std=c++17 -I$(pwd)/src" \
  rtl/gpu_boot_fsm.sv rtl/gpu_boot_regs.sv \
  tests/rtl_host_driver.cpp src/pcie.cpp
./build/cosim/Vgpu_boot_regs
```

The C++ driver configures the simulated endpoint and issues TLP-style configuration and BAR0 MMIO requests to the Verilator RTL model, including timeout and out-of-range diagnostics.

## Tested capabilities

- BDF device discovery, PCIe capability linked-list traversal, Command register access.
- 32-bit/64-bit BAR sizing-mask probing, alignment and MMIO resource allocation.
- Simulated firmware-validity checks, boot FSM timeout and hardware reset recovery.
- C++ directed tests, SystemVerilog testbenches, cross-language co-simulation.
- **v2:** Config Read/Write, Memory Read/Write (one DWORD), completion status/tag, synthetic cycle counters, model fault injection.
- Reproducible C++ and RTL GitHub Actions workflows.

## Documentation

- [Hardware architecture and register map](docs/HARDWARE_ARCHITECTURE.md)
- [PCIe TLP model, performance assumptions, and limitations](docs/TLP_PERFORMANCE.md)

## Technical limitations

- TLPs are **transaction-level objects**, not serialized PCIe packet bitstreams. No real PCIe PHY, LTSSM, flow-control credits, CRC, DMA or PCIe-compliant packet transport.
- Memory Write completion behavior is simplified; a real PCIe Memory Write is posted.
- Modeled latency values are synthetic cycle costs, **not hardware benchmarks or physical GPU boot latency**.
- Firmware validity is externally injected, not cryptographically verified. No actual firmware images execute.
- The separate capability model and simplified root-complex allocation flow do not claim full PCIe platform compatibility.

This project demonstrates hardware/software architecture, verification, and tradeoff reasoning—not compatibility with actual NVIDIA GPUs.
