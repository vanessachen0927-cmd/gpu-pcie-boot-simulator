# GPU PCIe Boot & Architecture Validation Lab

An educational, specification-inspired C++17 model of GPU endpoint initialization, PCIe link status, firmware-verification *result*, and MMIO BAR allocation. **Not an NVIDIA GPU implementation or real PCIe enumeration stack.**

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tools/validate.py build/gpu_boot
./build/gpu_boot
./build/gpu_boot --link-down
```

## Current scope
- Boot state model: RESET → FIRMWARE_VERIFIED → ENUMERATED → RESOURCES_ASSIGNED → READY, or FAILED.
- Atomic MMIO allocation with power-of-two BAR sizing and aperture checks.
- Five directed C++ tests and three Python CLI scenarios.
- GitHub Actions CI.

## Important model limitations
- `firmware_valid` is a **simulated verification result**, not cryptographic signature verification.
- BAR sizes are provided as inputs; this version does **not** implement real PCIe configuration transactions, BAR sizing probes, BDF scanning, PCIe capability linked lists, link training, or real device hardware.
- `elapsed_us` is simulator runtime, **not PCIe link or boot hardware latency**.
- The sample vendor/device IDs are illustrative and do not imply support for a real GPU.

## Next engineering milestones
1. Define a 4-KiB Type-0 configuration-space abstraction and register read/write semantics.
2. Implement BDF enumeration and absent-device detection via vendor ID `0xffff`.
3. Implement 32-bit/64-bit BAR sizing and resource allocation with rollback.
4. Model standard PCIe capability linked-list traversal and malformed-pointer tests.
5. Add trace events, reproducible benchmark harness, and documented fault matrix.
6. Optional: QEMU/Linux PCIe observation and secure-boot signature verification as separate integrations.

## Design questions for interviews
- Why must BAR allocations be aligned and nonoverlapping?
- What happens if a BAR allocation fails halfway through enumeration?
- How do config-space accesses differ from device MMIO?
- Which parts of boot and PCIe enumeration are firmware-owned vs OS-owned?
