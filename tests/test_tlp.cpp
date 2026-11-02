#include "tlp_model.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace lab;

int main() {
    ConfigSpace cfg(0x10de, 0x2684);
    cfg.configure_bar(0, 0x1000);
    cfg.write_bar(0, 0x80000000u);
    cfg.write_command(0x0006);
    uint32_t control = 0, status = 0x1;

    TlpEndpoint endpoint({0,1,0}, cfg, 0x80000000ULL, 4096,
        [&](uint32_t off) -> uint32_t {
            if (off == 4) return status;
            return control;
        },
        [&](uint32_t off, uint32_t val) {
            if (off == 0) { control = val; status = 1; }
        });
    TlpLink link(endpoint);
    TlpRequest req{TlpKind::ConfigRead0, {0,1,0}, 0x1234, 7, 0x00};
    auto result = link.transact(req);
    assert(result.completion);
    assert(result.completion->status == CompletionStatus::Successful);
    assert(result.completion->payload == 0x268410deu);
    assert(result.completion->requester_id == 0x1234);
    assert(result.completion->tag == 7);
    assert(result.latency_cycles == 5);
    std::cout << "PASS: config read completion and requester/tag routing\n";

    req.kind = TlpKind::ConfigWrite0;
    req.address = 4;
    req.payload = 0x0002;
    result = link.transact(req);
    assert(result.completion && result.completion->status == CompletionStatus::Successful);
    assert(cfg.memory_enabled() && !cfg.bus_master_enabled());
    cfg.write_command(0x0006);
    std::cout << "PASS: config command write\n";

    req.kind = TlpKind::MemoryWrite32;
    req.address = 0x80000000ULL;
    req.payload = 1;
    result = link.transact(req);
    assert(result.completion && result.completion->status == CompletionStatus::Successful);
    assert(result.latency_cycles == 3 && control == 1);
    req.kind = TlpKind::MemoryRead32;
    req.address = 0x80000004ULL;
    result = link.transact(req);
    assert(result.completion && result.completion->payload == 1);
    std::cout << "PASS: BAR-decoded memory requests\n";

    req.address = 0x80001000ULL;
    result = link.transact(req);
    assert(result.completion &&
           result.completion->status == CompletionStatus::UnsupportedRequest);
    req.address = 0x80000005ULL;
    result = link.transact(req);
    assert(result.completion &&
           result.completion->status == CompletionStatus::UnsupportedRequest);
    req.address = 0x80000004ULL;
    req.length_dw = 2;
    result = link.transact(req);
    assert(result.completion &&
           result.completion->status == CompletionStatus::UnsupportedRequest);
    req.length_dw = 1;
    req.target = {0,2,0};
    result = link.transact(req);
    assert(result.completion &&
           result.completion->status == CompletionStatus::UnsupportedRequest);
    req.target = {0,1,0};
    std::cout << "PASS: aperture, alignment, length and BDF rejection\n";

    const auto before = link.statistics().requests;
    result = link.transact(req, TlpFault::DropCompletion);
    assert(!result.completion && result.latency_cycles == 32);
    assert(link.statistics().requests == before + 1);
    assert(link.statistics().timeouts == 1);
    result = link.transact(req, TlpFault::CorruptCompletionTag);
    assert(result.completion && result.completion->tag != req.tag);
    std::cout << "PASS: timeout and completion-tag fault injection\n";

    const auto& stats = link.statistics();
    assert(stats.requests == 10);
    assert(stats.completed == 9);
    assert(stats.successful == 5);
    assert(stats.rejected == 4);
    assert(stats.timeouts == 1);
    assert(stats.payload_bytes == 12);
    assert(stats.total_cycles == 5 * 2 + 3 * 7 + 32);
    assert(std::abs(stats.average_latency_cycles() -
                    (static_cast<double>(stats.total_cycles) / stats.requests)) < 0.00001);
    std::cout << "PASS: deterministic latency, counters and payload accounting\n";
    std::cout << "transactions=" << stats.requests
              << " successful=" << stats.successful
              << " rejected=" << stats.rejected
              << " timeouts=" << stats.timeouts
              << " synthetic_cycles=" << stats.total_cycles
              << " payload_bytes=" << stats.payload_bytes << "\n";
    std::cout << "ALL TLP MODEL TESTS PASSED\n";
}
