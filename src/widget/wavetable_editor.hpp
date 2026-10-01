// A VCV Rack widget for viewing and editing samples in waveform.
// Copyright 2020 Christian Kauten
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef WIDGETS_WAVETABLE_EDITOR_HPP_
#define WIDGETS_WAVETABLE_EDITOR_HPP_

#include <rack.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include "../rack_extensions/wavetable.hpp"

/// History resolves a module ID so deletion and deletion-undo cannot leave a
/// dangling buffer pointer or target the storage of a former module instance.
struct WaveTableAction : rack::history::ModuleAction {
    using Samples = std::array<uint8_t, WavetableBank::SAMPLES>;
    unsigned page;
    Samples before{};
    Samples after{};

    WaveTableAction(int64_t id, unsigned page_) : page(page_) {
        moduleId = id;
        name = "Arhythmetic Units wavetable edit";
    }

    static Samples snapshot(const WavetableBank& bank, unsigned page) {
        Samples result{};
        if (page < WavetableBank::TABLES)
            for (unsigned i = 0; i < result.size(); ++i)
                result[i] = bank.samples[page][i].load(std::memory_order_relaxed);
        return result;
    }

    void apply(const Samples& samples) {
        if (!APP || !APP->engine || page >= WavetableBank::TABLES) return;
        auto owner = dynamic_cast<WavetableOwner*>(APP->engine->getModule(moduleId));
        if (!owner) return;
        for (unsigned i = 0; i < samples.size(); ++i)
            owner->waveBank->samples[page][i].store(samples[i], std::memory_order_relaxed);
    }

    void undo() override { apply(before); }
    void redo() override { apply(after); }
};

/// The browser draws a private, read-only preview. Live editors hold only a weak
/// reference, so they stop editing when their module's storage disappears.
struct WaveTableEditor : rack::TransparentWidget {
    std::weak_ptr<WavetableBank> bank;
    WaveTableAction::Samples preview{};
    int64_t moduleId = -1;
    unsigned page;
    unsigned length;
    unsigned bit_depth;
    NVGcolor fill, background, border;
    rack::Vec dragPosition;
    bool cursorLocked = false;
    std::unique_ptr<WaveTableAction> action;

    WaveTableEditor(rack::engine::Module* module, unsigned page_, const uint8_t* preview_,
        unsigned length_, unsigned bit_depth_, rack::Vec position, rack::Vec size,
        NVGcolor fill_ = {{{0.f, 0.f, 1.f, 1.f}}},
        NVGcolor background_ = {{{0.f, 0.f, 0.f, 1.f}}},
        NVGcolor border_ = {{{0.2f, 0.2f, 0.2f, 1.f}}}) :
        page(page_), length(std::min(length_, unsigned(WavetableBank::SAMPLES))),
        bit_depth(std::min(bit_depth_, 255u)), fill(fill_), background(background_), border(border_) {
        setPosition(position);
        setSize(size);
        if (auto owner = dynamic_cast<WavetableOwner*>(module)) {
            bank = owner->waveBank;
            moduleId = module->id;
        }
        if (preview_) std::copy(preview_, preview_ + length, preview.begin());
    }

    ~WaveTableEditor() override { unlockCursor(); }

    void unlockCursor() {
        if (cursorLocked && APP && APP->window) APP->window->cursorUnlock();
        cursorLocked = false;
    }

    bool editable() const {
        return page < WavetableBank::TABLES && length && bit_depth &&
            box.size.x > 0 && box.size.y > 0 &&
            std::isfinite(box.size.x) && std::isfinite(box.size.y) && !bank.expired();
    }

    unsigned sampleIndex(float x) const {
        float normalized = std::max(0.f, std::min(1.f, x / box.size.x));
        return std::min(length - 1, static_cast<unsigned>(normalized * length));
    }

    /// Inclusive endpoints also allow vertical edits within a single sample.
    void edit(rack::Vec from, rack::Vec to) {
        if (!editable() || !std::isfinite(from.x) || !std::isfinite(to.x) || !std::isfinite(to.y)) return;
        auto storage = bank.lock();
        if (!storage) return;
        unsigned first = sampleIndex(from.x), last = sampleIndex(to.x);
        if (first > last) std::swap(first, last);
        float y = std::max(0.f, std::min(1.f, 1.f - to.y / box.size.y));
        uint8_t value = y * bit_depth;
        for (unsigned i = first; i <= last; ++i)
            storage->samples[page][i].store(value, std::memory_order_relaxed);
    }

    void onButton(const rack::event::Button& e) override {
        e.consume(this);
        if (e.action != GLFW_PRESS) return;
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (auto widget = getAncestorOfType<rack::app::ModuleWidget>())
                if (widget->module) widget->createContextMenu();
            return;
        }
        if (e.button != GLFW_MOUSE_BUTTON_LEFT || !editable()) return;
        auto storage = bank.lock();
        if (!storage) return;
        action.reset(new WaveTableAction(moduleId, page));
        action->before = WaveTableAction::snapshot(*storage, page);
        dragPosition = e.pos;
        edit(e.pos, e.pos);
    }

    void onDragStart(const rack::event::DragStart& e) override {
        if (action && APP && APP->window) {
            APP->window->cursorLock();
            cursorLocked = true;
        }
        e.consume(this);
    }

    void onDragMove(const rack::event::DragMove& e) override {
        e.consume(this);
        if (!action) return;
        float zoom = APP && APP->scene ? APP->scene->rackScroll->zoomWidget->zoom : 1.f;
        auto next = dragPosition.plus(e.mouseDelta.div(zoom));
        edit(dragPosition, next);
        dragPosition = next;
    }

    void onDragEnd(const rack::event::DragEnd& e) override {
        unlockCursor();
        e.consume(this);
        auto storage = bank.lock();
        if (action && storage && APP && APP->history) {
            action->after = WaveTableAction::snapshot(*storage, page);
            if (action->before != action->after) APP->history->push(action.release());
        }
        action.reset();
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, -1, -1, box.size.x + 2, box.size.y + 2, 3);
            nvgFillColor(args.vg, background);
            nvgFill(args.vg);
            nvgClosePath(args.vg);
            auto samples = preview;
            if (auto storage = bank.lock()) samples = WaveTableAction::snapshot(*storage, page);
            if (length && bit_depth) {
                nvgSave(args.vg);
                nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
                nvgBeginPath(args.vg);
                for (unsigned i = 0; i < length; ++i) {
                    float x = box.size.x * i / length;
                    float y = box.size.y * (static_cast<float>(bit_depth) - samples[i]) / bit_depth;
                    if (i == 0) nvgMoveTo(args.vg, x, y);
                    else nvgLineTo(args.vg, x, y);
                }
                nvgStrokeColor(args.vg, fill);
                nvgStroke(args.vg);
                nvgClosePath(args.vg);
                nvgRestore(args.vg);
            }
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, -1, -1, box.size.x + 2, box.size.y + 2, 3);
            nvgStrokeColor(args.vg, border);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
        }
        rack::TransparentWidget::drawLayer(args, layer);
    }
};

#endif  // WIDGETS_WAVETABLE_EDITOR_HPP_
