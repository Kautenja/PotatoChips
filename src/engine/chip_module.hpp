// An abstract base class for module that use Blargg chip emulators.
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

#include <algorithm>
#include "rack.hpp"
#include "../plugin.hpp"
#include "../dsp/blip_buffer.hpp"

#ifndef ENGINE_CHIP_MODULE_HPP_
#define ENGINE_CHIP_MODULE_HPP_

// TODO: move BLIP Buffer inside the emulator class instead of using it here

/// @brief An abstract chip emulator module.
/// @tparam ChipEmulator the class of the chip emulator
/// @details All ports are assumed to be polyphonic. I.e., all inputs ports
/// allow a polyphonic signal and all output ports can emit polyphonic signals.
///
template<typename ChipEmulator>
struct ChipModule : rack::engine::Module {
 protected:
    /// The BLIP buffers to render audio samples from
    BLIPBuffer buffers[PORT_MAX_CHANNELS][ChipEmulator::OSC_COUNT];
    /// The chip emulators to synthesize sound with
    ChipEmulator apu[PORT_MAX_CHANNELS];

    /// a clock divider for running CV acquisition slower than audio rate
    rack::dsp::ClockDivider cvDivider;
    /// a clock divider for running LED updates slower than audio rate
    rack::dsp::ClockDivider lightDivider;

    /// a VU meter for measuring the output audio level from the emulator
    rack::dsp::VuMeter2 vuMeter[ChipEmulator::OSC_COUNT];

    /// whether the outputs should be normalled together into a mix
    bool normal_outputs = false;

    /// @brief Process the audio rate inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the audio inputs to
    ///
    virtual void processAudio(const rack::engine::Module::ProcessArgs &args, unsigned channel) { };

    /// @brief Process the CV inputs for the given channel.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channel the polyphonic channel to process the CV inputs to
    ///
    virtual void processCV(const rack::engine::Module::ProcessArgs &args, unsigned channel) = 0;

    /// @brief Process the lights on the module.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    /// @param channels the number of active polyphonic channels
    ///
    virtual void processLights(const rack::engine::Module::ProcessArgs &args, unsigned channels) = 0;

 public:
    /// @brief Initialize a new Chip module.
    ChipModule(float volume = 3.f) {
        // set the division of the CV and LED frame dividers
        cvDivider.setDivision(16);
        lightDivider.setDivision(512);
        // set the output buffer for each individual voice on each polyphonic
        // channel
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
            for (unsigned osc = 0; osc < ChipEmulator::OSC_COUNT; osc++)
                apu[channel].set_output(osc, &buffers[channel][osc]);
            apu[channel].set_volume(volume);
        }
        // update the sample rate on the engine
        onSampleRateChange();
        // reset to restore any local data to the emulators
        onReset();
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() final {
        // reset the CV and light divider clocks
        cvDivider.reset();
        lightDivider.reset();
        // update the buffer for each oscillator and polyphony channel
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
            for (unsigned osc = 0; osc < ChipEmulator::OSC_COUNT; osc++) {
                buffers[channel][osc].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
            }
        }
    }

    /// @brief Respond to the module being reset by the engine.
    inline void onReset() override {
        // reset the CV and light divider clocks
        cvDivider.reset();
        lightDivider.reset();
        // reset the audio processing unit for all poly channels
        for (unsigned channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
            apu[channel].reset();
        }
    }

    /// @brief Process a sample.
    ///
    /// @param args the sample arguments (sample rate, sample time, etc.)
    ///
    void process(const rack::engine::Module::ProcessArgs &args) final {
        // get the number of polyphonic channels (defaults to 1 for monophonic).
        // also set the channels on the output ports based on the number of
        // channels
        unsigned channels = 1;
        for (unsigned port = 0; port < inputs.size(); port++)
            channels = std::max(inputs[port].getChannels(), static_cast<int>(channels));
        // set the number of polyphony channels for output ports
        for (unsigned port = 0; port < outputs.size(); port++)
            outputs[port].setChannels(channels);
        // process the audio inputs to the chip using the overridden function
        for (unsigned channel = 0; channel < channels; channel++)
            processAudio(args, channel);
        // process the CV inputs to the chip using the overridden function
        if (cvDivider.process())
            for (unsigned channel = 0; channel < channels; channel++)
                processCV(args, channel);
        // process audio samples on the chip engine.
        for (unsigned channel = 0; channel < channels; channel++) {
            // end the frame on the engine
            apu[channel].end_frame(CLOCK_RATE / args.sampleRate);
            // get the output from each oscillator and set the output port
            for (unsigned osc = 0; osc < ChipEmulator::OSC_COUNT; osc++) {
                auto output = buffers[channel][osc].read_sample(5.f);
                if (normal_outputs) {  // mix outputs from previous voices
                    auto shouldNormal = osc && !outputs[osc - 1].isConnected();
                    auto lastOutput = shouldNormal ? outputs[osc - 1].getVoltage(channel) : 0.f;
                    output += lastOutput;
                }
                // update the VU meter with the un-clipped signal
                vuMeter[osc].process(args.sampleTime / channels, output / 5.f);
                // hard clip the output
                outputs[osc].setVoltage(math::clamp(output, -5.f, 5.f), channel);
            }
        }
        // process lights using the overridden function
        if (lightDivider.process()) processLights(args, channels);
    }
};

#endif  // ENGINE_CHIP_MODULE_HPP_
