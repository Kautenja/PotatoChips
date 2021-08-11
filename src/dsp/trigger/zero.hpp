// A trigger that detects when a signal goes from negative to positive.
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

#ifndef DSP_TRIGGER_ZERO_HPP
#define DSP_TRIGGER_ZERO_HPP

/// @brief A collection of structures for detecting trigger events.
namespace Trigger {

/// @brief A trigger that detects when a signal goes from negative to positive.
struct Zero {
 private:
    /// the value of the signal on the last sample, i.e., last call to process
    float lastSignal = 0.f;

 public:
    /// @brief Reset the trigger to its default state
    inline void reset() { lastSignal = 0.f; }

    /// @brief Process a step of the boolean signal.
    ///
    /// @param signal a sample of a boolean signal
    /// @returns true if the state changes from negative to positive
    ///
    inline bool process(float signal) {
        const bool triggered = lastSignal < 0.f && signal >= 0.f;
        lastSignal = signal;
        return triggered;
    }
};

}  // namespace Trigger

#endif  // DSP_TRIGGER_ZERO_HPP
