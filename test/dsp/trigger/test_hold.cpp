// Test cases for the Trigger::Hold structure.
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

#include "dsp/trigger/hold.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

SCENARIO("Trigger::Hold processes signals at 100Hz") {
    GIVEN("an initialized trigger and sample rate") {
        float sampleTime = 0.01f;
        Trigger::Hold trigger;
        WHEN("the trigger starts to go high") {
            auto value = trigger.process(1.f, sampleTime);
            THEN("the trigger does not fire and is not held") {
                REQUIRE_FALSE(value);
                REQUIRE_FALSE(trigger.isHeld());
            }
        }
        WHEN("the trigger goes high then low within the press window") {
            trigger.process(1.f, 0.01f);
            auto value = trigger.process(0.f, sampleTime);
            THEN("the trigger fires and is not held") {
                REQUIRE(value);
                REQUIRE_FALSE(trigger.isHeld());
            }
        }
        WHEN("the trigger goes high past the press window") {
            THEN("the trigger does not fire, but is held after the window") {
                float time = 0.f;
                // enter the press stage
                while (time < Trigger::Hold::HOLD_TIME) {
                    time += sampleTime;
                    auto value = trigger.process(1.f, sampleTime);
                    REQUIRE_FALSE(value);
                    REQUIRE_FALSE(trigger.isHeld());
                }
                // enter the held stage for a single sample
                {
                    auto value = trigger.process(1.f, sampleTime);
                    REQUIRE_FALSE(value);
                    REQUIRE(trigger.isHeld());
                }
                // take the trigger low in a single sample
                {
                    auto value = trigger.process(0.f, sampleTime);
                    REQUIRE_FALSE(value);
                    REQUIRE_FALSE(trigger.isHeld());
                }
            }
        }
    }
}
