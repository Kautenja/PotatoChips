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

// This fixture is a host: expose host-only declarations before rack.hpp.
#include <engine/Engine.hpp>
#include <history.hpp>
#undef PRIVATE
#include <rack.hpp>
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/widget/wavetable_editor.hpp"
#include "../../src/widget/indexed_frame_display.hpp"
#include <thread>

namespace {
struct WaveModule : rack::engine::Module, WavetableOwner {};
struct Host {
    rack::Context context;
    Host() {
        rack::contextSet(&context);
        context.engine = new rack::engine::Engine;
        context.history = new rack::history::State;
    }
    ~Host() { rack::contextSet(nullptr); }
};
}

CATCH_TEST_CASE("Wavetable edges, inclusive drags and previews are safe") {
    Host host;
    WaveModule module;
    WaveTableEditor editor(&module, 0, nullptr, 32, 15, {0, 0}, {100, 100});
    editor.edit({100, 0}, {100, 0});
    CATCH_CHECK(module.waveBank->samples[0][31] == 15);
    editor.edit({-200, 100}, {200, 100});
    for (auto& sample : module.waveBank->samples[0]) CATCH_CHECK(sample == 0);
    editor.edit({1, 100}, {1, 0});
    CATCH_CHECK(module.waveBank->samples[0][0] == 15);
    editor.edit({50, 100}, {0, 0});
    for (unsigned i = 0; i <= 16; ++i) CATCH_CHECK(module.waveBank->samples[0][i] == 15);
    const uint8_t preview[32] = {3};
    WaveTableEditor browser(nullptr, 0, preview, 32, 15, {0, 0}, {100, 100});
    browser.edit({0, 0}, {100, 0});
    CATCH_CHECK(browser.preview[0] == 3);
    CATCH_CHECK_FALSE(browser.editable());
    rack::event::Button event;
    event.action = GLFW_PRESS;
    event.button = GLFW_MOUSE_BUTTON_RIGHT;
    browser.onButton(event); // No parent or module in the browser.
    WaveTableEditor empty(&module, 0, nullptr, 0, 15, {0, 0}, {0, 0});
    CATCH_CHECK_FALSE(empty.editable());
    empty.edit({0, 0}, {100, 0});
}

CATCH_TEST_CASE("Wavetable history survives module deletion and recreation") {
    Host host;
    auto module = new WaveModule;
    host.context.engine->addModule(module);
    const auto id = module->id;
    WaveTableEditor editor(module, 0, nullptr, 32, 15, {0, 0}, {100, 100});
    rack::event::Button event;
    event.action = GLFW_PRESS;
    event.button = GLFW_MOUSE_BUTTON_LEFT;
    event.pos = {100, 0};
    editor.onButton(event);
    rack::event::DragEnd end;
    editor.onDragEnd(end);
    CATCH_REQUIRE(host.context.history->canUndo());
    host.context.history->undo();
    CATCH_CHECK(module->waveBank->samples[0][31] == 0);
    host.context.history->redo();
    CATCH_CHECK(module->waveBank->samples[0][31] == 15);
    host.context.engine->removeModule(module);
    delete module;
    CATCH_CHECK_FALSE(editor.editable());
    editor.edit({0, 0}, {100, 0});
    host.context.history->undo(); // Missing module is a safe no-op.
    module = new WaveModule;
    module->id = id;
    host.context.engine->addModule(module);
    host.context.history->redo(); // Like Rack's undo of a module deletion.
    CATCH_CHECK(module->waveBank->samples[0][31] == 15);
    host.context.history->undo();
    CATCH_CHECK(module->waveBank->samples[0][31] == 0);
    host.context.engine->removeModule(module);
    delete module;
}

CATCH_TEST_CASE("Wavetable samples support concurrent engine and editor access") {
    WaveModule module;
    std::thread writer([&] {
        for (unsigned n = 0; n < 10000; ++n)
            module.waveBank->samples[0][n % 32].store(n % 16, std::memory_order_relaxed);
    });
    bool valid = true;
    for (unsigned n = 0; n < 10000; ++n)
        valid &= module.waveBank->samples[0][n % 32].load(std::memory_order_relaxed) <= 15;
    writer.join();
    CATCH_CHECK(valid);
}

CATCH_TEST_CASE("Indexed displays own their frames and reject missing or invalid frames") {
    unsigned index = 0;
    for (unsigned i = 0; i < 100; ++i) {
        IndexedFrameDisplay display([&] { return index; }, "res/BossFight_algorithms/", 8, {0, 0}, {100, 100});
        index = 0;
        CATCH_REQUIRE(display.currentFrame() != nullptr);
        index = 7;
        CATCH_CHECK(display.currentFrame() != nullptr);
        index = 8;
        CATCH_CHECK(display.currentFrame() == nullptr);
        index = ~0u;
        CATCH_CHECK(display.currentFrame() == nullptr);
    }
    IndexedFrameDisplay missing({}, "res/not-a-frame/", 1, {0, 0}, {100, 100});
    CATCH_CHECK(missing.currentFrame() == nullptr);
    IndexedFrameDisplay empty({}, "res/BossFight_algorithms/", 0, {0, 0}, {100, 100});
    CATCH_CHECK(empty.currentFrame() == nullptr);
    IndexedFrameDisplay preview({}, "res/BossFight_algorithms/", 1, {0, 0}, {100, 100});
    CATCH_CHECK(preview.currentFrame() != nullptr);
}
