// Native Rack Scope reproduction of Contour's gate-off behavior.
// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
#include <engine/Engine.hpp>
#undef PRIVATE
#include "../../src/plugin.hpp"
#include "render.hpp"
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <memory>

Plugin* plugin_instance = nullptr;

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: adsr_scope RACK_DIR PLUGIN_DIR FUNDAMENTAL_DIR OUTPUT_DIR\n";
        return 1;
    }
    Context context;
    contextSet(&context);
    context.engine = new engine::Engine;
    context.event = new widget::EventState;
    context.history = new history::State;
    asset::systemDir = argv[1];
    asset::userDir = std::string(argv[4]) + "/user";
    Plugin plugin;
    plugin.slug = "KautenjaDSP-PotatoChips";
    plugin.path = argv[2];
    plugin_instance = &plugin;
    plugin.addModel(modelSuperADSR);
    Plugin fundamental;
    fundamental.slug = "Fundamental";
    fundamental.path = argv[3];
#ifdef __APPLE__
    const std::string library = fundamental.path + "/plugin.dylib";
#else
    const std::string library = fundamental.path + "/plugin.so";
#endif
    void* handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) { std::cerr << dlerror() << '\n'; return 2; }
    auto initialize = reinterpret_cast<void (*)(Plugin*)>(dlsym(handle, "init"));
    if (!initialize) return 2;
    initialize(&fundamental);
    if (!glfwInit()) return 2;
    int result = 0;
    try {
        context.window = new window::Window;
        glfwHideWindow(context.window->win);
        context.scene = new app::Scene;
        std::ofstream report(std::string(argv[4]) + "/measurements.csv");
        report << "host_hz,sr_slider,keyoff_voltage,first_zero_samples,first_zero_ms,scope_ms_screen\n";
        std::ofstream samples(std::string(argv[4]) + "/waveforms.csv");
        samples << "host_hz,sr_slider,frame,gate_v,out_v,inv_v\n";
        for (float rate : {44100.f, 48000.f, 96000.f}) for (int sr : {0, 20, 31}) {
            context.engine->setSampleRate(rate);
            auto module = modelSuperADSR->createModule();
            module->params[2].setValue(0); // Fastest attack.
            module->params[4].setValue(0); // Fastest decay.
            module->params[6].setValue(7); // Highest sustain threshold.
            module->params[8].setValue(sr);
            module->inputs[0].channels = 1;
            for (auto& output : module->outputs) output.channels = 1;
            module->id = 1;
            context.engine->addModule(module);
            std::unique_ptr<ModuleWidget> panel(modelSuperADSR->createModuleWidget(module));
            auto scopeModel = fundamental.getModel("Scope");
            auto scope = scopeModel->createModule();
            scope->id = 2;
            context.engine->addModule(scope);
            std::unique_ptr<ModuleWidget> scopeWidget(scopeModel->createModuleWidget(scope));
            scope->params[1].setValue(3); // Upper trace: 0/5 V gate.
            scope->params[3].setValue(-7); // Lower trace: OUT.
            scope->params[4].setValue(-std::log2(.010f)); // Requested 10 ms/screen.
            scope->params[7].setValue(1); // Untriggered one-shot acquisition.
            scope->inputs[0].channels = scope->inputs[1].channels = 1;
            Module::ProcessArgs args = {};
            args.sampleRate = rate;
            args.sampleTime = 1.f / rate;
            const int high_frames = std::lround(.001 * rate);
            const int pre_frames = std::lround(.0005 * rate);
            // Scope rounds each of its 256 buckets up to whole host samples.
            const int scope_stride = std::ceil(rack::dsp::exp2_taylor5(
                -scope->params[4].getValue()) * rate / 256);
            const int scope_frames = scope_stride * 256;
            float keyoff = 0;
            int zero_frame = -1;
            for (int frame = 0; frame < high_frames + int(.012 * rate); ++frame) {
                args.frame = frame;
                const float gate = frame < high_frames ? 5.f : 0.f;
                module->inputs[0].setVoltage(gate);
                module->process(args);
                const float out = module->outputs[0].getVoltage();
                if (frame == high_frames - 1) keyoff = out;
                if (frame >= high_frames && zero_frame < 0 && out == 0) zero_frame = frame - high_frames + 1;
                samples << int(rate) << ',' << sr << ',' << frame << ',' << gate << ','
                    << out << ',' << module->outputs[2].getVoltage() << '\n';
                const int scope_frame = frame - (high_frames - pre_frames);
                if (scope_frame >= 0 && scope_frame < scope_frames) {
                    scope->inputs[0].setVoltage(gate);
                    scope->inputs[1].setVoltage(out);
                    scope->process(args);
                }
            }
            if (keyoff <= 0 || zero_frame <= 1) throw std::runtime_error("Missing nonzero release tail");
            report << int(rate) << ',' << sr << ',' << keyoff << ',' << zero_frame << ','
                << std::setprecision(9) << 1000. * zero_frame / rate << ','
                << 1000. * scope_frames / rate << '\n';
            std::unique_ptr<ModuleWidget> scene(new ModuleWidget);
            scene->box.size = Vec(panel->box.size.x + scopeWidget->box.size.x, 380);
            scopeWidget->box.pos.x = panel->box.size.x;
            scene->addChild(panel.release());
            scene->addChild(scopeWidget.release());
            capture(scene.get(), std::string(argv[4]) + "/scope-" + std::to_string(int(rate))
                + "-sr" + std::to_string(sr) + ".ppm");
        }
        std::cout << "Rack " << APP_VERSION << ": 9 native gate/OUT Scope captures passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 3; }
    delete context.scene; context.scene = nullptr;
    delete context.window; context.window = nullptr;
    glfwTerminate();
    contextSet(nullptr);
    return result;
}
