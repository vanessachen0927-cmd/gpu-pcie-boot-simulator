#include "pcie.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace lab;
    {
        RootComplex rc;
        Endpoint gpu{0x10de,0x1234,0,1,0,{{4096},{8192}},{0x10}};
        auto r=rc.initialize(gpu);
        assert(r.success && gpu.state==BootState::Ready);
        assert(gpu.bars[0].address==0x80000000ULL);
        assert(gpu.bars[1].address==0x80002000ULL);
    }
    {
        RootComplex rc;
        Endpoint gpu{0x10de,0x1234,0,1,0,{{4096}}}; gpu.link_up=false;
        auto r=rc.initialize(gpu); assert(!r.success && gpu.state==BootState::Failed);
    }
    {
        RootComplex rc;
        Endpoint gpu{0x10de,0x1234,0,1,0,{{4096}}}; gpu.firmware_valid=false;
        auto r=rc.initialize(gpu); assert(!r.success);
    }
    {
        RootComplex rc(0x80000000ULL,0x80001000ULL);
        Endpoint gpu{0x10de,0x1234,0,1,0,{{8192}}};
        auto r=rc.initialize(gpu); assert(!r.success && rc.next_mmio()==0x80000000ULL);
    }
    {
        RootComplex rc;
        Endpoint gpu{0x10de,0x1234,0,1,0,{{3000}}};
        assert(!rc.initialize(gpu).success);
    }
    std::cout<<"5 directed tests passed\n";
}
