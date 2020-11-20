// Test cases for the Sony S-DSP processor.
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

#include "dsp/sony_s_dsp/processor.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// ---------------------------------------------------------------------------
// MARK: SonyS_DSP::Processor::GlobalData
// ---------------------------------------------------------------------------

TEST_CASE("SonyS_DSP::Processor::GlobalData should be the size of NUM_REGISTERS") {
    REQUIRE(SonyS_DSP::Processor::NUM_REGISTERS == sizeof(SonyS_DSP::Processor::GlobalData));
}

// ---------------------------------------------------------------------------
// MARK: SonyS_DSP::Processor::RawVoice
// ---------------------------------------------------------------------------

TEST_CASE("SonyS_DSP::Processor::RawVoice should be NUM_REGISTERS / VOICE_COUNT bytes") {
    REQUIRE(SonyS_DSP::Processor::NUM_REGISTERS / SonyS_DSP::Processor::VOICE_COUNT == sizeof(SonyS_DSP::Processor::RawVoice));
    SonyS_DSP::Processor::RawVoice voices[SonyS_DSP::Processor::VOICE_COUNT];
    REQUIRE(SonyS_DSP::Processor::NUM_REGISTERS == sizeof(voices));
}
