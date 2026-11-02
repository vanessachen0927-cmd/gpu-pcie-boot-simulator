#include "pcie.hpp"
#include <chrono>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    lab::Endpoint gpu{0x10de,0x2684,0,1,0,{{0x1000,0,false},{0x2000,0,true}},{0x01,0x10}};
    if (argc>1 && std::string(argv[1])=="--link-down") gpu.link_up=false;
    if (argc>1 && std::string(argv[1])=="--bad-firmware") gpu.firmware_valid=false;
    lab::RootComplex root;
    const auto start=std::chrono::steady_clock::now();
    const auto result=root.initialize(gpu);
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"state="<<lab::state_name(result.state)<<" success="<<result.success
             <<" message=\""<<result.message<<"\" elapsed_us="<<us<<"\n";
    for (size_t i=0;i<gpu.bars.size();++i)
        std::cout<<"BAR"<<i<<" address=0x"<<std::hex<<gpu.bars[i].address<<std::dec<<" size="<<gpu.bars[i].size<<"\n";
    return result.success ? 0 : 2;
}
