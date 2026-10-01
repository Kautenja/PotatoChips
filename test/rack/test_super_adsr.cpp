// Headless Rack envelope timing, gate and compatibility regressions.
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

// Real module clock, gate and saved-parameter regressions for spec 007.
#include <engine/Engine.hpp>
#undef PRIVATE
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/SuperADSR.cpp"
#include <memory>
#include <vector>

Plugin* plugin_instance = nullptr;
namespace {
struct Host {
    rack::Context context;
    rack::Plugin plugin;
    Host() {
        rack::contextSet(&context);
        context.engine = new rack::engine::Engine;
        plugin.slug = "KautenjaDSP-PotatoChips";
        plugin.addModel(modelSuperADSR);
        rack::plugin::plugins.push_back(&plugin);
    }
    ~Host() {
        rack::plugin::plugins.clear();
        plugin.models.clear();
        modelSuperADSR->plugin = nullptr;
        rack::contextSet(nullptr);
    }
};
void process(SuperADSR& module, float rate) {
    rack::engine::Module::ProcessArgs args = {};
    args.sampleRate = rate;
    args.sampleTime = 1.f / rate;
    static_cast<Module&>(module).process(args);
}
void connect(SuperADSR& module, int channels = 1) {
    for (auto& output : module.outputs) output.channels = channels;
    module.inputs[0].channels = module.inputs[1].channels = channels;
}
void fast(SuperADSR& module) {
    for (int lane = 0; lane < 2; ++lane) {
        module.params[2 + lane].setValue(0);
        module.params[4 + lane].setValue(7);
        module.params[6 + lane].setValue(7);
        module.params[8 + lane].setValue(31);
    }
}
}

CATCH_TEST_CASE("Contour follows the same chip-tick waveform through every stage at every host rate") {
    Host host;
    for (int amplitude : {127, -128, 0}) {
        SonyS_DSP::ADSR reference;
        reference.setAttack(5); reference.setDecay(0);
        reference.setSustainLevel(5); reference.setSustainRate(11);
        reference.setAmplitude(amplitude);
        std::vector<int> samples;
        bool visited[5] = {};
        for (int tick = 0; tick < 24000; ++tick) {
            samples.push_back(reference.run(tick == 0, tick < 22400));
            visited[static_cast<int>(reference.getStage())] = true;
        }
        for (bool stage : visited) CATCH_REQUIRE(stage);
        for (float rate : {22050.f, 32000.f, 44100.f, 48000.f, 96000.f}) {
            SuperADSR module;
            connect(module);
            module.params[0].setValue(amplitude);
            const int frames = std::lround(.75 * rate);
            bool equal = true;
            int first_mismatch = -1;
            for (int frame = 0; frame < frames; ++frame) {
                module.inputs[0].setVoltage(frame < std::lround(.7 * rate) ? 5 : 0);
                process(module, rate);
                const int tick = int(std::floor((frame + 1) * 32000. / rate + 1e-8)) - 1;
                const float expected = tick < 0 ? 0 : 10.f * samples.at(tick) / 128.f;
                if (module.outputs[0].getVoltage() != expected) {
                    equal = false;
                    if (first_mismatch < 0) first_mismatch = frame;
                }
                CATCH_REQUIRE(module.outputs[2].getVoltage() == -module.outputs[0].getVoltage());
                CATCH_REQUIRE(module.outputs[1].getVoltage() == 0);
            }
            CATCH_INFO(rate << " Hz, amplitude " << amplitude << ", first mismatch " << first_mismatch);
            CATCH_CHECK(equal);
        }
    }
}

