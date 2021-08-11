// A trigger that detects when a boolean changes from false to true.
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

#ifndef DSP_TRIGGER_BOOLEAN_HPP
#define DSP_TRIGGER_BOOLEAN_HPP

/// @brief A collection of structures for detecting trigger events.
namespace Trigger {

/// @brief A trigger that detects when a boolean changes from false to true.
struct Boolean {
 private:
    /// the current state of the trigger
    bool state = false;

 public:
    /// @brief Return the state of the boolean trigger.
    inline bool isHigh() const { return state; }

    /// @brief Reset the trigger to its default state
    inline void reset() { state = false; }

    /// @brief Process a step of the boolean signal.
    ///
    /// @param signal a sample of a boolean signal
    /// @returns true if the state changes from false to true
    ///
    inline bool process(bool signal) {
        const bool triggered = signal && !state;
        state = signal;
        return triggered;
    }
};

}  // namespace Trigger

#endif  // DSP_TRIGGER_BOOLEAN_HPP
