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

// Exercise the production module without constructing a widget.
#include <engine/Engine.hpp>
#undef PRIVATE
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/SuperEcho.cpp"
#include <memory>
#include <fstream>

Plugin* plugin_instance = nullptr;

namespace {
struct Host {
    rack::Context context;
    rack::Plugin plugin;
    Host() {
        rack::contextSet(&context);
        context.engine = new rack::engine::Engine;
        context.engine->setSampleRate(48000.f);
        plugin.slug = "KautenjaDSP-PotatoChips";
        plugin.addModel(modelSuperEcho);
        rack::plugin::plugins.push_back(&plugin);
    }
    ~Host() {
        rack::plugin::plugins.clear();
        plugin.models.clear();
        modelSuperEcho->plugin = nullptr;
        rack::contextSet(nullptr);
    }
};

bool protectedParam(int id) {
    return id == SuperEcho::PARAM_MIX || id == SuperEcho::PARAM_MIX + 1
        || id == SuperEcho::PARAM_GAIN || id == SuperEcho::PARAM_GAIN + 1
        || id == SuperEcho::PARAM_BYPASS;
}
}

CATCH_TEST_CASE("Echo preserves parameter identities, defaults and integer edits") {
    Host host;
    SuperEcho module;
    CATCH_REQUIRE(module.params.size() == 23);
    CATCH_CHECK(module.inputs.size() == 14);
    CATCH_CHECK(module.outputs.size() == 2);
    CATCH_CHECK(module.lights.size() == 36);
    CATCH_CHECK(SuperEcho::PARAM_DELAY == 0);
    CATCH_CHECK(SuperEcho::PARAM_FEEDBACK == 1);
    CATCH_CHECK(SuperEcho::PARAM_MIX == 2);
    CATCH_CHECK(SuperEcho::PARAM_FIR_COEFFICIENT == 4);
    CATCH_CHECK(SuperEcho::PARAM_FIR_COEFFICIENT_ATT == 12);
    CATCH_CHECK(SuperEcho::PARAM_GAIN == 20);
    CATCH_CHECK(SuperEcho::PARAM_BYPASS == 22);
    for (int id = 0; id < SuperEcho::NUM_PARAMS; ++id) {
        auto quantity = module.getParamQuantity(id);
        CATCH_INFO(id);
        CATCH_CHECK(quantity->paramId == id);
        CATCH_CHECK(quantity->randomizeEnabled == !protectedParam(id));
        // Exact names/ranges/defaults for all 23 are also pinned by test_contract.
        CATCH_CHECK(quantity->getValue() == quantity->getDefaultValue());
    }
    for (int i = 0; i < 8; ++i) {
        auto quantity = module.getParamQuantity(SuperEcho::PARAM_FIR_COEFFICIENT + i);
        CATCH_CHECK(quantity->snapEnabled);
        CATCH_CHECK(quantity->minValue == -128);
        CATCH_CHECK(quantity->maxValue == 127);
        CATCH_CHECK(quantity->defaultValue == (i ? 0 : 127));
        for (float value : {-128.f, -1.f, 0.f, 1.f, 127.f}) {
            quantity->setDisplayValueString(std::to_string(value));
            CATCH_CHECK(quantity->getValue() == value);
            CATCH_CHECK(std::stof(quantity->getDisplayValueString()) == value);
        }
        quantity->setValue(12.6f);
        CATCH_CHECK(quantity->getValue() == 13);
        quantity->setValue(-12.6f);
        CATCH_CHECK(quantity->getValue() == -13);
        quantity->reset();
        CATCH_CHECK(quantity->getValue() == (i ? 0 : 127));
    }
}

CATCH_TEST_CASE("Echo engine randomization protects exactly five controls without a widget") {
    Host host;
    SuperEcho module;
    rack::random::local().seed(0x006, 0x097);
    for (float bypass : {0.f, 1.f}) {
        module.getParamQuantity(SuperEcho::PARAM_MIX)->setValue(-64);
        module.getParamQuantity(SuperEcho::PARAM_MIX + 1)->setValue(96);
        module.getParamQuantity(SuperEcho::PARAM_GAIN)->setValue(.5f);
        module.getParamQuantity(SuperEcho::PARAM_GAIN + 1)->setValue(1.5f);
        module.getParamQuantity(SuperEcho::PARAM_BYPASS)->setValue(bypass);
        float before[SuperEcho::NUM_PARAMS];
        bool changed[SuperEcho::NUM_PARAMS] = {};
        for (int id = 0; id < SuperEcho::NUM_PARAMS; ++id)
            before[id] = module.params[id].getValue();
        for (int draw = 0; draw < 64; ++draw) {
            host.context.engine->randomizeModule(&module);
            for (int id = 0; id < SuperEcho::NUM_PARAMS; ++id) {
                const float value = module.params[id].getValue();
                if (protectedParam(id)) CATCH_CHECK(value == before[id]);
                else changed[id] |= value != before[id];
                if (id >= 4 && id < 12) CATCH_CHECK(value == std::round(value));
            }
        }
        for (int id = 0; id < SuperEcho::NUM_PARAMS; ++id) {
            CATCH_INFO(id);
            CATCH_CHECK(changed[id] == !protectedParam(id));
        }
    }
    host.context.engine->resetModule(&module);
    for (auto quantity : module.paramQuantities)
        CATCH_CHECK(quantity->getValue() == quantity->getDefaultValue());
}