CATCH_TEST_CASE("Contour key-off time is rate independent and survives a host-rate change") {
    Host host;
    for (float rate : {44100.f, 48000.f, 96000.f}) {
        SuperADSR module;
        connect(module); fast(module);
        module.inputs[0].setVoltage(5);
        for (int n = 0; n < 100; ++n) process(module, rate);
        CATCH_REQUIRE(module.outputs[0].getVoltage() > 9);
        module.inputs[0].setVoltage(0);
        int samples = 0;
        while (module.outputs[0].getVoltage() && samples++ < int(rate * .02)) process(module, rate);
        CATCH_CHECK(std::abs(samples / double(rate) - 253. / 32000) <= 1. / rate + 1. / 32000 + 1e-12);
        for (int n = 0; n < 100; ++n) process(module, rate);
        CATCH_CHECK(module.outputs[0].getVoltage() == 0);
    }
    SuperADSR module;
    connect(module); fast(module);
    module.inputs[0].setVoltage(5);
    for (int n = 0; n < 101; ++n) process(module, 48000);
    module.inputs[0].setVoltage(0);
    double elapsed = 0;
    for (int n = 0; n < 97; ++n) { process(module, 48000); elapsed += 1. / 48000; }
    float previous = module.outputs[0].getVoltage();
    for (int n = 0; n < 1000 && module.outputs[0].getVoltage(); ++n) {
        process(module, 96000); elapsed += 1. / 96000;
        CATCH_CHECK(module.outputs[0].getVoltage() <= previous);
        previous = module.outputs[0].getVoltage();
    }
    CATCH_CHECK(std::abs(elapsed - 253. / 32000) <= 1. / 48000 + 1. / 32000 + 1e-12);
    // A fractional tick is not restarted on a host-rate change.
    SuperADSR phase;
    connect(phase); fast(phase);
    phase.inputs[0].setVoltage(5);
    process(phase, 96000); // 1/3 tick retained.
    process(phase, 48000); // +2/3 tick: first attack update.
    CATCH_CHECK(phase.outputs[0].getVoltage() > 0);
}

CATCH_TEST_CASE("Contour latches sub-tick edges and preserves retrigger priority and hysteresis") {
    Host host;
    SuperADSR module;
    connect(module); fast(module);
    module.inputs[2].channels = 1;
    module.inputs[0].setVoltage(1.99f);
    process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == 0);
    module.inputs[0].setVoltage(2.f);
    process(module, 96000); // Rising edge before the first chip tick.
    module.inputs[0].setVoltage(0.f);
    process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() > 4);
    for (int n = 0; n < 900; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == 0);
    module.inputs[0].setVoltage(5);
    for (int n = 0; n < 30; ++n) process(module, 96000);
    module.inputs[0].setVoltage(.02f); // Hysteresis retains high.
    for (int n = 0; n < 900; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() > 9);
    // RETRIG during a held, decaying gate resumes attack from the current level.
    module.params[8].setValue(0);
    for (int n = 0; n < 300; ++n) process(module, 96000);
    const float held_level = module.outputs[0].getVoltage();
    module.inputs[2].setVoltage(5); process(module, 96000);
    module.inputs[2].setVoltage(0); process(module, 96000); process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() > held_level);
    module.params[8].setValue(31);
    module.inputs[0].setVoltage(.01f);
    for (int n = 0; n < 600; ++n) process(module, 96000);
    const float releasing = module.outputs[0].getVoltage();
    CATCH_REQUIRE(releasing > 0);
    module.inputs[2].setVoltage(5); process(module, 96000);
    module.inputs[2].setVoltage(0); process(module, 96000); process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() > releasing);
    for (int n = 0; n < 900; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == 0);
    // Coincident key-off/retrigger attacks first, even with RETRIG held high.
    module.inputs[0].setVoltage(5);
    for (int n = 0; n < 30; ++n) process(module, 96000);
    const float peak = module.outputs[0].getVoltage();
    module.inputs[0].setVoltage(0);
    module.inputs[2].setVoltage(5);
    for (int n = 0; n < 3; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == peak);
    for (int n = 0; n < 900; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == 0);
    module.inputs[2].setVoltage(.01f); process(module, 96000);
    // A RETRIG-only pulse with Gate low attacks once, then releases.
    module.inputs[2].setVoltage(5);
    for (int n = 0; n < 3; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() > 4);
    for (int n = 0; n < 900; ++n) process(module, 96000);
    CATCH_CHECK(module.outputs[0].getVoltage() == 0);
}

