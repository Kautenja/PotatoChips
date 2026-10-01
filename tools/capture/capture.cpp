// Native production-widget capture and geometry inspection; no audio device.
// Adapted from RackNES tools/capture and Fourier test/rack/inspect_panels.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/plugin.hpp"
#include "render.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include <vector>

void init(Plugin* plugin);

/// Assert native panel, port and screw caches follow the global preference.
static void themed(Widget* widget) {
    const bool dark = settings::preferDarkPanels;
    if (auto panel = dynamic_cast<ThemedSvgPanel*>(widget)) {
        if (!panel->lightSvg || !panel->darkSvg || panel->lightSvg == panel->darkSvg
            || panel->svg != (dark ? panel->darkSvg : panel->lightSvg))
            throw std::runtime_error("Panel theme selection failed");
    } else if (dynamic_cast<SvgPanel*>(widget)) {
        throw std::runtime_error("Unthemed module panel");
    }
    if (auto port = dynamic_cast<ThemedSvgPort*>(widget)) {
        if (port->sw->svg != (dark ? port->darkSvg : port->lightSvg))
            throw std::runtime_error("Port theme selection failed");
    } else if (dynamic_cast<SvgPort*>(widget)) {
        throw std::runtime_error("Unthemed port");
    }
    if (auto screw = dynamic_cast<ThemedSvgScrew*>(widget)) {
        if (screw->sw->svg != (dark ? screw->darkSvg : screw->lightSvg))
            throw std::runtime_error("Screw theme selection failed");
    } else if (dynamic_cast<SvgScrew*>(widget)) {
        throw std::runtime_error("Unthemed screw");
    }
    for (auto child : widget->children) themed(child);
}

/// Export actual control bounds for the separate, static manual schematic.
static void geometry(Widget* widget, Vec offset, json_t* controls) {
    const Vec position = offset.plus(widget->box.pos);
    std::string kind;
    int id = -1;
    std::string label;
    if (auto param = dynamic_cast<ParamWidget*>(widget)) {
        id = param->paramId;
        if (auto quantity = param->getParamQuantity()) label = quantity->name;
        kind = dynamic_cast<SliderKnob*>(param) ? "slider" :
            dynamic_cast<Knob*>(param) ? "knob" : "switch";
    } else if (auto port = dynamic_cast<PortWidget*>(widget)) {
        id = port->portId;
        if (port->module) {
            auto info = port->type == engine::Port::INPUT
                ? port->module->getInputInfo(id) : port->module->getOutputInfo(id);
            if (info) label = info->getName();
        }
        kind = port->type == engine::Port::INPUT ? "input" : "output";
    } else if (dynamic_cast<ModuleLightWidget*>(widget)) {
        kind = "light";
    } else {
        const std::string type = typeid(*widget).name();
        if (type.find("WaveTableEditor") != std::string::npos ||
            type.find("IndexedFrameDisplay") != std::string::npos) kind = "display";
    }
    if (!kind.empty()) {
        json_array_append_new(controls, json_pack("{s:s,s:s,s:i,s:f,s:f,s:f,s:f}",
            "kind", kind.c_str(), "label", label.c_str(), "id", id, "x", double(position.x),
            "y", double(position.y), "width", double(widget->box.size.x),
            "height", double(widget->box.size.y)));
        return;
    }
    // The panel artwork and control internals are not independent controls.
    if (dynamic_cast<SvgPanel*>(widget)) return;
    for (auto child : widget->children) geometry(child, position, controls);
}

#include "inspect_super_echo.hpp"

