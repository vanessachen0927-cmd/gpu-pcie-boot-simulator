# PCIe TLP and Performance Model (v2.0)

## Purpose

This is an **educational transaction-level subset** of PCI Express, not a compliant PCIe controller, physical transport, or PCIe wire-format serializer.

The C++17 requester models single-DWORD configuration and memory requests. The endpoint checks the target BDF, DWORD alignment, BAR0 aperture, and Memory Space Enable. Completions carry the requester ID, tag, status and read payload. **For simplicity, Memory Writes also return a completion in this model, unlike real posted PCIe Memory Writes.** Firmware-validity remains test-driven.

## Transaction support

| Request / fault | Modeled behavior |
| --- | --- |
| Config Read Type 0 | Read 32-bit configuration values, including programmed BARs |
| Config Write Type 0 | Write modeled PCI Command register only |
| Memory Read / Write | 32-bit access to four BAR0-mapped boot registers |
| Unsupported Request | Reject wrong BDF, alignment, length, offset, disabled decoding, or BAR mismatch |
| Completer Abort | Report a register callback exception |
| Dropped completion | Synthetic timeout after 32 cycles |
| Tag corruption | Alter completion tag to exercise diagnostics |

No serialized TLP headers, credit flow, multi-DWORD payload streaming, DMA, replay, CRC, real read completion splitting, or LTSSM training is implemented.

## Synthetic performance metrics

All timings are **deterministic, invented model parameters**, not measured PCIe timing or hardware boot latency.

| Operation | Synthetic cycles |
| --- | ---: |
| Configuration transaction | 5 |
| Memory transaction | 3 |
| Dropped completion / timeout | 32 |

Counters: requests, completions, successes, rejections, timeouts, total synthetic cycles, request-average synthetic latency, and successful MMIO payload bytes. These support reproducible model comparisons; they are not throughput/bandwidth benchmarks. Dropped-completion injection suppresses transaction execution in this simplified version.

## Reproduce

Run the C++ CTest suite with CMake. Run the co-simulation using Verilator as documented in GitHub Actions `.github/workflows/rtl-ci.yml`. It builds the C++ host driver together with the RTL register interface and boot FSM.

## Next possible milestones

More faithful TLP header fields and encoding, posted/nonposted semantics, byte enables, outstanding tag tracking, completion timeout/error semantics, capability integration, LTSSM link states, and FPGA-friendly endpoint interfaces.
