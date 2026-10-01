// Registration, saved patches, presets, and representative audio contracts.
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
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
extern void init(rack::Plugin*);

CATCH_TEST_CASE("Registered models preserve saved-patch and audio contracts") {
    rack::Context context;
    rack::contextSet(&context);
    context.engine = new rack::engine::Engine;
    rack::Plugin plugin;
    plugin.slug = "KautenjaDSP-PotatoChips";
    plugin.version = "2.1.0";
    init(&plugin);
    rack::plugin::plugins.push_back(&plugin);
    CATCH_REQUIRE(plugin.models.size() == 16);
    CATCH_CHECK(plugin.getModel("SuperSampler") == nullptr);
    CATCH_CHECK(plugin.getModel("SuperSynth") == nullptr);
    json_t* manifest = json_load_file("plugin.json", 0, nullptr);
    CATCH_REQUIRE(manifest != nullptr);
    json_t* models = json_object_get(manifest, "modules");
    CATCH_CHECK(json_array_size(models) == plugin.models.size());
    unsigned disabled = 0;
    size_t modelIndex;
    json_t* modelData;
    json_array_foreach(models, modelIndex, modelData) {
        const char* slug = json_string_value(json_object_get(modelData, "slug"));
        CATCH_REQUIRE(slug != nullptr);
        CATCH_CHECK(plugin.getModel(slug) != nullptr);
        if (json_is_true(json_object_get(modelData, "disabled"))) {
            ++disabled;
        }
    }
    CATCH_CHECK(disabled == 0);
    json_decref(manifest);
    std::ostringstream out;
    out << std::setprecision(17);
    for (auto model : plugin.models) {
        context.engine->setSampleRate(48000.f);
        std::unique_ptr<rack::engine::Module> module(model->createModule());
        CATCH_INFO(model->slug);
        json_t* saved = module->toJson();
        std::unique_ptr<rack::engine::Module> restored(model->createModule());
        restored->fromJson(saved);
        json_decref(saved);
        json_t* before = module->dataToJson();
        json_t* after = restored->dataToJson();
        CATCH_CHECK(((!before && !after) || json_equal(before, after)));
        json_decref(before);
        json_decref(after);
        for (unsigned i = 0; i < module->params.size(); ++i)
            CATCH_CHECK(module->params[i].getValue() == restored->params[i].getValue());
        out << model->slug << ' ' << module->params.size() << ' ' << module->inputs.size()
            << ' ' << module->outputs.size() << ' ' << module->lights.size() << '\n';
        for (auto quantity : module->paramQuantities)
            out << quantity->name << ' ' << quantity->minValue << ' ' << quantity->maxValue
                << ' ' << quantity->defaultValue << ' ' << quantity->snapEnabled << '\n';
        json_t* data = module->dataToJson();
        char* text = json_dumps(data, JSON_SORT_KEYS);
        out << (text ? text : "null") << '\n';
        free(text);
        json_decref(data);
        if (model->slug != "2A03" && model->slug != "106" &&
            model->slug != "GBS" && model->slug != "SuperEcho") continue;
        for (float rate : {44100.f, 48000.f, 96000.f}) for (int channels : {1, 16}) {
            context.engine->setSampleRate(rate);
            module.reset(model->createModule());
            // Headless host connection: setChannels() cannot connect a port.
            module->inputs[0].channels = channels;
            for (int channel = 0; channel < channels; ++channel)
                module->inputs[0].setVoltage(0.01f * channel, channel);
            for (auto& output : module->outputs) output.channels = channels;
            rack::engine::Module::ProcessArgs args;
            args.sampleRate = rate;
            args.sampleTime = 1.f / rate;
            double sum = 0, energy = 0;
            for (int frame = 0; frame < 2048; ++frame) {
                args.frame = frame;
                module->process(args);
                for (auto& output : module->outputs) for (int channel = 0; channel < channels; ++channel) {
                    double voltage = output.getVoltage(channel);
                    sum += voltage;
                    energy += voltage * voltage;
                }
            }
            out << rate << ' ' << channels << ' ' << sum << ' ' << energy << '\n';
        }
    }

    std::ifstream fixture("test/rack/fixtures/contracts.txt");
    CATCH_REQUIRE(fixture.good());
    std::istringstream actual(out.str());
    std::string expectedLine, actualLine;
    while (std::getline(fixture, expectedLine)) {
        CATCH_REQUIRE(static_cast<bool>(std::getline(actual, actualLine)));
        CATCH_INFO(expectedLine);
        if (expectedLine.find("44100 ") == 0 || expectedLine.find("48000 ") == 0 ||
            expectedLine.find("96000 ") == 0) {
            std::istringstream expectedValues(expectedLine), actualValues(actualLine);
            double expectedValue, actualValue;
            for (unsigned i = 0; i < 4; ++i) {
                CATCH_REQUIRE(static_cast<bool>(expectedValues >> expectedValue));
                CATCH_REQUIRE(static_cast<bool>(actualValues >> actualValue));
                // Float synthesis and aggregate summation may differ by compiler.
                CATCH_CHECK(actualValue == Catch::Approx(expectedValue).epsilon(1e-5).margin(1e-7));
            }
        } else CATCH_CHECK(actualLine == expectedLine);
    }
    CATCH_CHECK_FALSE(static_cast<bool>(std::getline(actual, actualLine)));
    unsigned presetCount = 0, patchModuleCount = 0;
    auto restore = [&](json_t* saved) {
        auto model = rack::plugin::modelFromJson(saved);
        CATCH_REQUIRE(model != nullptr);
        std::unique_ptr<rack::engine::Module> module(model->createModule());
        module->fromJson(saved);
        json_t* params = json_object_get(saved, "params");
        size_t i;
        json_t* param;
        json_array_foreach(params, i, param) {
            auto id = json_integer_value(json_object_get(param, "id"));
            CATCH_REQUIRE(id >= 0);
            CATCH_REQUIRE(id < static_cast<int64_t>(module->params.size()));
            float value = json_number_value(json_object_get(param, "value"));
            auto quantity = module->paramQuantities[id];
            if (!quantity->isBounded()) value = quantity->defaultValue;
            else {
                value = std::max(std::min(quantity->minValue, quantity->maxValue),
                    std::min(std::max(quantity->minValue, quantity->maxValue), value));
                if (quantity->snapEnabled) value = std::round(value);
            }
            CATCH_CHECK(module->params[id].getValue() == Catch::Approx(value));
        }
    };
    for (const auto& path : rack::system::getEntries("presets/SuperEcho")) {
        CATCH_INFO(path);
        json_t* saved = json_load_file(path.c_str(), 0, nullptr);
        CATCH_REQUIRE(saved != nullptr);
        restore(saved);
        json_decref(saved);
        ++presetCount;
    }
    CATCH_CHECK(presetCount == 60);
    for (const auto& path : rack::system::getEntries("patches/debug")) {
        if (rack::system::getExtension(path) != ".vcv") continue;
        CATCH_INFO(path);
        json_t* patch = json_load_file(path.c_str(), 0, nullptr);
        CATCH_REQUIRE(patch != nullptr);
        size_t i;
        json_t* saved;
        json_array_foreach(json_object_get(patch, "modules"), i, saved) {
            const char* slug = json_string_value(json_object_get(saved, "plugin"));
            const char* model = json_string_value(json_object_get(saved, "model"));
            if (!slug || !model || plugin.slug != slug) continue;
            // Historical SCC/TurboGrafx16 examples refer to unregistered models.
            if (!plugin.getModel(model)) continue;
            restore(saved);
            ++patchModuleCount;
        }
        json_decref(patch);
    }
    CATCH_CHECK(patchModuleCount > 40);
    rack::plugin::plugins.clear();
    rack::contextSet(nullptr);
}
