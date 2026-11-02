# GPU PCIe Boot Lab — Hardware Architecture

## Overview

A hardware/software co-design project modeling PCIe GPU initialization.

The host is written in C++17. The device-side boot controller uses synthesizable SystemVerilog, simulated with Verilator.

## System Architecture

1. C++ PCIe Host / Root Complex
2. BDF Device Enumeration
3. PCIe Configuration Space
4. 32-bit and 64-bit BAR Probing
5. MMIO Resource Allocation
6. C++ MMIO-to-RTL Adapter
7. SystemVerilog Register Interface
8. GPU Boot Finite-State Machine
9. READY / ERROR / TIMEOUT

## GPU Register Map

| Offset | Register | Function |
|---|---|---|
| 0x00 | BOOT_CONTROL | Write bit 0 to start |
| 0x04 | BOOT_STATUS | Ready, error, and FSM state |
| 0x08 | ERROR_CODE | 0=none, 1=invalid firmware, 2=timeout |
| 0x0C | FW_STATUS | Firmware subsystem input status |

## Boot FSM

IDLE -> FW_LOAD -> FW_VERIFY -> READY

Invalid firmware or firmware-loading timeout transitions to ERROR.

Reset returns the controller to IDLE.

## Verification

- C++ PCIe unit and integration tests
- SystemVerilog boot FSM testbench
- SystemVerilog register interface testbench
- Verilator C++/RTL co-simulation
- BAR sizing and MMIO address validation
- Firmware failure, timeout, and reset recovery tests

## Scope and Limitations

This is an educational hardware/software co-simulation.

It does not implement a physical PCIe PHY, real GPU firmware, PCIe TLP transport, DMA, secure boot, or cryptographic firmware verification.

The MMIO adapter is simulated, and the firmware-valid signal is supplied by the test environment.

Simulation execution time is not actual GPU boot latency.

## Technologies

C++17, SystemVerilog, CMake, Verilator, Python, GitHub Actions.
