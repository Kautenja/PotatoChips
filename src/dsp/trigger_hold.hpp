// A trigger that detects a threshold value held for a period of time.
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

#ifndef DSP_TRIGGER_HOLD_HPP
#define DSP_TRIGGER_HOLD_HPP

/// @brief A collection of structures for detecting trigger events.
namespace Trigger {

/// @brief A trigger that detects a threshold value held for a period of time.
struct Hold {
 private:
    /// the current state of the trigger
    enum State {
        Off = 0,
        Pressed,
        Held
    } state = Off;

    /// the current time, only used when the trigger is pressed
    float time = 0.f;

 public:
    /// the number of seconds to wait for detecting a hold (\f$100ms\f$)
    static constexpr float HOLD_TIME = 0.100;

    /// @brief Reset the trigger to the default state.
    inline void reset() { state = Off; }

    /// @brief Process a step of the signal.
    ///
    /// @param signal the input signal to process
    /// @param sampleTime the amount of time between samples, i.e.,
    /// \f$T_s = \frac{1}{f_s}\f$
    ///
    inline bool process(float signal, float sampleTime) {
        switch (state) {
        case Off:  // off; detect initial press event
            if (signal >= 1.f) {  // initial press event; reset timer
                state = Pressed;
                time = 0.f;
            }
            break;
        case Pressed:  // pressing; might be holding
            if (signal <= 0.f) {  // went low before hold time, trigger
                state = Off;
                return true;
            } else {  // still high, increment timer and don't fire
                time += sampleTime;
                if (time >= HOLD_TIME) state = Held;
            }
            break;
        case Held:  // holding; might be releasing
            if (signal <= 0.f) state = Off;
            break;
        }
        return false;
    }

    /// @brief Return true if the trigger is being held, opposed to triggered.
    inline bool isHeld() const { return state == Held; }
};

}  // namespace Trigger

#endif  // DSP_TRIGGER_HOLD_HPP
