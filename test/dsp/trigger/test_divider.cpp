// Test cases for the Trigger:Divider structure.
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

#include "dsp/trigger/divider.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

SCENARIO("Trigger::Divider accessors and mutators are used") {
    GIVEN("an initialized divider") {
        Trigger::Divider divider;
        WHEN("the default values are accessed") {
            THEN("the values are correct") {
                REQUIRE(divider.getDivision() == 1);
                REQUIRE(divider.getClock() == 0);
                REQUIRE(divider.getPhase() == 0.f);
                REQUIRE(divider.getGate() == true);
            }
        }
        WHEN("the division is set to a valid value") {
            divider.setDivision(2);
            THEN("the division is correct when accessed") {
                REQUIRE(divider.getDivision() == 2);
            }
            THEN("the other values are not effected") {
                REQUIRE(divider.getClock() == 0);
                REQUIRE(divider.getPhase() == 0.f);
                REQUIRE(divider.getGate() == true);
            }
        }
        WHEN("the division is set below the minimal value") {
            divider.setDivision(0);
            THEN("the division is set to the minimal value") {
                REQUIRE(divider.getDivision() == 1);
            }
            THEN("the other values are not effected") {
                REQUIRE(divider.getClock() == 0);
                REQUIRE(divider.getPhase() == 0.f);
                REQUIRE(divider.getGate() == true);
            }
        }
    }
}

SCENARIO("Trigger::Divider processes signals") {
    GIVEN("an initialized divider") {
        Trigger::Divider divider;
        WHEN("the divider processes at a division of 1") {
            for (unsigned i = 0; i < 10; i++) {  // arbitrarily check 10 samples
                // the division is 1, so the divider should always fire
                auto value = divider.process();
                THEN("the divider fires on the first step") {
                    REQUIRE(value);
                }
                THEN("the clock is reset to 0") {
                    REQUIRE(0 == divider.getClock());
                    REQUIRE(0.f == divider.getPhase());
                    REQUIRE(divider.getGate());
                }
            }
        }
        WHEN("the divider processes at a division of 2") {
            divider.setDivision(2);
            for (unsigned i = 0; i < 10; i++) {  // arbitrarily check 10 samples
                auto value = divider.process();
                THEN("the divider fires on the even steps") {
                    switch (i % 2) {
                        case 0:
                            REQUIRE(value);
                            break;
                        case 1:
                            REQUIRE_FALSE(value);
                            break;
                    }
                }
                THEN("the clock is at 1 on even steps") {
                    switch (i % 2) {
                        case 0:
                            REQUIRE(1 == divider.getClock());
                            REQUIRE(0.5f == divider.getPhase());
                            REQUIRE_FALSE(divider.getGate());
                            REQUIRE(divider.getGate(0.6f));
                            break;
                        case 1:
                            REQUIRE(0 == divider.getClock());
                            REQUIRE(0.f == divider.getPhase());
                            REQUIRE(divider.getGate());
                            break;
                    }
                }
            }
        }
    }
}
