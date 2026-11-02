#pragma once

#include "config_space.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace lab {

struct BDF {
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    bool operator<(const BDF& other) const {
        if (bus != other.bus) return bus < other.bus;
        if (device != other.device) return device < other.device;
        return function < other.function;
    }
};

struct DiscoveredDevice {
    BDF address;
    uint16_t vendor_id;
    uint16_t device_id;
};

class PcieFabric {
public:
    void attach(BDF address, const ConfigSpace& config) {
        devices_.insert_or_assign(address, config);
    }

    ConfigSpace* find(BDF address) {
        auto it = devices_.find(address);
        return it == devices_.end() ? nullptr : &it->second;
    }

    const ConfigSpace* find(BDF address) const {
        auto it = devices_.find(address);
        return it == devices_.end() ? nullptr : &it->second;
    }

    std::vector<DiscoveredDevice> enumerate() const {
        std::vector<DiscoveredDevice> result;

        for (uint16_t bus = 0; bus <= 255; ++bus) {
            for (uint8_t dev = 0; dev < 32; ++dev) {
                for (uint8_t fn = 0; fn < 8; ++fn) {
                    BDF address{
                        static_cast<uint8_t>(bus), dev, fn
                    };

                    auto it = devices_.find(address);
                    if (it == devices_.end()) continue;

                    const auto vendor = it->second.read16(0x00);
                    if (vendor == 0xFFFF) continue;

                    result.push_back({
                        address,
                        vendor,
                        it->second.read16(0x02)
                    });
                }
            }
        }

        return result;
    }

private:
    std::map<BDF, ConfigSpace> devices_;
};

} // namespace lab