/// Construct, process and remove real modules without starting the engine thread.
int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: capture RACK_DIR PLUGIN_DIR OUTPUT_DIR INVENTORY MODULE_OR_ALL\n";
        return 1;
    }
    Context context;
    contextSet(&context);
    context.engine = new engine::Engine;
    context.engine->setSampleRate(48000.f);
    context.event = new widget::EventState;
    context.history = new history::State;
    asset::systemDir = argv[1];
    asset::userDir = std::string(argv[3]) + "/user";
    plugin::Plugin plugin;
    plugin.path = argv[2];
    plugin.slug = "KautenjaDSP-PotatoChips";
    init(&plugin);
    plugin::plugins.push_back(&plugin);
    if (!glfwInit()) {
        std::cerr << "Capture requires a graphical desktop and OpenGL\n";
        return 2;
    }
    int result = 0;
    try {
        context.window = new window::Window;
        glfwHideWindow(context.window->win);
        settings::showTipsOnLaunch = false;
        settings::rackBrightness = 1.f;
        context.scene = new app::Scene;
        json_t* inventory = json_load_file(argv[4], 0, nullptr);
        if (!inventory) throw std::runtime_error("Cannot read inventory");
        size_t index;
        json_t* entry;
        json_array_foreach(inventory, index, entry) {
            const std::string slug = json_string_value(json_object_get(entry, "slug"));
            const char* manual = json_string_value(json_object_get(entry, "manual"));
            const std::string name = manual ? manual : slug;
            if (std::string(argv[5]) != "all" && name != argv[5]) continue;
            const int expected_width = json_integer_value(json_object_get(entry, "width"));
            auto model = plugin.getModel(slug);
            if (!model) throw std::runtime_error("Unregistered model " + slug);
            json_t* report = json_object();
            json_object_set_new(report, "controls", json_array());
            json_object_set_new(report, "rack_version", json_string(APP_VERSION.c_str()));
            for (bool preview : {false, true}) {
                random::local().seed(0x504f5441544fULL, 0x4348495053ULL);
                auto module = preview ? nullptr : model->createModule();
                std::unique_ptr<Module> reference;
                if (!preview && (slug == "2A03" || slug == "106"
                    || slug == "GBS" || slug == "SuperEcho")) {
                    random::local().seed(0x504f5441544fULL, 0x4348495053ULL);
                    reference.reset(model->createModule());
                }
                if (module) {
                    module->id = index + 1;
                    context.engine->addModule(module);
                    if (reference) {
                        reference->id = index + 1001;
                        context.engine->addModule(reference.get());
                    }
                    // Fixed defaults, no patched signals or randomize/reset calls.
                    Module::ProcessArgs args = {};
                    args.sampleRate = 48000.f;
                    args.sampleTime = 1.f / args.sampleRate;
                    for (int sample = 0; sample < 4800; ++sample) {
                        args.frame = sample;
                        module->process(args);
                        if (reference) reference->process(args);
                    }
                }
                settings::preferDarkPanels = false;
                std::unique_ptr<ModuleWidget> widget(model->createModuleWidget(module));
                if (widget->box.size != Vec(expected_width, 380))
                    throw std::runtime_error("Geometry changed: " + name);
                if (!preview) geometry(widget.get(), Vec(), json_object_get(report, "controls"));
                json_t* state = module ? module->toJson() : nullptr;
                const int history_index = context.history->actionIndex;
                const int saved_index = context.history->savedIndex;
                for (const std::string theme : {"Light", "Dark", "Light"}) {
                    settings::preferDarkPanels = theme == "Dark";
                    const int ratio = capture(widget.get(), std::string(argv[3]) + "/" + name
                        + (preview ? "-Preview-" : "-") + theme + ".ppm");
                    json_object_set_new(report, "pixel_ratio", json_integer(ratio));
                    themed(widget.get());
                    if (theme == "Dark" || theme == "Light") {
                        const std::string path = std::string(argv[3]) + "/" + name
                            + (preview ? "-Preview-" : "-") + theme;
                        capture(widget.get(), path + "-Zoom75.ppm", .75f);
                        capture(widget.get(), path + "-Zoom150.ppm", 1.5f);
                        capture(widget.get(), path + "-Dim.ppm", 1.f, .5f);
                    }
                    json_t* bounds = json_array();
                    geometry(widget.get(), Vec(), bounds);
                    if (!preview && !json_equal(bounds, json_object_get(report, "controls")))
                        throw std::runtime_error("Theme changed control geometry");
                    json_decref(bounds);
                    if (module) {
                        json_t* after = module->toJson();
                        const bool equal = json_equal(state, after);
                        json_decref(after);
                        if (!equal) throw std::runtime_error("Theme changed module state");
                    }
                    if (context.history->actionIndex != history_index
                        || context.history->savedIndex != saved_index)
                        throw std::runtime_error("Theme changed patch history");
                }
                json_decref(state);
                // Real context lifecycle events, then another complete render.
                Widget::ContextDestroyEvent destroy;
                destroy.vg = context.window->vg;
                widget->onContextDestroy(destroy);
                Widget::ContextCreateEvent create;
                create.vg = context.window->vg;
                widget->onContextCreate(create);
                capture(widget.get(), std::string(argv[3]) + "/" + name
                    + (preview ? "-Preview-Restored.ppm" : "-Restored.ppm"));
                themed(widget.get());
                // Also construct in dark mode, before any step/event callback.
                settings::preferDarkPanels = true;
                std::unique_ptr<ModuleWidget> initial(model->createModuleWidget(nullptr));
                themed(initial.get());
                if (reference) {
                    // Compare a rendered/toggled instance to an untouched twin.
                    // This is an offline deterministic probe, not an audio device.
                    for (Module* target : {module, reference.get()}) {
                        target->inputs[0].channels = 16;
                        for (auto& output : target->outputs) output.channels = 16;
                    }
                    Module::ProcessArgs args = {};
                    args.sampleRate = 48000.f;
                    args.sampleTime = 1.f / args.sampleRate;
                    for (int sample = 0; sample < 512; ++sample) {
                        args.frame = 4800 + sample;
                        if (sample % 64 == 0) {
                            settings::preferDarkPanels = !settings::preferDarkPanels;
                            widget->step();
                            themed(widget.get());
                            initial->step();
                            themed(initial.get());
                        }
                        for (int channel = 0; channel < 16; ++channel) {
                            const float voltage = slug == "SuperEcho"
                                ? (sample % 32 < 16 ? 1.f : -1.f) : channel * .01f;
                            module->inputs[0].setVoltage(voltage, channel);
                            reference->inputs[0].setVoltage(voltage, channel);
                        }
                        module->process(args);
                        reference->process(args);
                        for (size_t port = 0; port < module->outputs.size(); ++port)
                            for (int channel = 0; channel < 16; ++channel)
                                if (module->outputs[port].getVoltage(channel)
                                    != reference->outputs[port].getVoltage(channel))
                                    throw std::runtime_error("Theme changed audio: " + name
                                        + " frame " + std::to_string(sample)
                                        + " port " + std::to_string(port)
                                        + " channel " + std::to_string(channel));
                    }
                    json_object_set_new(report, "polyphonic_audio_equal_after_toggles", json_true());
                    context.engine->removeModule(reference.get());
                }
                if (slug == "SuperEcho") {
                    inspectSuperEcho(widget.get(), std::string(argv[3]) + "/SuperEcho-Probe");
                    json_object_set_new(report, "echo_interactions_verified", json_true());
                }
            }
            json_object_set_new(report, "theme_state_geometry_history_verified", json_true());
            const std::string filename = std::string(argv[3]) + "/" + name + ".json";
            if (json_dump_file(report, filename.c_str(), JSON_INDENT(2)))
                throw std::runtime_error("Cannot write geometry report");
            json_decref(report);
            std::cout << name << ": live, preview, preference toggles, context recreation passed\n";
        }
        json_decref(inventory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 3;
    }
    delete context.scene;
    context.scene = nullptr;
    delete context.window;
    context.window = nullptr;
    glfwTerminate();
    plugin::plugins.clear();
    contextSet(nullptr);
    return result;
}
