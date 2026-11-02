#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace lab {

struct BarResource {
    uint64_t size;
    uint64_t address = 0;
    bool assigned = false;
};

class BarAllocator {
public:
    explicit BarAllocator(uint64_t base, uint64_t limit)
        : next_(base), limit_(limit) {
        if (base >= limit) {
            throw std::invalid_argument("invalid MMIO window");
        }
    }

    uint64_t allocate(uint64_t size) {
        if (size == 0 || (size & (size - 1)) != 0) {
            throw std::invalid_argument(
                "BAR size must be a power of two"
            );
        }

        if (size > limit_) {
            throw std::overflow_error("BAR allocation overflow");
        }

        const uint64_t mask = size - 1;

        if (next_ > UINT64_MAX - mask) {
            throw std::overflow_error("address overflow");
        }

        const uint64_t aligned = (next_ + mask) & ~mask;

        if (aligned > limit_ || size > limit_ - aligned) {
            throw std::overflow_error("MMIO window exhausted");
        }

        next_ = aligned + size;
        return aligned;
    }

private:
    uint64_t next_;
    uint64_t limit_;
};

} // namespace lab
