// Test cases for the Sony S-DSP ADSR emulator.
//
// Copyright (c) 2020 Christian Kauten
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "dsp/sony_s_dsp/adsr.hpp"
#include "catch_amalgamated.hpp"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// MARK: sizeof Sony_S_DSP_ADSR
// ---------------------------------------------------------------------------

TEST_CASE("Sony_S_DSP_ADSR should be 8 bytes") {
    REQUIRE(8 == sizeof(SonyS_DSP::ADSR));
}

using ADSR = SonyS_DSP::ADSR;
using Stage = ADSR::EnvelopeStage;

TEST_CASE("Release is 256 linear chip ticks with unchanged signed scaling") {
    for (int amplitude : {-128, -63, 0, 1, 63, 127}) for (int rate : {0, 31}) {
        ADSR envelope;
        envelope.setAmplitude(amplitude);
        envelope.setAttack(15);
        envelope.setSustainRate(rate);
        envelope.run(true, true);
        const int peak = envelope.run(false, true);
        REQUIRE(envelope.getStage() == Stage::Decay);
        REQUIRE(peak == (127 * amplitude >> 7));
        int previous = peak;
        for (int tick = 1; tick <= 256; ++tick) {
            const int actual = envelope.run(false, false);
            const int level = std::max(0, 2047 - 8 * tick);
            CHECK(actual == ((level >> 4) * amplitude >> 7));
            CHECK(std::abs(actual) <= std::abs(previous));
            CHECK(envelope.getStage() == (tick < 256 ? Stage::Release : Stage::Off));
            previous = actual;
        }
        for (int tick = 0; tick < 32; ++tick) CHECK(envelope.run(false, false) == 0);
    }
}

TEST_CASE("Key-off releases from attack decay and sustain without a level jump") {
    for (Stage stage : {Stage::Attack, Stage::Decay, Stage::Sustain}) {
        ADSR envelope;
        envelope.setAmplitude(127);
        envelope.setAttack(14);
        envelope.setDecay(7);
        envelope.setSustainLevel(3);
        envelope.setSustainRate(0);
        int previous = envelope.run(true, true);
        int ticks = 0;
        while ((envelope.getStage() != stage || previous < 20) && ticks++ < 10000)
            previous = envelope.run(false, true);
        REQUIRE(ticks < 10000);
        REQUIRE(previous > 0);
        int released = 0;
        do {
            const int value = envelope.run(false, false);
            CHECK(value <= previous);
            CHECK(previous - value <= 1);
            previous = value;
        } while (envelope.getStage() != Stage::Off && ++released < 256);
        REQUIRE(envelope.getStage() == Stage::Off);
        CHECK(previous == 0);
    }
}

TEST_CASE("Sustain rate affects held decay but cannot change fixed key-off") {
    ADSR held, decaying;
    for (auto envelope : {&held, &decaying}) {
        envelope->setAmplitude(127);
        envelope->setAttack(15);
        envelope->setSustainLevel(7);
        envelope->run(true, true);
        envelope->run(false, true);
    }
    held.setSustainRate(0);
    decaying.setSustainRate(31);
    int held_value = 0, decayed_value = 0;
    for (int tick = 0; tick < 5000; ++tick) {
        held_value = held.run(false, true);
        decayed_value = decaying.run(false, true);
    }
    CHECK(held_value > 120);
    CHECK(decayed_value == 0);
    CHECK(decaying.run(false, false) == 0);
    CHECK(decaying.getStage() == Stage::Off);
}

TEST_CASE("Retrigger wins over coincident key-off then low gate releases") {
    ADSR envelope;
    envelope.setAmplitude(127);
    envelope.setAttack(15);
    envelope.run(true, true);
    envelope.run(false, true);
    for (int i = 0; i < 200; ++i) envelope.run(false, false);
    REQUIRE(envelope.getStage() == Stage::Release);
    const int before = envelope.run(false, false);
    CHECK(envelope.run(true, false) > before);
    CHECK(envelope.getStage() == Stage::Attack);
    envelope.run(false, false);
    CHECK(envelope.getStage() == Stage::Release);
    for (int i = 0; i < 256; ++i) envelope.run(false, false);
    CHECK(envelope.getStage() == Stage::Off);
    CHECK(envelope.run(false, false) == 0);
}
