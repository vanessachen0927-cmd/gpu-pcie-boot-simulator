#include "pcie.hpp"
#include <limits>
#include <stdexcept>
namespace lab {
namespace {
bool power_of_two(uint64_t x) { return x && !(x & (x-1)); }
uint64_t align_up(uint64_t x, uint64_t alignment) {
    if (x > std::numeric_limits<uint64_t>::max() - (alignment-1)) throw std::overflow_error("alignment overflow");
    return (x + alignment-1) & ~(alignment-1);
}
}
RootComplex::RootComplex(uint64_t base, uint64_t limit) : cursor_(base), limit_(limit) {
    if (base >= limit) throw std::invalid_argument("invalid MMIO aperture");
}
BootReport RootComplex::initialize(Endpoint& ep) {
    ep.state = BootState::Reset;
    auto fail = [&](std::string reason) { ep.state = BootState::Failed; return BootReport{false, reason, 0, ep.state}; };
    if (!ep.link_up) return fail("PCIe link down");
    if (!ep.firmware_valid) return fail("firmware verification failed");
    ep.state = BootState::FirmwareVerified;
    if (ep.vendor_id == 0xffff) return fail("no device present");
    ep.state = BootState::Enumerated;
    uint64_t candidate = cursor_;
    std::vector<uint64_t> assigned;
    for (const auto& bar : ep.bars) {
        if (!power_of_two(bar.size) || bar.size < 16) return fail("invalid BAR size");
        try { candidate = align_up(candidate, bar.size); }
        catch (const std::overflow_error&) { return fail("MMIO address overflow"); }
        if (candidate > limit_ || bar.size > limit_ - candidate) return fail("MMIO aperture exhausted");
        assigned.push_back(candidate);
        candidate += bar.size;
    }
    for (size_t i=0;i<assigned.size();++i) ep.bars[i].address=assigned[i];
    ep.state = BootState::ResourcesAssigned;
    const uint64_t allocated = candidate-cursor_;
    cursor_ = candidate;
    ep.state = BootState::Ready;
    return {true,"GPU endpoint initialized",allocated,ep.state};
}
std::string state_name(BootState s) {
    switch(s) {
        case BootState::Reset:return "RESET";
        case BootState::FirmwareVerified:return "FIRMWARE_VERIFIED";
        case BootState::Enumerated:return "ENUMERATED";
        case BootState::ResourcesAssigned:return "RESOURCES_ASSIGNED";
        case BootState::Ready:return "READY";
        case BootState::Failed:return "FAILED";
    }
    return "UNKNOWN";
}
}
