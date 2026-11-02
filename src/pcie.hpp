#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lab {
enum class BootState { Reset, FirmwareVerified, Enumerated, ResourcesAssigned, Ready, Failed };
struct Bar { uint64_t size; uint64_t address = 0; bool is64 = false; };
struct Endpoint {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus = 0, device = 0, function = 0;
    std::vector<Bar> bars;
    std::vector<uint8_t> capabilities;
    bool firmware_valid = true;
    bool link_up = true;
    BootState state = BootState::Reset;
};
struct BootReport {
    bool success;
    std::string message;
    uint64_t assigned_bytes;
    BootState state;
};
class RootComplex {
public:
    explicit RootComplex(uint64_t mmio_base = 0x80000000ULL, uint64_t mmio_limit = 0x90000000ULL);
    BootReport initialize(Endpoint& ep);
    uint64_t next_mmio() const { return cursor_; }
private:
    uint64_t cursor_, limit_;
};
std::string state_name(BootState state);
}
