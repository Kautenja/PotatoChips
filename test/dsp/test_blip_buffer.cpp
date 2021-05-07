// Test cases and behaviors for BlipBuffer.
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

#include "dsp/blip_buffer.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// ---------------------------------------------------------------------------
// MARK:
// ---------------------------------------------------------------------------

SCENARIO("Set sample rate and clock rate") {
    GIVEN("an initialized buffer") {
        BLIPBuffer buffer;
        WHEN("the sample rate is set to 44.1kHz with a clock rate of 44.1kHz") {
            buffer.set_sample_rate(44100, 44100);
            THEN("the rates are set") {
                REQUIRE(44100 == buffer.get_sample_rate());
                REQUIRE(44100 == buffer.get_clock_rate());
            }
        }

        WHEN("the sample rate is set to 44.1kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(44100, 768000);
            THEN("the rates are set") {
                REQUIRE(44100 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 48kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(48000, 768000);
            THEN("the rates are set") {
                REQUIRE(48000 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 88.2kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(88200, 768000);
            THEN("the rates are set") {
                REQUIRE(88200 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 96kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(96000, 768000);
            THEN("the rates are set") {
                REQUIRE(96000 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 176.4kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(176400, 768000);
            THEN("the rates are set") {
                REQUIRE(176400 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 192kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(192000, 768000);
            THEN("the rates are set") {
                REQUIRE(192000 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 352.8kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(352800, 768000);
            THEN("the rates are set") {
                REQUIRE(352800 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 384kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(384000, 768000);
            THEN("the rates are set") {
                REQUIRE(384000 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 705.6kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(705600, 768000);
            THEN("the rates are set") {
                REQUIRE(705600 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
        WHEN("the sample rate is set to 768kHz with a clock rate of 768kHz") {
            buffer.set_sample_rate(768000, 768000);
            THEN("the rates are set") {
                REQUIRE(768000 == buffer.get_sample_rate());
                // REQUIRE(768000 == buffer.get_clock_rate());
            }
        }
    }
}
