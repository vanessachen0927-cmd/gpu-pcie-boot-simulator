#pragma once

#include "config_space.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace lab {

struct ProbedBar {
    uint64_t size;
    bool is64;
};

inline ProbedBar probe_memory_bar(ConfigSpace& config, unsigned index) {
    if (index >= 6)
        throw std::out_of_range("invalid BAR index");

    const uint32_t original_low = config.read_bar(index);

    // This helper supports memory BARs only.
    if ((original_low & 0x1u) != 0)
        throw std::runtime_error("I/O BAR not supported");

    const bool is64 = (original_low & 0x6u) == 0x4u;

    if (is64 && index == 5)
        throw std::runtime_error("64-bit BAR exceeds configuration slots");

    const uint32_t original_high =
        is64 ? config.read_bar(index + 1) : 0;

    // Probe by writing all ones, then reading the sizing mask.
    config.write_bar(index, 0xFFFFFFFFu);
    if (is64)
        config.write_bar(index + 1, 0xFFFFFFFFu);

    const uint32_t low_mask = config.read_bar(index);
    const uint32_t high_mask =
        is64 ? config.read_bar(index + 1) : 0;

    // Restore the original registers before returning.
    config.write_bar(index, original_low);
    if (is64)
        config.write_bar(index + 1, original_high);

    const uint64_t mask = is64
        ? (static_cast<uint64_t>(high_mask) << 32) |
              (low_mask & 0xFFFFFFF0u)
        : static_cast<uint64_t>(low_mask & 0xFFFFFFF0u);

    const uint64_t size = is64
        ? (~mask + 1)
        : (static_cast<uint32_t>(~mask) + 1ULL);

    if (size < 16 || (size & (size - 1)) != 0)
        throw std::runtime_error("invalid BAR sizing response");

    return {size, is64};
}

} // namespace lab
