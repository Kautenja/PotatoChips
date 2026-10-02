// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: MIT
#include <initializer_list>
#include "ymfm_opm.h"
#include <chrono>
#include <cstdio>
#include <memory>
int main() {
    ymfm::ymfm_interface interfaces[16];
    std::unique_ptr<ymfm::ym2151> chips[16];
    for (int i=0;i<16;++i) {
        chips[i].reset(new ymfm::ym2151(interfaces[i]));
        chips[i]->reset();
        auto write=[&](int a,int d){chips[i]->write_address(a); chips[i]->write_data(d);};
        write(0x27,0xc7); write(0x2f,0x4a);
        for(int slot=7;slot<32;slot+=8) { write(0x40+slot,1); write(0x60+slot,16); write(0x80+slot,31); }
        write(8,0x7f);
    }
    for(int n : {1,4,16}) {
        auto start=std::chrono::steady_clock::now();
        ymfm::ym2151::output_data out;
        for(int j=0;j<3579545/64/4;++j) for(int i=0;i<n;++i) chips[i]->generate(&out);
        double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::printf("%d lanes: %.6f sec per 0.25 audio sec (%.2f%% callback budget)\n",n,sec,sec*400);
    }
}
