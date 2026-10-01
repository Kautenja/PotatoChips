// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: MIT
#include <initializer_list>
#include "opm.h"
#include <chrono>
#include <cstdio>
int main() {
    opm_t chips[16];
    for (int n : {1, 4, 16}) {
        for (int i = 0; i < n; ++i) OPM_Reset(&chips[i], 0);
        auto start = std::chrono::steady_clock::now();
        int32_t out[2];
        for (int j = 0; j < 3579545 / 2 / 4; ++j)
            for (int i = 0; i < n; ++i) OPM_Clock(&chips[i], out, nullptr, nullptr, nullptr);
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::printf("%d lanes: %.6f sec per 0.25 audio sec (%.2f%% callback budget)\n", n, sec, sec*400);
    }
}
// This is a deliberately silent lower-bound cost probe. The active, independent
// reference fixtures in test/dsp/yamaha_ym2151/test_voice.cpp verify synthesis.
