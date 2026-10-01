// Host-local before/after audio equivalence probe, not a hardware reference.
// Copyright (c) 2026 Christian Kauten. MIT license; see docs/licenses/MIT-TESTS.txt.
#include "src/dsp/yamaha_ym2612/feedback_operator.hpp"
#include "src/dsp/yamaha_ym2612/voice4op.hpp"
#include <iostream>
using namespace YamahaYM2612;
void hash(uint64_t& h, int16_t sample) {
    h = (h ^ (uint16_t(sample) & 255)) * 1099511628211ULL;
    h = (h ^ (uint16_t(sample) >> 8)) * 1099511628211ULL;
}
int main() {
    std::cout << "rate,soft,loop,algorithm,fnv64\n";
    for (float rate : {44100.f, 48000.f, 96000.f}) for (bool soft : {false, true})
    for (bool loop : {false, true}) for (int algorithm = -1; algorithm < 8; ++algorithm) {
        FeedbackOperator single(rate); Voice4Op voice(rate); voice.set_algorithm(algorithm & 7);
        single.set_attack_rate(15); single.set_decay_rate(31); single.set_sustain_level(10);
        single.set_sustain_rate(7); single.set_release_rate(10); single.set_frequency(115.08f);
        single.set_ssg_enabled(loop); single.set_feedback(7); single.set_fm_sensitivity(7);
        single.set_am_sensitivity(3);
        for (int op = 0; op < 4; ++op) {
            voice.set_attack_rate(op, 15 + op); voice.set_decay_rate(op, 31 - op);
            voice.set_sustain_level(op, 10); voice.set_sustain_rate(op, 7 + op);
            voice.set_release_rate(op, 10); voice.set_frequency(op, 115.08f * (op + 1));
            voice.set_ssg_enabled(op, loop); voice.set_fm_sensitivity(op, 7);
            voice.set_am_sensitivity(op, 3);
        }
        voice.set_feedback(7);
        uint64_t h = 14695981039346656037ULL;
        for (int n = 0; n < 48000; ++n) {
            if (n % 12000 == 0 || n % 12000 == 11000) {
                const bool on = n % 12000 == 0;
                single.set_gate(on, soft);
                for (int op = 0; op < 4; ++op) voice.set_gate(op, on, soft);
            }
            hash(h, algorithm == -1 ? single.step(n % 16384 - 8192) : voice.step());
        }
        std::cout << int(rate) << ',' << soft << ',' << loop << ',' << algorithm << ',' << h << '\n';
    }
}