CATCH_TEST_CASE("Contour lanes and 16 voices remain independent through disconnect and reconnect") {
    Host host;
    SuperADSR module;
    connect(module, 16); fast(module);
    module.params[1].setValue(-128);
    for (int lane = 0; lane < 2; ++lane) for (int c = 0; c < 16; ++c)
        module.inputs[lane].setVoltage(5, c);
    for (int n = 0; n < 100; ++n) process(module, 48000);
    module.inputs[0].setVoltage(0, 7);
    for (int n = 0; n < 500; ++n) process(module, 48000);
    for (int c = 0; c < 16; ++c) {
        CATCH_CHECK(module.outputs[0].getVoltage(c) == (c == 7 ? 0 : 9.84375f));
        CATCH_CHECK(module.outputs[1].getVoltage(c) == -9.921875f);
        CATCH_CHECK(module.outputs[2].getVoltage(c) == -module.outputs[0].getVoltage(c));
        CATCH_CHECK(module.outputs[3].getVoltage(c) == -module.outputs[1].getVoltage(c));
    }
    // Remove cables without trusting residual input buffer contents.
    module.inputs[0].channels = module.inputs[1].channels = 0;
    for (int n = 0; n < 500; ++n) process(module, 48000);
    CATCH_CHECK(module.outputs[0].getChannels() == 1);
    connect(module, 16);
    for (int c = 0; c < 16; ++c) for (int lane = 0; lane < 2; ++lane)
        module.inputs[lane].setVoltage(0, c);
    process(module, 48000);
    for (int c = 0; c < 16; ++c) CATCH_CHECK(module.outputs[0].getVoltage(c) == 0);
    module.inputs[1].setVoltage(5, 15);
    for (int n = 0; n < 4; ++n) process(module, 48000);
    CATCH_CHECK(module.outputs[1].getVoltage(15) < 0);
    CATCH_CHECK(module.outputs[0].getVoltage(15) == 0);
}

CATCH_TEST_CASE("Contour reset and saved parameters keep their existing meaning") {
    Host host;
    std::unique_ptr<Module> module(modelSuperADSR->createModule());
    CATCH_CHECK(module->params.size() == 10);
    CATCH_CHECK(module->inputs.size() == 4);
    CATCH_CHECK(module->outputs.size() == 4);
    CATCH_CHECK(module->lights.size() == 30);
    for (int id = 0; id < 10; ++id) module->params[id].setValue(module->getParamQuantity(id)->maxValue);
    json_t* saved = module->toJson();
    std::unique_ptr<Module> restored(modelSuperADSR->createModule());
    restored->fromJson(saved);
    for (int id = 0; id < 10; ++id) CATCH_CHECK(module->params[id].getValue() == restored->params[id].getValue());
    json_decref(saved);
    host.context.engine->resetModule(module.get());
    for (auto quantity : module->paramQuantities) CATCH_CHECK(quantity->getValue() == quantity->defaultValue);
    SuperADSR active;
    connect(active); fast(active);
    active.inputs[0].setVoltage(5);
    for (int n = 0; n < 100; ++n) process(active, 48000);
    const float active_level = active.outputs[0].getVoltage();
    host.context.engine->resetModule(&active);
    process(active, 48000);
    CATCH_CHECK(active.outputs[0].getVoltage() == active_level);
    active.inputs[0].setVoltage(0);
    for (int n = 0; n < 500; ++n) process(active, 48000);
    CATCH_CHECK(active.outputs[0].getVoltage() == 0);
    // Reloaded progress is fresh; patch state contains no oscillator/envelope clock.
    connect(static_cast<SuperADSR&>(*restored));
    for (int n = 0; n < 1000; ++n) process(static_cast<SuperADSR&>(*restored), 48000);
    CATCH_CHECK(restored->outputs[0].getVoltage() == 0);
}
