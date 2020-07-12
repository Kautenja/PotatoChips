// A Namco 106 Chip module.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
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

#include "plugin.hpp"
#include "components.hpp"
#include "dsp/namco_106_apu.hpp"

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// A Namco 106 Chip module.
struct Chip106 : Module {
    enum ParamIds {
        PARAM_FREQ0,
        PARAM_COUNT
    };
    enum InputIds {
        INPUT_VOCT0,
        INPUT_FM0,
        INPUT_COUNT
    };
    enum OutputIds {
        ENUMS(OUTPUT_CHANNEL, 8),
        OUTPUT_COUNT
    };
    enum LightIds { LIGHT_COUNT };

    /// the clock rate of the module
    static constexpr uint64_t CLOCK_RATE = 768000;

    /// The BLIP buffer to render audio samples from
    BLIPBuffer buf[Namco106::OSC_COUNT];
    /// The 106 instance to synthesize sound with
    Namco106 apu;

    /// a signal flag for detecting sample rate changes
    bool new_sample_rate = true;

    /// Initialize a new 106 Chip module.
    Chip106() {
        config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);
        configParam(PARAM_FREQ0, -30.f, 30.f, 0.f, "Pulse 1 Frequency",  " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        // set the output buffer for each individual voice
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            apu.osc_output(i, &buf[i]);
            buf[i].set_clock_rate(CLOCK_RATE);
        }
        // volume of 3 produces a roughly 5Vpp signal from all voices
        apu.volume(3.f);
    }

    /// Return a 10V signed sample from the chip.
    ///
    /// @param channel the channel to get the audio sample for
    ///
    float getAudioOut(int channel) {
        // the peak to peak output of the voltage
        static constexpr float Vpp = 10.f;
        // the amount of voltage per increment of 16-bit fidelity volume
        static constexpr float divisor = std::numeric_limits<int16_t>::max();
        auto samples = buf[channel].samples_count();
        if (samples == 0) return 0.f;
        // copy the buffer to a local vector and return the first sample
        std::vector<int16_t> output_buffer(samples);
        buf[channel].read_samples(&output_buffer[0], samples);
        // convert the 16-bit sample to 10Vpp floating point
        return Vpp * output_buffer[0] / divisor;
    }

    /// Process a sample.
    void process(const ProcessArgs &args) override {
        // calculate the number of clock cycles on the chip per audio sample
        uint32_t cycles_per_sample = CLOCK_RATE / args.sampleRate;
        // check for sample rate changes from the engine to send to the chip
        if (new_sample_rate) {
            // update the buffer for each channel
            for (int i = 0; i < Namco106::OSC_COUNT; i++) {
                buf[i].set_sample_rate(args.sampleRate);
                buf[i].set_clock_rate(cycles_per_sample * args.sampleRate);
            }
            // clear the new sample rate flag
            new_sample_rate = false;
        }

        // a foo waveform
        static constexpr uint8_t values[32] = {
            0x00, 0x00, 0x00, 0xA8, 0xDC, 0xEE, 0xFF, 0xFF, 0xEF, 0xDE, 0xAC, 0x58, 0x23, 0x11, 0x00, 0x00,
            0x10, 0x21, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        // write the waveform to the RAM
        for (int i = 0; i < 32; i++) {
            apu.write_addr(i);
            apu.write_data(0, values[i]);
        }
        // write the wave address
        apu.write_addr(0x7E);
        apu.write_data(0, 0);

        // get the frequency of the oscillator from the parameter and CVs
        float pitch = params[PARAM_FREQ0].getValue() / 12.f;
        pitch += inputs[INPUT_VOCT0].getVoltage();
        float freq = rack::dsp::FREQ_C4 * powf(2.0, pitch);
        freq += 4 * inputs[INPUT_FM0].getVoltage();
        freq = rack::clamp(freq, 0.0f, 20000.0f);
        // convert the frequency to the 8-bit value for the oscillator
        auto num_channels = 2;
        auto num_samples = 64;
        auto wave_length = 64 - (num_samples / 16);
        freq *= (wave_length * num_channels * 15.f * 65536.f) / CLOCK_RATE;
        freq = rack::clamp(freq, 4.f, 262143.f);
        // extract the low, medium, and high frequency register values
        auto freq18bit = static_cast<uint32_t>(freq);
        // FREQUENCY LOW
        uint8_t low = (freq18bit & 0b000000000011111111) >> 0;
        apu.write_addr(0x78);
        apu.write_data(0, low);
        // FREQUENCY MEDIUM
        uint8_t med = (freq18bit & 0b001111111100000000) >> 8;
        apu.write_addr(0x7A);
        apu.write_data(0, med);
        // WAVEFORM LENGTH + FREQUENCY HIGH
        uint8_t hig = (freq18bit & 0b110000000000000000) >> 16;
        apu.write_addr(0x7C);
        apu.write_data(0, (48 << 2) + hig);

        // volume and channel selection
        static constexpr uint8_t volume = 0b00001111;
        apu.write_addr(0x7F);
        apu.write_data(0, (num_channels << 4) + volume);

        // set the output from the oscillators (in reverse order)
        apu.end_frame(cycles_per_sample);
        for (int i = 0; i < Namco106::OSC_COUNT; i++) {
            buf[i].end_frame(cycles_per_sample);
            auto channel = (Namco106::OSC_COUNT - 1) + OUTPUT_CHANNEL - i;
            outputs[channel].setVoltage(getAudioOut(i));
        }
    }

    /// Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override { new_sample_rate = true; }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// A widget that displays / edits a wave-table.
struct WaveTableEditor : OpaqueWidget {
 private:
    /// the background color for the widget
    NVGcolor background;
    /// the fill color for the widget
    NVGcolor fill;
    /// the border color for the widget
    NVGcolor border;
    /// the state of the drag operation
    struct {
        /// whether a drag is currently active
        bool is_active = false;
        /// whether the drag operation is being modified
        bool is_modified = false;
        /// the current position of the mouse pointer during the drag
        Vec position = {0, 0};
    } drag_state;
    /// the length of the wave-table to edit
    uint32_t length;
    /// the bit depth of the waveform
    uint64_t bit_depth;
    /// the vector containing the waveform
    std::vector<uint64_t> waveform;

 public:
    /// @brief Initialize a new wave-table editor widget.
    ///
    /// @param position the position of the screen on the module
    /// @param size the output size of the display to render
    /// @param background_ the background color for the widget
    /// @param fill_ the fill color for the widget
    /// @param fill_ the border color for the widget
    /// @param length_ the length of the wave-table to edit
    /// @param bit_depth_ the bit-depth of the waveform samples to generate
    ///
    explicit WaveTableEditor(
        Vec position,
        Vec size,
        NVGcolor background_,
        NVGcolor fill_,
        NVGcolor border_,
        uint32_t length_,
        uint64_t bit_depth_
    ) :
        OpaqueWidget(),
        background(background_),
        fill(fill_),
        border(border_),
        length(length_),
        bit_depth(bit_depth_) {
        setPosition(position);
        setSize(size);
        waveform.resize(length, 0);
    }

    /// @brief Update a sample in the wave-table.
    ///
    /// @param index the index of the value in the wavetable to update
    /// @param value the value of the waveform for the given index
    ///
    void update_position(uint32_t index, uint64_t value) {
        waveform[index] = value;
    }

    /// Respond to a button event on this widget.
    void onButton(const event::Button &e) override {
        OpaqueWidget::onButton(e);
        // consume the event to prevent it from propagating
        e.consume(this);
        // setup the drag state
        drag_state.is_active = e.button == GLFW_MOUSE_BUTTON_LEFT;
        drag_state.is_modified = e.mods & GLFW_MOD_CONTROL;
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {  // handle right click event
            // TODO: show menu with basic waveforms
        }
        // return if the drag operation is not active
        if (!drag_state.is_active) return;
        // set the position of the drag operation to the position of the mouse
        drag_state.position = e.pos;
        // calculate the normalized x position in [0, 1]
        float x = drag_state.position.x / box.size.x;
        x = math::clamp(x, 0.f, 1.f);
        // calculate the position in the wave-table
        uint32_t index = x * length;
        // calculate the normalized y position in [0, 1]
        // y increases downward in pixel space, so invert about 1
        float y = 1.f - drag_state.position.y / box.size.y;
        y = math::clamp(y, 0.f, 1.f);
        // calculate the value of the wave-table at this index
        uint64_t value = y * bit_depth;
        update_position(index, value);
    }

    /// Respond to drag move event on this widget.
    void onDragMove(const event::DragMove &e) override {
        OpaqueWidget::onDragMove(e);
        // consume the event to prevent it from propagating
        e.consume(this);
        // if the drag operation is not active, return early
        if (!drag_state.is_active) return;
        // update the drag state based on the change in position from the mouse
        uint32_t index = length * math::clamp(drag_state.position.x / box.size.x, 0.f, 1.f);
        drag_state.position.x += e.mouseDelta.x / APP->scene->rackScroll->zoomWidget->zoom;
        uint32_t next_index = length * math::clamp(drag_state.position.x / box.size.x, 0.f, 1.f);
        drag_state.position.y += e.mouseDelta.y / APP->scene->rackScroll->zoomWidget->zoom;
        // calculate the normalized y position in [0, 1]
        // y increases downward in pixel space, so invert about 1
        float y = 1.f - drag_state.position.y / box.size.y;
        y = math::clamp(y, 0.f, 1.f);
        // calculate the value of the wave-table at this index
        uint64_t value = y * bit_depth;
        if (next_index < index)  // swap next index if it's less the current
            (index ^= next_index), (next_index ^= index), (index ^= next_index);
        for (; index < next_index; index++)  // update the positions
            update_position(index, value);
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
        OpaqueWidget::draw(args);
        // -------------------------------------------------------------------
        // draw the background
        // -------------------------------------------------------------------
        nvgBeginPath(args.vg);
        nvgRect(args.vg, x, y, box.size.x, box.size.y);
        nvgFillColor(args.vg, background);
        nvgFill(args.vg);
        nvgClosePath(args.vg);
        // -------------------------------------------------------------------
        // draw the waveform
        // -------------------------------------------------------------------
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, box.size.y);
        for (int i = 0; i < length; i++) {
            auto pixelX = box.size.x * i / static_cast<float>(length);
            auto pixelY = box.size.y * (bit_depth - waveform[i]) / static_cast<float>(bit_depth);
            nvgLineTo(args.vg, pixelX, pixelY);
        }
        nvgMoveTo(args.vg, 0, box.size.y);
        nvgStrokeColor(args.vg, fill);
        nvgClosePath(args.vg);
        nvgStroke(args.vg);
        // -------------------------------------------------------------------
        // draw the border
        // -------------------------------------------------------------------
        nvgBeginPath(args.vg);
        nvgRect(args.vg, x, y, box.size.x, box.size.y);
        nvgStrokeColor(args.vg, border);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
    }
};

