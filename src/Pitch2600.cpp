// A V/OCT distortion effect based on Atari 2600 music programming.
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

// TODO: sync input
// TODO: different note modes: 2, 3, 4, 5
// TODO: offset control between notes (models for musical / video-game based)
// TODO: mess with non-uniform offsets for the cycle apportionment of notes
// TODO: internal slew limiter that only slews internal note changes, not note
// changes in the input V/OCT signal. i.e., allow the changes to be blended
// between analog and discrete

#include "plugin.hpp"

// static float powf2_table(float value) {
//     static constexpr int twos[] = {
//         1<<0,  1<<1,  1<<2,  1<<3,  1<<4,  1<<5,  1<<6,  1<<7,
//         1<<8,  1<<9,  1<<10, 1<<11, 1<<12, 1<<13, 1<<14, 1<<15,
//         1<<16, 1<<17, 1<<18, 1<<19, 1<<20, 1<<21, 1<<22, 1<<23,
//         1<<24, 1<<25, 1<<26, 1<<27, 1<<28, 1<<29, 1<<30, 1<<31
//     };
//     const int value_lower = floorf(value);
//     const int lower = std::lower_bound(std::begin(twos), std::end(twos), value_lower) - std::begin(twos);
//     const int value_upper = ceilf(value);
//     const int upper = std::lower_bound(std::begin(twos), std::end(twos), value_upper) - std::begin(twos);
//     // linearly interpolate between the two values
//     return lower + (value - value_lower) * (upper - lower);
// }

/// @brief Convert the given pitch in V/OCT to frequency in \f$Hz\f$.
///
/// @param pitch the pitch value in V/OCT to convert to frequency in \f$Hz\f$
/// @param tuning the base tuning for the system in \f$Hz\f$ (default is C4)
/// @returns the frequency in \f$Hz\f$ based on the pitch tuning
///
static inline float pitch_to_frequency(
    float pitch,
    float tuning = rack::dsp::FREQ_C4
) {
    return tuning * powf(2.f, pitch);
}

/// @brief Convert the given frequency in \f$Hz\f$ to pitch in V/OCT.
///
/// @param frequency the frequency in \f$Hz\f$ to convert to V/OCT
/// @param tuning the base tuning for the system in \f$Hz\f$ (default is C4)
/// @returns the pitch in V/OCT based on the frequency and tuning
///
static inline float frequency_to_pitch(
    float frequency,
    float tuning = rack::dsp::FREQ_C4
) {
    return log2(frequency / tuning);
}

// ---------------------------------------------------------------------------
// MARK: Module
// ---------------------------------------------------------------------------

/// @brief A V/OCT distortion effect based on Atari 2600 music programming.
struct Pitch2600 : rack::engine::Module {
    /// the indexes of parameters (knobs, switches, etc.) on the module
    enum ParamIds {
        PARAM_FREQ,
        NUM_PARAMS
    };

    /// the indexes of input ports on the module
    enum InputIds {
        INPUT_VOCT,
        NUM_INPUTS
    };

    /// the indexes of output ports on the module
    enum OutputIds {
        OUTPUT_VOCT,
        NUM_OUTPUTS
    };

    /// the indexes of lights on the module
    enum LightIds {
        NUM_LIGHTS
    };

    /// the phase counter for cycling between different frequency offsets
    float phase = 0.f;
    /// whether key-scaling is enabled for the internal arpeggiator clock
    bool key_scaling = false;

    /// @brief Initialize a new module.
    Pitch2600() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PARAM_FREQ, -7.f, 7.f, 0.f, "Refresh Rate", " Hz", dsp::FREQ_SEMITONE, 60.f);
    }

    /// @brief Reset the module to its initial state.
    inline void onReset() final { phase = 0.f; }

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    inline void process(const ProcessArgs &args) final {
        // get the input frequency from the V/OCT port
        const float frequency_input = pitch_to_frequency(inputs[INPUT_VOCT].getVoltage());
        // get the frequency of the internal clock. if key-scaling is enabled,
        // offset the frequency by 100th of the input frequency.
        const float frequency_clock = key_scaling ?
            // pitch_to_frequency(params[PARAM_FREQ].getValue() + inputs[INPUT_VOCT].getVoltage() / 5.f) :
            pitch_to_frequency(params[PARAM_FREQ].getValue()) + frequency_input / 100.f :
            pitch_to_frequency(params[PARAM_FREQ].getValue());
        // get the change in phase based on the clock frequency
        const float deltaPhase = rack::clamp(frequency_clock * args.sampleTime, 1e-6f, 0.5f);
        // increment the phase counter
        phase += deltaPhase;
        // wrap the phase counter if past the boundary of 1
        if (phase >= 1.f) phase -= 1.f;

        float output_frequency = frequency_input;
        if (phase < 0.25) {         // root note
            // do nothing
        } else if (phase < 0.5) {   // note 1
            output_frequency -= 28.5;
        } else if (phase < 0.75) {  // note 2
            output_frequency -= 53.9;
        } else {                    // note 3
            output_frequency -= 76.6;
        }
        outputs[OUTPUT_VOCT].setVoltage(frequency_to_pitch(output_frequency));

        // float output_frequency = frequency_input;
        // if (phase < 0.33) {         // root note
        //     // do nothing
        // } else if (phase < 0.66) {  // note 1
        //     output_frequency -= 28.5;
        // } else {                    // note 2
        //     output_frequency -= 53.9;
        // }
        // outputs[OUTPUT_VOCT].setVoltage(frequency_to_pitch(output_frequency));

        // float output_frequency = frequency_input;
        // if (phase < 0.5) {  // root note
        //     // do nothing
        // } else {            // note 1
        //     output_frequency -= 28.5;
        // }
        // outputs[OUTPUT_VOCT].setVoltage(frequency_to_pitch(output_frequency));
    }
};

// ---------------------------------------------------------------------------
// MARK: Widget
// ---------------------------------------------------------------------------

/// @brief The panel widget for the Pitch2600 module.
struct Pitch2600Widget : ModuleWidget {
    /// @brief Initialize a new panel widget.
    ///
    /// @param module the Pitch2600 module to interact with
    ///
    explicit Pitch2600Widget(Pitch2600 *module) {
        setModule(module);
        static constexpr auto panel = "res/StepSaw.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        addInput(createInput<PJ301MPort>(  Vec(10, 20), module, Pitch2600::INPUT_VOCT ));
        addOutput(createOutput<PJ301MPort>(Vec(10, 55), module, Pitch2600::OUTPUT_VOCT));
        addParam(createParam<Trimpot>(     Vec(10, 90), module, Pitch2600::PARAM_FREQ ));
    }
};

/// the global instance of the VCV Rack module
rack::Model *modelPitch2600 = createModel<Pitch2600, Pitch2600Widget>("Pitch2600");
