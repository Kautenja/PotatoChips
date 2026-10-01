// Native Echo interaction and rendering probes for spec 006.
// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CAPTURE_INSPECT_SUPER_ECHO_HPP_
#define CAPTURE_INSPECT_SUPER_ECHO_HPP_

#include "../../src/widget/horizontal_light_slider.hpp"

static void echoRequire(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Echo: " + message);
}

template <typename T>
static T* echoFind(Widget* widget) {
    if (auto result = dynamic_cast<T*>(widget)) return result;
    for (auto child : widget->children)
        if (auto result = echoFind<T>(child)) return result;
    return nullptr;
}

/// Use Rack's real widget events/history, then render lights-off and CV states.
static void inspectSuperEcho(ModuleWidget* widget, const std::string& path) {
    auto module = widget->module;
    std::vector<HorizontalLightSlider*> sliders;
    for (auto param : widget->getParams()) {
        if (auto slider = dynamic_cast<HorizontalLightSlider*>(param)) {
            const int i = sliders.size();
            echoRequire(slider->paramId == 4 + i, "coefficient binding/order");
            echoRequire(slider->getLight()->firstLightId == 12 + 3 * i, "RGB binding");
            echoRequire(slider->module == module && slider->getLight()->module == module,
                "module/light ownership");
            echoRequire(slider->box.pos == Vec(147, 29 + 43 * i)
                && slider->box.size == Vec(76, 20), "row geometry");
            echoRequire(slider->horizontal && slider->minHandlePos == Vec(0, 0)
                && slider->maxHandlePos == Vec(64, 0), "horizontal travel");
            sliders.push_back(slider);
        }
    }
    echoRequire(sliders.size() == 8, "eight distinct coefficient sliders");
    if (!module) return;
    echoRequire(widget->getParams().size() == 23, "exactly one widget per parameter");
    for (int i = 0; i < 8; ++i) {
        auto slider = sliders[i];
        auto quantity = slider->getParamQuantity();
        for (float value : {-128.f, 0.f, 127.f}) {
            quantity->setValue(value);
            widget->step();
            const float expected_x = 64.f * (value + 128.f) / 255.f;
            echoRequire(std::abs(slider->handle->box.pos.x - expected_x) < .001f,
                "endpoint/zero handle position");
            echoRequire(slider->getLight()->box.getCenter()
                == slider->handle->box.getCenter(), "light follows handle");
        }
        // Both track ends must receive rectangular hit events.
        for (float x : {1.f, 75.f}) {
            widget::EventContext context;
            event::Hover hover;
            hover.context = &context;
            hover.pos = slider->box.pos.plus(Vec(x, 10));
            widget->onHover(hover);
            echoRequire(hover.getTarget() == slider, "track hit region");
        }
        quantity->setValue(0);
        const auto knob_mode = settings::knobMode;
        settings::knobMode = settings::KNOB_MODE_LINEAR;
        event::DragStart start;
        start.button = GLFW_MOUSE_BUTTON_LEFT;
        slider->onDragStart(start);
        event::DragMove move;
        move.button = GLFW_MOUSE_BUTTON_LEFT;
        move.mouseDelta = Vec(1000, 0);
        slider->onDragMove(move);
        event::DragEnd end;
        end.button = GLFW_MOUSE_BUTTON_LEFT;
        slider->onDragEnd(end);
        settings::knobMode = knob_mode;
        echoRequire(quantity->getValue() == 127, "rightward drag reaches maximum");
        APP->history->undo();
        echoRequire(quantity->getValue() == 0, "drag undo");
        APP->history->redo();
        echoRequire(quantity->getValue() == 127, "drag redo");
        // Submit the actual Rack context-menu numeric field.
        slider->createContextMenu();
        auto overlay = APP->scene->children.back();
        auto field = echoFind<ui::TextField>(overlay);
        echoRequire(field != nullptr, "numeric entry field");
        field->text = "-64.6";
        event::SelectKey key;
        key.action = GLFW_PRESS;
        key.key = GLFW_KEY_ENTER;
        field->onSelectKey(key);
        echoRequire(quantity->getValue() == -65, "numeric entry snaps");
        APP->scene->removeChild(overlay);
        delete overlay;
        APP->history->undo();
        echoRequire(quantity->getValue() == 127, "numeric entry undo");
        APP->history->redo();
        echoRequire(quantity->getValue() == -65, "numeric entry redo");
        slider->resetAction();
        echoRequire(quantity->getValue() == (i ? 0 : 127), "parameter reset");
    }
    // Both bypass positions and asymmetric stereo levels through the UI action.
    for (float bypass : {0.f, 1.f}) {
        for (const auto& setting : std::vector<std::pair<int, float>>{
            {2, -64}, {3, 96}, {20, .5f}, {21, 1.5f}, {22, bypass}})
            module->getParamQuantity(setting.first)->setImmediateValue(setting.second);
        for (int draw = 0; draw < 8; ++draw) {
            json_t* before = module->paramsToJson();
            widget->randomizeAction();
            echoRequire(module->params[2].getValue() == -64
                && module->params[3].getValue() == 96
                && module->params[20].getValue() == .5f
                && module->params[21].getValue() == 1.5f
                && module->params[22].getValue() == bypass, "randomize exclusions");
            json_t* after = module->paramsToJson();
            APP->history->undo();
            json_t* undone = module->paramsToJson();
            echoRequire(json_equal(before, undone), "randomize undo");
            APP->history->redo();
            json_t* redone = module->paramsToJson();
            echoRequire(json_equal(after, redone), "randomize redo");
            for (auto value : {before, after, undone, redone}) json_decref(value);
        }
    }
    widget->resetAction();
    for (auto quantity : module->paramQuantities)
        echoRequire(quantity->getValue() == quantity->defaultValue, "module reset");
    // Existing presets, loaded by the production widget, preserve coefficient order.
    for (const auto& preset : system::getEntries(asset::plugin(plugin_instance, "presets/SuperEcho"))) {
        widget->loadAction(preset);
        widget->step();
        json_t* saved = widget->toJson();
        widget->fromJson(saved);
        json_t* restored = widget->toJson();
        echoRequire(json_equal(saved, restored), "preset/patch round trip");
        json_decref(saved);
        json_decref(restored);
    }
    // Supplement the default captures with alternating endpoints/zero, bypass,
    // and actual positive/negative CV processing. Never synthesize LED values.
    widget->resetAction();
    for (auto& input : module->inputs) input.channels = 0;
    module->params[22].setValue(1);
    for (int i = 0; i < 8; ++i) module->params[4 + i].setValue(i % 3 == 0 ? -128 : i % 3 == 1 ? 0 : 127);
    for (bool cv : {false, true}) {
        for (int i = 0; i < 8; ++i) {
            module->inputs[6 + i].channels = cv ? 1 : 0;
            module->inputs[6 + i].setVoltage(cv ? (i % 2 ? -10 : 10) : 0);
            module->params[12 + i].setValue(1);
        }
        Module::ProcessArgs args = {};
        args.sampleRate = 48000;
        args.sampleTime = 1.f / args.sampleRate;
        for (int frame = 0; frame < 48000; ++frame) { args.frame = frame; module->process(args); }
        for (int i = 0; i < 8; ++i) {
            const float red = module->lights[12 + 3 * i].getBrightness();
            const float green = module->lights[13 + 3 * i].getBrightness();
            echoRequire(cv ? (i % 2 ? red > .9f && green == 0 : green > .9f && red == 0)
                : red == 0 && green == 0, "CV indicator polarity/lights off");
        }
        for (bool dark : {false, true}) {
            settings::preferDarkPanels = dark;
            const std::string name = path + (cv ? "-CV" : "-Off") + (dark ? "-Dark" : "-Light");
            capture(widget, name + ".ppm");
            capture(widget, name + "-Dim.ppm", 1.f, .5f);
            capture(widget, name + "-Zoom75.ppm", .75f);
            capture(widget, name + "-Zoom150.ppm", 1.5f);
        }
    }
}
#endif  // CAPTURE_INSPECT_SUPER_ECHO_HPP_
