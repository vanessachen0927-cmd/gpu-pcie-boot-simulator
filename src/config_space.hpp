#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace lab {

class ConfigSpace {
public:
    static constexpr unsigned kSize = 4096;

    ConfigSpace(uint16_t vendor, uint16_t device) {
        data_.fill(0);
        write16_raw(0x00, vendor);
        write16_raw(0x02, device);
    }

    uint16_t read16(unsigned offset) const {
        check(offset, 2);
        return static_cast<uint16_t>(
            data_[offset] |
            (static_cast<uint16_t>(data_[offset + 1]) << 8)
        );
    }

    uint32_t read32(unsigned offset) const {
        check(offset, 4);
        return static_cast<uint32_t>(read16(offset)) |
               (static_cast<uint32_t>(read16(offset + 2)) << 16);
    }

    void write_command(uint16_t value) {
        // Simplified writable command bits:
        // bit 1: Memory Space Enable
        // bit 2: Bus Master Enable
        constexpr uint16_t supported = 0x0006;
        write16_raw(0x04, value & supported);
    }

    bool memory_enabled() const {
        return (read16(0x04) & 0x0002) != 0;
    }

    bool bus_master_enabled() const {
        return (read16(0x04) & 0x0004) != 0;
    }

private:
    std::array<uint8_t, kSize> data_{};

    static void check(unsigned offset, unsigned width) {
        if (offset > kSize || width > kSize - offset ||
            offset % width != 0) {
            throw std::out_of_range(
                "invalid PCIe configuration-space access"
            );
        }
    }

    void write16_raw(unsigned offset, uint16_t value) {
        check(offset, 2);
        data_[offset] = static_cast<uint8_t>(value);
        data_[offset + 1] = static_cast<uint8_t>(value >> 8);
    }
};

} // namespace lab