/// The widget structure that lays out the panel of the module and the UI menus.
struct Chip106Widget : ModuleWidget {
    WaveTableEditor* table_editor = nullptr;
    Chip106Widget(Chip106 *module) {
        setModule(module);
        static const auto panel = "res/106.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        // panel screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        // add the wavetable editor
        table_editor = new WaveTableEditor(
            Vec(RACK_GRID_WIDTH, 110),                    // position
            Vec(box.size.x - 2 * RACK_GRID_WIDTH, 80),    // size
            {.r = 0,   .g = 0,   .b = 0,   .a = 1  },     // background color
            {.r = 0,   .g = 0,   .b = 1,   .a = 1  },     // fill color
            {.r = 0.2, .g = 0.2, .b = 0.2, .a = 1  },     // border color
            32,                                           // wave-table length
            15                                            // waveform bit depth
        );
        addChild(table_editor);
        // V/OCT inputs
        addInput(createInput<PJ301MPort>(Vec(28, 74), module, Chip106::INPUT_VOCT0));
        // FM inputs
        addInput(createInput<PJ301MPort>(Vec(33, 32), module, Chip106::INPUT_FM0));
        // Frequency parameters
        addParam(createParam<Rogan3PSNES>(Vec(62, 42), module, Chip106::PARAM_FREQ0));
        // channel outputs
        addOutput(createOutput<PJ301MPort>(Vec(114, 74), module, Chip106::OUTPUT_CHANNEL));
    }
};

/// the global instance of the model
Model *modelChip106 = createModel<Chip106, Chip106Widget>("106");
