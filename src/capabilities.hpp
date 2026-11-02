#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <vector>

namespace lab {

struct Capability {
    uint8_t offset;
    uint8_t id;
    uint8_t next;
};

class CapabilitySpace {
public:
    CapabilitySpace() {
        data_.fill(0);
    }

    void set_head(uint8_t offset) {
        data_[0x34] = offset;
    }

    void add(uint8_t offset, uint8_t id, uint8_t next) {
        validate(offset);
        if (next != 0) validate(next);

        data_[offset] = id;
        data_[offset + 1] = next;
    }

    std::vector<Capability> discover() const {
        std::vector<Capability> result;
        std::set<uint8_t> visited;

        uint8_t current = data_[0x34];

        while (current != 0) {
            validate(current);

            if (!visited.insert(current).second) {
                throw std::runtime_error(
                    "circular PCIe capability list"
                );
            }

            const uint8_t id = data_[current];
            const uint8_t next = data_[current + 1];

            result.push_back({current, id, next});
            current = next;
        }

        return result;
    }

private:
    std::array<uint8_t, 256> data_{};

    static void validate(uint8_t offset) {
        if (offset < 0x40 || offset > 0xFC ||
            (offset % 4) != 0) {
            throw std::out_of_range(
                "invalid PCIe capability pointer"
            );
        }
    }
};

} // namespace lab
