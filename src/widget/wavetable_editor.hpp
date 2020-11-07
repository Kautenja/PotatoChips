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

#include "rack.hpp"
#include <cstdint>
#include <algorithm>

#ifndef WIDGETS_WAVETABLE_EDITOR_HPP_
#define WIDGETS_WAVETABLE_EDITOR_HPP_

/// @brief An action for an update to a wavetable.
template<typename Wavetable>
struct WaveTableAction : rack::history::Action {
 private:
    /// the vector containing the waveform
    Wavetable* const waveform;
    /// the length of the wave-table to edit
    const uint32_t length;
    /// the waveform before the edit
    Wavetable* before;
    /// the waveform after the edit
    Wavetable* after;

 public:
    /// @brief Initialize a new wavetable update action.
    explicit WaveTableAction(Wavetable* waveform_, uint32_t length_) :
        rack::history::Action(),
        waveform(waveform_),
        length(length_) {
        name = "KautenjaDSP WaveTableEditorAction";
        before = new Wavetable[length];
        after = new Wavetable[length];
    }

    /// @brief Delete the action.
    ~WaveTableAction() { delete[] before; delete[] after; }

    /// @brief copy the waveform into the before buffer.
    inline void copy_before() { std::copy(waveform, waveform + length, before); }

    /// @brief copy the waveform into the after buffer.
    inline void copy_after() { std::copy(waveform, waveform + length, after); }

    /// @brief Return true if the action is a commit-able update.
    inline bool is_diff() { return std::memcmp(before, after, length); }

    /// @brief De-commit the action.
    inline void undo() final { std::copy(before, before + length, waveform); }

    /// @brief Commit the action.
    inline void redo() final { std::copy(after, after + length, waveform); }
};

/// A widget that displays / edits a wave-table.
template<typename Wavetable>
struct WaveTableEditor : rack::LightWidget {
 private:
    /// the vector containing the waveform
    Wavetable* const waveform;
    /// the length of the wave-table to edit
    const uint32_t length;
    /// the bit depth of the waveform
    const uint64_t bit_depth;
    /// the fill color for the widget
    NVGcolor fill;
    /// the background color for the widget
    NVGcolor background;
    /// the border color for the widget
    NVGcolor border;
    /// the state of the drag operation
    struct {
        /// whether a drag is currently active
        bool is_pressed = false;
        /// whether a drag is currently active
        bool is_active = false;
        /// whether the drag operation is being modified
        bool is_modified = false;
        /// the current position of the mouse pointer during the drag
        rack::Vec position = {0, 0};
    } drag_state;

 public:
    /// @brief Initialize a new wave-table editor widget.
    ///
    /// @param waveform_ the waveform buffer to read and update
    /// @param length_ the length of the wave-table to edit
    /// @param bit_depth_ the bit-depth of the waveform samples to generate
    /// @param position the position of the screen on the module
    /// @param size the output size of the display to render
    /// @param fill_ the fill color for the widget
    /// @param background_ the background color for the widget
    /// @param border_ the border color for the widget
    ///
    explicit WaveTableEditor(
        Wavetable* waveform_,
        uint32_t length_,
        uint64_t bit_depth_,
        rack::Vec position,
        rack::Vec size,
        NVGcolor fill_ =       {{{0.f,  0.f,  1.f,  1.f}}},
        NVGcolor background_ = {{{0.f,  0.f,  0.f,  1.f}}},
        NVGcolor border_ =     {{{0.2f, 0.2f, 0.2f, 1.f}}}
    ) :
        rack::LightWidget(),
        waveform(waveform_),
        length(length_),
        bit_depth(bit_depth_),
        fill(fill_),
        background(background_),
        border(border_) {
        setPosition(position);
        setSize(size);
    }

    /// the active action to commit to history
    WaveTableAction<Wavetable>* action = nullptr;

