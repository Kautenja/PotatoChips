// Test cases for the Sony S-DSP common code.
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

#include "dsp/sony_s_dsp/common.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// ---------------------------------------------------------------------------
// MARK: SonyS_DSP::SourceDirectoryEntry
// ---------------------------------------------------------------------------

TEST_CASE("SonyS_DSP::SourceDirectoryEntry should be 4 bytes") {
    REQUIRE(4 == sizeof(SonyS_DSP::SourceDirectoryEntry));
}

// ---------------------------------------------------------------------------
// MARK: SonyS_DSP::BitRateReductionBlock
// ---------------------------------------------------------------------------

TEST_CASE("SonyS_DSP::BitRateReductionBlock should be 9 bytes") {
    REQUIRE(9 == sizeof(SonyS_DSP::BitRateReductionBlock));
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(1 == sizeof(block.header));
    REQUIRE(8 == sizeof(block.samples));
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should have correct constants") {
    REQUIRE(8 == SonyS_DSP::BitRateReductionBlock::NUM_SAMPLES);
    REQUIRE(12 == SonyS_DSP::BitRateReductionBlock::MAX_VOLUME);
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should set volume") {
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(0x0 == block.header.byte);
    block.header.flags.volume = 0xC;
    REQUIRE(0xC0 == block.header.byte);
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should clip volume") {
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(0x0 == block.header.byte);
    block.header.flags.set_volume(0xF);
    REQUIRE(0xC0 == block.header.byte);
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should set filter mode") {
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(0x0 == block.header.byte);
    block.header.flags.filter = 3;
    REQUIRE(0x0C == block.header.byte);
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should set is_loop") {
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(0x0 == block.header.byte);
    block.header.flags.is_loop = 1;
    REQUIRE(0x02 == block.header.byte);
}

TEST_CASE("SonyS_DSP::BitRateReductionBlock should set is_end") {
    SonyS_DSP::BitRateReductionBlock block;
    REQUIRE(0x0 == block.header.byte);
    block.header.flags.is_end = 1;
    REQUIRE(0x01 == block.header.byte);
}

// ---------------------------------------------------------------------------
// MARK: SonyS_DSP::StereoSample
// ---------------------------------------------------------------------------

TEST_CASE("SonyS_DSP::StereoSample should be 4 bytes") {
    REQUIRE(4 == sizeof(SonyS_DSP::StereoSample));
}

TEST_CASE("SonyS_DSP::StereoSample should have correct constants") {
    REQUIRE(0 == SonyS_DSP::StereoSample::LEFT);
    REQUIRE(1 == SonyS_DSP::StereoSample::RIGHT);
    REQUIRE(2 == SonyS_DSP::StereoSample::CHANNELS);
}
