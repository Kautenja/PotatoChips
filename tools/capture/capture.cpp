// Native production-widget capture and geometry inspection; no audio device.
// Adapted from RackNES tools/capture and Fourier test/rack/inspect_panels.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/plugin.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include <vector>

void init(Plugin* plugin);

/// Check component caches and SVG resources, including late child controls.
static bool ready(Widget* widget, bool nested = false) {
    if (auto svg = dynamic_cast<SvgWidget*>(widget))
        if (!svg->svg || !svg->svg->handle)
            throw std::runtime_error("Missing widget SVG resource");
    bool settled = true;
    if (auto cache = dynamic_cast<FramebufferWidget*>(widget)) {
        if (!nested && !cache->bypassed)
            settled = !cache->dirty && cache->getImageHandle() > 0;
        nested = true;
    }
    for (auto child : widget->children) settled = ready(child, nested) && settled;
    return settled;
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

/// Draw production layers until every component framebuffer settles.
static int capture(ModuleWidget* widget, const std::string& filename) {
    const int canvas_width = int(widget->box.size.x) + 20;
    const int canvas_height = 420;
    glfwSetWindowSize(APP->window->win, canvas_width, canvas_height);
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(APP->window->win, &width, &height);
    if (width % canvas_width || height != canvas_height * (width / canvas_width))
        throw std::runtime_error("Unexpected desktop framebuffer dimensions");
    const int ratio = width / canvas_width;
    if (ratio < 1) throw std::runtime_error("Invalid desktop pixel ratio");
    APP->window->pixelRatio = ratio;
    bool settled = false;
    for (int frame = 0; frame < 160; ++frame) {
        widget->step();
        auto vg = APP->window->vg;
        nvgBeginFrame(vg, canvas_width, canvas_height, ratio);
        Widget::DrawArgs args = {};
        args.vg = vg;
        args.clipBox = Rect(Vec(), Vec(canvas_width, canvas_height));
        APP->window->fbCount() = 0;
        nvgSave(vg);
        nvgTranslate(vg, 10.f, 20.f);
        widget->draw(args);
        widget->drawLayer(args, 1);
        nvgRestore(vg);
        glViewport(0, 0, width, height);
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgEndFrame(vg);
        glFinish();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL draw failed");
        if (ready(widget) && frame >= 3) { settled = true; break; }
    }
    if (!settled) throw std::runtime_error("Component framebuffers did not settle");
    std::vector<unsigned char> pixels(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Framebuffer read failed");
    std::ofstream output(filename, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        output.write(reinterpret_cast<const char*>(pixels.data() + y * width * 3), width * 3);
    if (!output) throw std::runtime_error("Cannot write " + filename);
    return ratio;
}

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
            for (bool preview : {false, true}) {
                random::local().seed(0x504f5441544fULL, 0x4348495053ULL);
                auto module = preview ? nullptr : model->createModule();
                if (module) {
                    module->id = index + 1;
                    context.engine->addModule(module);
                    // Fixed defaults, no patched signals or randomize/reset calls.
                    Module::ProcessArgs args = {};
                    args.sampleRate = 48000.f;
                    args.sampleTime = 1.f / args.sampleRate;
                    for (int sample = 0; sample < 4800; ++sample) {
                        args.frame = sample;
                        module->process(args);
                    }
                }
                settings::preferDarkPanels = false;
                std::unique_ptr<ModuleWidget> widget(model->createModuleWidget(module));
                if (widget->box.size != Vec(expected_width, 380))
                    throw std::runtime_error("Geometry changed: " + name);
                if (!preview) geometry(widget.get(), Vec(), json_object_get(report, "controls"));
                for (const std::string theme : {"Light", "Dark", "Light"}) {
                    settings::preferDarkPanels = theme == "Dark";
                    const int ratio = capture(widget.get(), std::string(argv[3]) + "/" + name
                        + (preview ? "-Preview-" : "-") + theme + ".ppm");
                    json_object_set_new(report, "pixel_ratio", json_integer(ratio));
                }
                // Real context lifecycle events, then another complete render.
                Widget::ContextDestroyEvent destroy;
                destroy.vg = context.window->vg;
                widget->onContextDestroy(destroy);
                Widget::ContextCreateEvent create;
                create.vg = context.window->vg;
                widget->onContextCreate(create);
                capture(widget.get(), std::string(argv[3]) + "/" + name
                    + (preview ? "-Preview-Restored.ppm" : "-Restored.ppm"));
            }
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
    contextSet(nullptr);
    return result;
}
