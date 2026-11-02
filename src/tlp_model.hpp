#pragma once

#include "config_space.hpp"
#include "enumeration.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>

namespace lab {

// Transaction-level, single-DWORD teaching model. No wire-format TLP encoding,
// credit management, ordering rules, byte enables, or physical PCIe transport.
enum class TlpKind { ConfigRead0, ConfigWrite0, MemoryRead32, MemoryWrite32 };
enum class CompletionStatus { Successful, UnsupportedRequest, CompleterAbort };
enum class TlpFault { None, DropCompletion, CorruptCompletionTag };

struct TlpRequest {
    TlpKind kind;
    BDF target;
    uint16_t requester_id = 0;
    uint8_t tag = 0;
    uint64_t address = 0; // config-space byte offset OR absolute MMIO address
    uint32_t payload = 0;
    uint16_t length_dw = 1;
};

struct TlpCompletion {
    CompletionStatus status = CompletionStatus::UnsupportedRequest;
    uint16_t requester_id = 0;
    uint8_t tag = 0;
    uint32_t payload = 0;
};

struct TlpResult {
    std::optional<TlpCompletion> completion;
    uint32_t latency_cycles = 0;
};

struct TlpStatistics {
    uint64_t requests = 0;
    uint64_t completed = 0;
    uint64_t successful = 0;
    uint64_t rejected = 0;
    uint64_t timeouts = 0;
    uint64_t total_cycles = 0;
    uint64_t payload_bytes = 0;

    double average_latency_cycles() const {
        return completed ? static_cast<double>(total_cycles) / requests : 0.0;
    }
};

class TlpEndpoint {
public:
    using ReadRegister = std::function<uint32_t(uint32_t)>;
    using WriteRegister = std::function<void(uint32_t, uint32_t)>;

    TlpEndpoint(BDF bdf, ConfigSpace& config, uint64_t bar0_base,
                uint64_t bar0_size, ReadRegister read_reg, WriteRegister write_reg)
        : bdf_(bdf), config_(config), bar_base_(bar0_base),
          bar_size_(bar0_size), read_(std::move(read_reg)),
          write_(std::move(write_reg)) {
        if (bar_size_ < 16 || bar_base_ > UINT64_MAX - bar_size_)
            throw std::invalid_argument("invalid BAR aperture");
    }

    TlpCompletion handle(const TlpRequest& req) {
        TlpCompletion c{CompletionStatus::UnsupportedRequest,
                        req.requester_id, req.tag, 0};
        if (req.target.bus != bdf_.bus || req.target.device != bdf_.device ||
            req.target.function != bdf_.function)
            return c;

        if (req.length_dw != 1 || (req.address & 3u))
            return c;

        try {
            if (req.kind == TlpKind::ConfigRead0 ||
                req.kind == TlpKind::ConfigWrite0) {
                if (req.address > 4092)
                    return c;
                const unsigned off = static_cast<unsigned>(req.address);
                if (req.kind == TlpKind::ConfigRead0) {
                    c.payload = off >= 0x10 && off <= 0x24
                        ? config_.read_bar((off - 0x10) / 4)
                        : config_.read32(off);
                } else {
                    // Only Command register writes are supported in this
                    // teaching model. BAR sizing/programming is the existing
                    // root-complex initialization path.
                    if (off != 0x04)
                        return c;
                    config_.write_command(static_cast<uint16_t>(req.payload));
                }
                c.status = CompletionStatus::Successful;
                return c;
            }
            if (!config_.memory_enabled() || req.address < bar_base_ ||
                req.address - bar_base_ > bar_size_ - 4)
                return c;
            const uint64_t offset = req.address - bar_base_;
            if (offset > 0x0c) // Four documented device boot registers only
                return c;
            if (req.kind == TlpKind::MemoryRead32)
                c.payload = read_(static_cast<uint32_t>(offset));
            else if (req.kind == TlpKind::MemoryWrite32)
                write_(static_cast<uint32_t>(offset), req.payload);
            else
                return c;
            c.status = CompletionStatus::Successful;
        } catch (const std::exception&) {
            c.status = CompletionStatus::CompleterAbort;
        }
        return c;
    }

private:
    BDF bdf_;
    ConfigSpace& config_;
    uint64_t bar_base_;
    uint64_t bar_size_;
    ReadRegister read_;
    WriteRegister write_;
};

class TlpLink {
public:
    explicit TlpLink(TlpEndpoint& endpoint) : endpoint_(endpoint) {}

    // Synthetic deterministic costs, NOT measured PCIe latency.
    // Config=5 cycles, MMIO=3 cycles, timeout=32 cycles.
    TlpResult transact(const TlpRequest& req, TlpFault fault = TlpFault::None) {
        const bool config = req.kind == TlpKind::ConfigRead0 ||
                            req.kind == TlpKind::ConfigWrite0;
        const uint32_t cycles = fault == TlpFault::DropCompletion
            ? 32u : (config ? 5u : 3u);
        ++stats_.requests;
        stats_.total_cycles += cycles;
        if (fault == TlpFault::DropCompletion) {
            ++stats_.timeouts;
            return {std::nullopt, cycles};
        }
        TlpCompletion c = endpoint_.handle(req);
        if (fault == TlpFault::CorruptCompletionTag)
            c.tag ^= 0x01;
        ++stats_.completed;
        if (c.status == CompletionStatus::Successful) {
            ++stats_.successful;
            if (req.kind == TlpKind::MemoryRead32 ||
                req.kind == TlpKind::MemoryWrite32)
                stats_.payload_bytes += 4;
        } else
            ++stats_.rejected;
        return {c, cycles};
    }

    const TlpStatistics& statistics() const { return stats_; }
    void clear_statistics() { stats_ = {}; }

private:
    TlpEndpoint& endpoint_;
    TlpStatistics stats_{};
};

} // namespace lab
