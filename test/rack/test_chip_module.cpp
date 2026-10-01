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

#include <engine/Engine.hpp>
#undef PRIVATE
#include <rack.hpp>
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/engine/chip_module.hpp"

rack::Plugin* plugin_instance = nullptr;

namespace {
/// Deterministic impulses test host mixing independently of chip algorithms.
struct ProbeChip {
    static constexpr unsigned OSC_COUNT = 2;
    BLIPBuffer* output[2];
    unsigned resets = 0, frames = 0, clocks = 0;
    void set_output(unsigned osc, BLIPBuffer* buffer) { output[osc] = buffer; }
    void set_volume(float) {}
    void reset() { ++resets; frames = 0; }
    void end_frame(unsigned ticks) {
        clocks = ticks;
        if (frames++ == 0) {
            output[0]->get_buffer()[0] += 3 * (1 << 27);
            output[1]->get_buffer()[0] += 1 << 28;
        }
    }
};
struct ProbeModule : ChipModule<ProbeChip> {
    unsigned audio[16] = {}, cv[16] = {}, lightsCount = 0;
    ProbeModule() { config(0, 1, 2, 0); }
    void processAudio(const ProcessArgs&, const unsigned& channel) override { ++audio[channel]; }
    void processCV(const ProcessArgs&, const unsigned& channel) override { ++cv[channel]; }
    void processLights(const ProcessArgs&, const unsigned&) override { ++lightsCount; }
    using ChipModule::apu;
    using ChipModule::buffers;
    using ChipModule::normal_outputs;
    using ChipModule::hard_clip;
};
struct Host {
    rack::Context context;
    Host() { rack::contextSet(&context); context.engine = new rack::engine::Engine; }
    ~Host() { rack::contextSet(nullptr); }
};
}

CATCH_TEST_CASE("ChipModule preserves rate conversion, channel independence and CV cadence") {
    Host host;
    for (float rate : {44100.f, 48000.f, 96000.f}) {
        host.context.engine->setSampleRate(rate);
        for (unsigned channels : {1u, 16u}) {
            ProbeModule module;
            module.inputs[0].channels = channels;
            for (auto& output : module.outputs) output.channels = 1;
            rack::engine::Module::ProcessArgs args;
            args.sampleRate = rate;
            args.sampleTime = 1.f / rate;
            for (unsigned frame = 0; frame < 513; ++frame) module.process(args);
            CATCH_CHECK(module.outputs[0].getChannels() == channels);
            CATCH_CHECK(module.lightsCount == 2);
            for (unsigned c = 0; c < 16; ++c) {
                CATCH_CHECK(module.audio[c] == (c < channels ? 513 : 0));
                CATCH_CHECK(module.cv[c] == (c < channels ? 33 : 0));
                CATCH_CHECK(module.buffers[c][0].get_sample_rate() == rate);
                CATCH_CHECK(module.buffers[c][0].get_clock_rate() == unsigned(rate) * (CLOCK_RATE / unsigned(rate)));
                if (c < channels) CATCH_CHECK(module.apu[c].clocks == CLOCK_RATE / unsigned(rate));
                if (c && c < channels) CATCH_CHECK(module.outputs[0].getVoltage(c) == module.outputs[0].getVoltage(0));
            }
            module.onReset();
            for (unsigned c = 0; c < 16; ++c) CATCH_CHECK(module.apu[c].resets == 2);
            module.process(args);
            CATCH_CHECK(module.cv[0] == 34); // Reset restarts on the downbeat.
            host.context.engine->setSampleRate(rate);
            module.onSampleRateChange();
            CATCH_CHECK(module.buffers[0][0].get_accumulator() == 0);
        }
    }
}

CATCH_TEST_CASE("ChipModule output normalling and hard clipping are stable") {
    Host host;
    host.context.engine->setSampleRate(48000.f);
    for (bool normal : {false, true}) for (bool connected : {false, true}) for (bool clip : {false, true}) {
        ProbeModule module;
        module.normal_outputs = normal;
        module.hard_clip = clip;
        module.outputs[0].channels = connected ? 1 : 0;
        module.outputs[1].channels = 1;
        rack::engine::Module::ProcessArgs args;
        args.sampleRate = 48000.f;
        args.sampleTime = 1.f / 48000.f;
        module.process(args);
        module.process(args);
        float expected = 16384.f / 32767.f;
        if (normal && !connected) expected += 24576.f / 32767.f;
        if (clip) expected = std::min(expected, 1.f);
        CATCH_CHECK(module.outputs[1].getVoltage() == Catch::Approx(Math::Eurorack::toAC(expected)));
    }
}
