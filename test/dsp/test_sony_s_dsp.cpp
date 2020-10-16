// Test cases for the Sony S-DSP emulator.
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

#include "dsp/sony_s_dsp.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// ---------------------------------------------------------------------------
// MARK: Sony_S_DSP::GlobalData
// ---------------------------------------------------------------------------

TEST_CASE("Sony_S_DSP::GlobalData should be the size of NUM_REGISTERS") {
    REQUIRE(Sony_S_DSP::NUM_REGISTERS == sizeof(Sony_S_DSP::GlobalData));
}

// ---------------------------------------------------------------------------
// MARK: Sony_S_DSP::RawVoice
// ---------------------------------------------------------------------------

TEST_CASE("Sony_S_DSP::RawVoice should be NUM_REGISTERS / VOICE_COUNT bytes") {
    REQUIRE(Sony_S_DSP::NUM_REGISTERS / Sony_S_DSP::VOICE_COUNT == sizeof(Sony_S_DSP::RawVoice));
    Sony_S_DSP::RawVoice voices[Sony_S_DSP::VOICE_COUNT];
    REQUIRE(Sony_S_DSP::NUM_REGISTERS == sizeof(voices));
}

// ---------------------------------------------------------------------------
// MARK: SourceDirectoryEntry
// ---------------------------------------------------------------------------

TEST_CASE("SourceDirectoryEntry should be 4 bytes") {
    REQUIRE(4 == sizeof(SourceDirectoryEntry));
}

// ---------------------------------------------------------------------------
// MARK: BitRateReductionBlock
// ---------------------------------------------------------------------------

TEST_CASE("BitRateReductionBlock should be 9 bytes") {
    REQUIRE(9 == sizeof(BitRateReductionBlock));
    BitRateReductionBlock block;
    REQUIRE(1 == sizeof(block.header));
    REQUIRE(8 == sizeof(block.samples));
}

TEST_CASE("BitRateReductionBlock should have correct constants") {
    REQUIRE(8 == BitRateReductionBlock::NUM_SAMPLES);
    REQUIRE(12 == BitRateReductionBlock::MAX_VOLUME);
}

TEST_CASE("BitRateReductionBlock should set volume") {
    BitRateReductionBlock block;
    REQUIRE(0x0 == block.header);
    block.flags.volume = 0xC;
    REQUIRE(0xC0 == block.header);
}

TEST_CASE("BitRateReductionBlock should clip volume") {
    BitRateReductionBlock block;
    REQUIRE(0x0 == block.header);
    block.flags.set_volume(0xF);
    REQUIRE(0xC0 == block.header);
}

TEST_CASE("BitRateReductionBlock should set filter mode") {
    BitRateReductionBlock block;
    REQUIRE(0x0 == block.header);
    block.flags.filter = 3;
    REQUIRE(0x0C == block.header);
}

TEST_CASE("BitRateReductionBlock should set is_loop") {
    BitRateReductionBlock block;
    REQUIRE(0x0 == block.header);
    block.flags.is_loop = 1;
    REQUIRE(0x02 == block.header);
}

TEST_CASE("BitRateReductionBlock should set is_end") {
    BitRateReductionBlock block;
    REQUIRE(0x0 == block.header);
    block.flags.is_end = 1;
    REQUIRE(0x01 == block.header);
}
