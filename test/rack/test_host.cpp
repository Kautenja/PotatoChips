// Headless Rack construction smoke test.
//
// Copyright (c) 2026 Christian Kauten
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

#include "catch_amalgamated.hpp"
#include "../../src/InfiniteStairs.cpp"
#include <memory>

Plugin* plugin_instance = nullptr;

TEST_CASE("Infinite Stairs constructs under a real Rack engine") {
    rack::Context context;
    rack::contextSet(&context);
    context.engine = new rack::engine::Engine;
    context.engine->setSampleRate(48000.f);
    {
        std::unique_ptr<rack::engine::Module> module(modelInfiniteStairs->createModule());
        REQUIRE(module != nullptr);
        REQUIRE(module->outputs.size() == 4);
        REQUIRE(modelInfiniteStairs->slug == "2A03");
    }
    rack::contextSet(nullptr);
}
