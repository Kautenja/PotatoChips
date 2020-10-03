// Test cases for the PCM code.
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

#include "dsp/pcm.hpp"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace PCM;

// ---------------------------------------------------------------------------
// MARK:
// ---------------------------------------------------------------------------

// SCENARIO("initialize Order") {
//     GIVEN("arbitrary legal parameters") {
//         UID uid = 5;
//         auto side = Side::Buy;
//         Quantity quantity = 100;
//         Price price = 5746;
//         WHEN("an Order is initialized") {
//             Order order = {uid, side, quantity, price};
//             THEN("the order is created with parameters") {
//                 REQUIRE(order.next == nullptr);
//                 REQUIRE(order.prev == nullptr);
//                 REQUIRE(order.uid == uid);
//                 REQUIRE(order.side == side);
//                 REQUIRE(order.quantity == quantity);
//                 REQUIRE(order.price == price);
//                 REQUIRE(order.limit == nullptr);
//             }
//         }
//     }
// }

TEST_CASE("sizeof should be correct") {
    REQUIRE(3 == sizeof(int24_t));
}

TEST_CASE("Should create 24-bit signed value from 8-bit signed value") {
    int8_t int8 = 0;
    int24_t int24(int8);
    // 24-bit on left-hand side of operation
    REQUIRE(int24 == int8);
    // 24-bit on right-hand side of operation
    REQUIRE(int8 == int24);
}

TEST_CASE("numeric limits should be correct") {
    REQUIRE(std::numeric_limits<int24_t>::max() == int24_t(0x7fffff));
    REQUIRE(std::numeric_limits<int24_t>::min() == int24_t(0xffffff));
    REQUIRE(std::numeric_limits<int24_t>::lowest() == int24_t(0xffffff));
}