CATCH_TEST_CASE("Echo signed presets and patch parameters round trip through Rack") {
    Host host;
    SuperEcho module;
    module.model = modelSuperEcho;
    unsigned presets = 0;
    for (const auto& path : rack::system::getEntries("presets/SuperEcho")) {
        json_t* saved = json_load_file(path.c_str(), 0, nullptr);
        CATCH_REQUIRE(saved);
        module.fromJson(saved);
        json_t* snapshot = module.paramsToJson();
        SuperEcho restored;
        restored.paramsFromJson(snapshot);
        for (int id = 0; id < SuperEcho::NUM_PARAMS; ++id)
            CATCH_CHECK(restored.params[id].getValue() == module.params[id].getValue());
        json_decref(snapshot);
        json_decref(saved);
        ++presets;
    }
    CATCH_CHECK(presets == 60);
    // Rack's metadata snapping applies to legacy fractional JSON on load.
    json_t* fractional = json_pack("[{s:i,s:f},{s:i,s:f}]",
        "id", 4, "value", -12.6, "id", 5, "value", 12.6);
    module.paramsFromJson(fractional);
    CATCH_CHECK(module.params[4].getValue() == -13);
    CATCH_CHECK(module.params[5].getValue() == 13);
    json_decref(fractional);
}

// Hash signed output samples of an audible, non-default stereo echo fixture.
static uint64_t echoAudio(float rate, int channels) {
    SuperEcho module;
    module.params[SuperEcho::PARAM_DELAY].setValue(1);
    module.params[SuperEcho::PARAM_FEEDBACK].setValue(-48);
    module.params[SuperEcho::PARAM_MIX].setValue(80);
    module.params[SuperEcho::PARAM_MIX + 1].setValue(-64);
    const float coefficients[8] = {100, -25, 12, -7, 3, -2, 1, 32};
    for (int i = 0; i < 8; ++i)
        module.params[SuperEcho::PARAM_FIR_COEFFICIENT + i].setValue(coefficients[i]);
    for (auto& output : module.outputs) output.channels = channels;
    module.inputs[SuperEcho::INPUT_AUDIO].channels = channels;
    module.inputs[SuperEcho::INPUT_AUDIO + 1].channels = channels;
    Module::ProcessArgs args = {};
    args.sampleRate = rate;
    args.sampleTime = 1.f / rate;
    uint64_t hash = 14695981039346656037ULL;
    for (int frame = 0; frame < 4096; ++frame) {
        args.frame = frame;
        for (int c = 0; c < channels; ++c) {
            module.inputs[0].setVoltage(frame % 127 == 0 ? (c + 1) * .1f : 0.f, c);
            module.inputs[1].setVoltage(frame % 131 == 0 ? (c + 1) * -.08f : 0.f, c);
        }
        static_cast<Module&>(module).process(args);
        for (auto& output : module.outputs) for (int c = 0; c < channels; ++c) {
            const int sample = std::lround(output.getVoltage(c) * 32767.f / 5.f);
            hash ^= static_cast<uint16_t>(sample);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

CATCH_TEST_CASE("Echo mono and polyphonic wet audio matches the pre-fix module") {
    Host host;
    std::ifstream fixture("test/rack/fixtures/super-echo-audio.txt");
    CATCH_REQUIRE(fixture.good());
    for (float rate : {44100.f, 48000.f, 96000.f}) for (int channels : {1, 16}) {
        int expected_rate, expected_channels;
        uint64_t expected_hash;
        CATCH_REQUIRE(static_cast<bool>(fixture >> expected_rate >> expected_channels >> expected_hash));
        CATCH_CHECK(expected_rate == rate);
        CATCH_CHECK(expected_channels == channels);
        CATCH_CHECK(echoAudio(rate, channels) == expected_hash);
    }
}