    /// Respond to a button event on this widget.
    void onButton(const rack::event::Button &e) override {
        // consume the event to prevent it from propagating
        e.consume(this);
        // setup the drag state
        drag_state.is_active = e.button == GLFW_MOUSE_BUTTON_LEFT;
        drag_state.is_modified = e.mods & GLFW_MOD_CONTROL;
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {  // right click event
            // TODO: show menu with basic waveforms
            return;
        }
        // return if the drag operation is not active
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        // set the position of the drag operation to the position of the mouse
        drag_state.position = e.pos;
        // calculate the normalized x position in [0, 1]
        float x = drag_state.position.x / box.size.x;
        x = rack::math::clamp(x, 0.f, 1.f);
        // calculate the position in the wave-table
        uint32_t index = x * length;
        // calculate the normalized y position in [0, 1]
        // y increases downward in pixel space, so invert about 1
        float y = 1.f - drag_state.position.y / box.size.y;
        y = rack::math::clamp(y, 0.f, 1.f);
        // calculate the value of the wave-table at this index
        uint64_t value = y * bit_depth;
        // if the action is a press copy the waveform before updating
        if (e.action == GLFW_PRESS) {
            drag_state.is_pressed = true;
            action = new WaveTableAction<Wavetable>(waveform, length);
            action->copy_before();
        }
        // update the waveform, we need to check if the button is pressed
        // this could be a mouse up event from a click that started somewhere
        // else
        if (drag_state.is_pressed) {
            waveform[index] = value;
        }
    }

    /// Respond to drag move event on this widget.
    void onDragMove(const rack::event::DragMove &e) override {
        // consume the event to prevent it from propagating
        e.consume(this);
        // if the drag operation is not active, return early
        if (!(drag_state.is_active && drag_state.is_pressed)) return;
        // update the drag state based on the change in position from the mouse
        uint32_t index = length * rack::math::clamp(drag_state.position.x / box.size.x, 0.f, 1.f);
        drag_state.position.x += e.mouseDelta.x / APP->scene->rackScroll->zoomWidget->zoom;
        uint32_t next_index = length * rack::math::clamp(drag_state.position.x / box.size.x, 0.f, 1.f);
        drag_state.position.y += e.mouseDelta.y / APP->scene->rackScroll->zoomWidget->zoom;
        // calculate the normalized y position in [0, 1]
        // y increases downward in pixel space, so invert about 1
        float y = 1.f - drag_state.position.y / box.size.y;
        y = rack::math::clamp(y, 0.f, 1.f);
        // calculate the value of the wave-table at this index
        uint64_t value = y * bit_depth;
        if (next_index < index)  // swap next index if it's less the current
            (index ^= next_index), (next_index ^= index), (index ^= next_index);
        // update the waveform (use memset for SIMD; opposed to a loop)
        memset(waveform + index, value, next_index - index);
    }

    /// Respond to drag end event on this widget.
    void onDragEnd(const rack::event::DragEnd &e) override {
        // consume the event to prevent it from propagating
        e.consume(this);
        // disable the drag state and commit the action
        drag_state.is_pressed = false;
        action->copy_after();
        if (action->is_diff()) APP->history->push(action);
    }

    /// @brief Draw the display on the main context.
    ///
    /// @param args the arguments for the draw context for this widget
    ///
    void draw(const DrawArgs& args) override {
        // the x position of the widget
        static constexpr int x = 0;
        // the y position of the widget
        static constexpr int y = 0;
        // the radius for the corner on the rectangle
        static constexpr int corner_radius = 3;
        // arbitrary padding
        static constexpr int pad = 1;
        // -------------------------------------------------------------------
        // draw the background
        // -------------------------------------------------------------------
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, x - pad, y - pad, box.size.x + 2 * pad, box.size.y + 2 * pad, corner_radius);
        nvgFillColor(args.vg, background);
        nvgFill(args.vg);
        nvgClosePath(args.vg);
        // -------------------------------------------------------------------
        // draw the waveform
        // -------------------------------------------------------------------
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgScissor(args.vg, x, y, box.size.x, box.size.y);
        // get the start pixel for the path (first sample in the table)
        auto startY = box.size.y * (bit_depth - waveform[0]) / static_cast<float>(bit_depth);
        nvgMoveTo(args.vg, x, startY);
        for (uint32_t i = 0; i < length; i++) {
            auto pixelX = box.size.x * i / static_cast<float>(length);
            auto pixelY = box.size.y * (bit_depth - waveform[i]) / static_cast<float>(bit_depth);
            nvgLineTo(args.vg, pixelX, pixelY);
        }
        nvgMoveTo(args.vg, x, startY);
        nvgStrokeColor(args.vg, fill);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        nvgRestore(args.vg);
        // -------------------------------------------------------------------
        // draw the border
        // -------------------------------------------------------------------
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, x - pad, y - pad, box.size.x + 2 * pad, box.size.y + 2 * pad, corner_radius);
        nvgStrokeColor(args.vg, border);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
    }
};

#endif  // WIDGETS_WAVETABLE_EDITOR_HPP_
