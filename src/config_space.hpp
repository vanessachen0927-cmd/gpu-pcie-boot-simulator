#pragma once

#include <array>
#include <cstdint>
#include <limits>
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

    // Configure a modeled memory BAR.
    // A 64-bit BAR occupies two consecutive 32-bit slots.
    void configure_bar(unsigned index, uint64_t size, bool is64 = false) {
        if (index >= 6 || (is64 && index == 5))
            throw std::out_of_range("invalid BAR index");

        if (size < 16 || (size & (size - 1)) != 0)
            throw std::invalid_argument("invalid BAR size");

        if (!is64 && size > (1ULL << 32))
            throw std::invalid_argument("32-bit BAR size too large");

        if (bar_sizes_[index] != 0 ||
            (index > 0 && bar_64_[index - 1]))
            throw std::invalid_argument("BAR slot already occupied");

        if (is64 && bar_sizes_[index + 1] != 0)
            throw std::invalid_argument("64-bit BAR overlaps another BAR");

        bar_sizes_[index] = size;
        bar_64_[index] = is64;

        write32_raw(0x10 + index * 4, is64 ? 0x4 : 0x0);

        if (is64)
            write32_raw(0x10 + (index + 1) * 4, 0);
    }

    uint32_t read_bar(unsigned index) const {
        if (index >= 6)
            throw std::out_of_range("invalid BAR index");

        unsigned owner = index;
        const bool upper = index > 0 && bar_64_[index - 1];

        if (upper)
            owner = index - 1;

        const uint64_t size = bar_sizes_[owner];

        if (size == 0)
            return 0;

        const unsigned offset = 0x10 + index * 4;
        const uint32_t stored = read32(offset);

        if (stored != 0xFFFFFFFFu)
            return stored;

        const uint64_t mask = ~(size - 1);

        if (upper)
            return static_cast<uint32_t>(mask >> 32);

        return static_cast<uint32_t>(mask) & 0xFFFFFFF0u |
               (bar_64_[owner] ? 0x4u : 0x0u);
    }

    void write_bar(unsigned index, uint32_t value) {
        if (index >= 6)
            throw std::out_of_range("invalid BAR index");

        const bool upper = index > 0 && bar_64_[index - 1];
        const unsigned owner = upper ? index - 1 : index;

        if (bar_sizes_[owner] == 0)
            throw std::invalid_argument("BAR not configured");

        const uint32_t attributes =
            !upper && bar_64_[owner] ? 0x4u : 0x0u;

        // Preserve memory BAR type bits, except during sizing probe.
        const uint32_t stored =
            value == 0xFFFFFFFFu ? value :
            upper ? value : ((value & 0xFFFFFFF0u) | attributes);

        write32_raw(0x10 + index * 4, stored);
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
    std::array<uint64_t, 6> bar_sizes_{};
    std::array<bool, 6> bar_64_{};

    void write32_raw(unsigned offset, uint32_t value) {
        write16_raw(offset, static_cast<uint16_t>(value));
        write16_raw(offset + 2, static_cast<uint16_t>(value >> 16));
    }

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
