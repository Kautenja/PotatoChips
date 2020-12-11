// Test cases for the triggers code.
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

#include "dsp/triggers.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// ---------------------------------------------------------------------------
// MARK: BooleanTrigger
// ---------------------------------------------------------------------------

TEST_CASE("BooleanTrigger should be false when initialized") {
    BooleanTrigger trigger;
    REQUIRE_FALSE(trigger.isHigh());
}

TEST_CASE("BooleanTrigger should be false when initialized and reset") {
    BooleanTrigger trigger;
    trigger.reset();
    REQUIRE_FALSE(trigger.isHigh());
}

TEST_CASE("BooleanTrigger should be false when high and reset") {
    BooleanTrigger trigger;
    trigger.process(true);
    REQUIRE(trigger.isHigh());
    trigger.reset();
    REQUIRE_FALSE(trigger.isHigh());
}

SCENARIO("BooleanTrigger processes a signal") {
    GIVEN("a trigger") {
        BooleanTrigger trigger;
        WHEN("the signal goes from low to low") {
            trigger.process(false);
            bool value = trigger.process(false);
            THEN("the trigger does not fire and state is low") {
                REQUIRE_FALSE(value);
                REQUIRE_FALSE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from low to high") {
            trigger.process(false);
            bool value = trigger.process(true);
            THEN("the trigger fires and state is high") {
                REQUIRE(value);
                REQUIRE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from high to high") {
            trigger.process(true);
            bool value = trigger.process(true);
            THEN("the trigger does not fire and state is high") {
                REQUIRE_FALSE(value);
                REQUIRE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from high to low") {
            trigger.process(true);
            bool value = trigger.process(false);
            THEN("the trigger does not fire and state is high") {
                REQUIRE_FALSE(value);
                REQUIRE_FALSE(trigger.isHigh());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// MARK: ThresholdTrigger
// ---------------------------------------------------------------------------

TEST_CASE("ThresholdTrigger should be false when initialized") {
    ThresholdTrigger trigger;
    REQUIRE_FALSE(trigger.isHigh());
}

TEST_CASE("ThresholdTrigger should be false when initialized and reset") {
    ThresholdTrigger trigger;
    trigger.reset();
    REQUIRE_FALSE(trigger.isHigh());
}

TEST_CASE("ThresholdTrigger should be false when high and reset") {
    ThresholdTrigger trigger;
    trigger.process(1.f);
    REQUIRE(trigger.isHigh());
    trigger.reset();
    REQUIRE_FALSE(trigger.isHigh());
}

SCENARIO("ThresholdTrigger processes a binary signal") {
    GIVEN("a trigger") {
        ThresholdTrigger trigger;
        WHEN("the signal goes from low to low") {
            trigger.process(0.f);
            bool value = trigger.process(0.f);
            THEN("the trigger does not fire and state is low") {
                REQUIRE_FALSE(value);
                REQUIRE_FALSE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from low to high") {
            trigger.process(0.f);
            bool value = trigger.process(1.f);
            THEN("the trigger fires and state is high") {
                REQUIRE(value);
                REQUIRE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from high to high") {
            trigger.process(1.f);
            bool value = trigger.process(1.f);
            THEN("the trigger does not fire and state is high") {
                REQUIRE_FALSE(value);
                REQUIRE(trigger.isHigh());
            }
        }
        WHEN("the signal goes from high to low") {
            trigger.process(1.f);
            bool value = trigger.process(0.f);
            THEN("the trigger does not fire and state is high") {
                REQUIRE_FALSE(value);
                REQUIRE_FALSE(trigger.isHigh());
            }
        }
    }
}

SCENARIO("ThresholdTrigger processes a simple triangular signal") {
    GIVEN("a trigger") {
        ThresholdTrigger trigger;
        WHEN("the signal increases to 1.f and decreases to 0.f") {
            {    // 0.0
                bool value = trigger.process(0.f);
                THEN("the trigger does not fire at 0.f and is low") {
                    REQUIRE_FALSE(value);
                    REQUIRE_FALSE(trigger.isHigh());
                }
            } {  // 0.5
                bool value = trigger.process(0.5f);
                THEN("the trigger does not fire at 0.5f and is low") {
                    REQUIRE_FALSE(value);
                    REQUIRE_FALSE(trigger.isHigh());
                }
            } {  // 1.0
                bool value = trigger.process(1.0f);
                THEN("the trigger fires at 1.0f and is high") {
                    REQUIRE(value);
                    REQUIRE(trigger.isHigh());
                }
            } {  // 0.5
                bool value = trigger.process(0.5f);
                THEN("the trigger does not fire at 0.5f and is high") {
                    REQUIRE_FALSE(value);
                    REQUIRE(trigger.isHigh());
                }
            } {  // 0.0
                bool value = trigger.process(0.f);
                THEN("the reverse does not fires at 0.f and goes low") {
                    REQUIRE_FALSE(value);
                    REQUIRE_FALSE(trigger.isHigh());
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// MARK: HeldThresholdTrigger
// ---------------------------------------------------------------------------
