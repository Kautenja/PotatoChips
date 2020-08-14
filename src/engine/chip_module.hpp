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

#include "rack.hpp"
#include "../plugin.hpp"
#include "../dsp/blip_buffer.hpp"

#ifndef ENGINE_CHIP_MODULE_HPP_
#define ENGINE_CHIP_MODULE_HPP_

/// A chip emulator module.
/// @tparam ChipEmulator the class of the chip emulator
///
template<typename ChipEmulator>
struct ChipModule : rack::engine::Module {
 protected:
    /// The BLIP buffer to render audio samples from
    BLIPBuffer buffers[POLYPHONY_CHANNELS][ChipEmulator::OSC_COUNT];
    /// The 106 instance to synthesize sound with
    ChipEmulator apu[POLYPHONY_CHANNELS];

    /// a clock divider for running CV acquisition slower than audio rate
    rack::dsp::ClockDivider cvDivider;
    /// a clock divider for running LED updates slower than audio rate
    rack::dsp::ClockDivider lightDivider;

    // unsigned get_channels() {
    //     unsigned channels = 1;
    //     for (unsigned input = 0; input < sizeof inputs; input++)
    //         channels = std::max(inputs[input].getChannels(), static_cast<int>(channels));
    //     return channels;
    // }

 public:
    /// @brief Initialize a new Chip module.
    ChipModule() {
        // set the division of the CV and LED frame dividers
        cvDivider.setDivision(16);
        lightDivider.setDivision(128);
        // set the output buffer for each individual voice
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < ChipEmulator::OSC_COUNT; oscillator++)
                apu[channel].set_output(oscillator, &buffers[channel][oscillator]);
            // volume of 3 produces a 5V (10Vpp) signal from all voices
            apu[channel].set_volume(3.f);
        }
        // update the sample rate on the engine
        onSampleRateChange();
        // reset to restore any local data to the emulators
        onReset();
    }

    /// @brief Respond to the change of sample rate in the engine.
    inline void onSampleRateChange() override {
        // update the buffer for each oscillator and polyphony channel
        for (unsigned channel = 0; channel < POLYPHONY_CHANNELS; channel++) {
            for (unsigned oscillator = 0; oscillator < ChipEmulator::OSC_COUNT; oscillator++) {
                buffers[channel][oscillator].set_sample_rate(APP->engine->getSampleRate(), CLOCK_RATE);
            }
        }
    }
};


#endif  // ENGINE_CHIP_MODULE_HPP_
