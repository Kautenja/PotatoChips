// Test cases for the Trigger::Zero structure.
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

#include "dsp/trigger/zero.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("Trigger::Boolean should be false when processing 0s") {
    Trigger::Zero trigger;
    REQUIRE_FALSE(trigger.process(0.f));
}

TEST_CASE("Trigger::Boolean should be false when processing positive from 0") {
    Trigger::Zero trigger;
    REQUIRE_FALSE(trigger.process(0.f));
}

TEST_CASE("Trigger::Boolean should be true when processing positive from negative") {
    Trigger::Zero trigger;
    REQUIRE_FALSE(trigger.process(-1.f));
    REQUIRE(trigger.process(1.f));
    REQUIRE_FALSE(trigger.process(1.f));
    REQUIRE_FALSE(trigger.process(0.f));
    REQUIRE_FALSE(trigger.process(-1.f));
}
