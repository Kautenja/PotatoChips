// A VCV Rack widget for viewing and editing samples in waveform.
// Copyright 2026 Christian Kauten
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

#ifndef RACK_EXTENSIONS_WAVETABLE_HPP_
#define RACK_EXTENSIONS_WAVETABLE_HPP_

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

/// Fixed storage shared by the engine and its editor. Each sample is independent;
/// a concurrent edit becomes audible at the next CV update without blocking DSP.
struct WavetableBank {
    static constexpr unsigned TABLES = 5;
    static constexpr unsigned SAMPLES = 32;
    std::atomic<uint8_t> samples[TABLES][SAMPLES];

    WavetableBank() {
        static_assert(ATOMIC_CHAR_LOCK_FREE == 2, "Wavetable samples must be lock-free");
        for (auto& table : samples)
            for (auto& sample : table) sample.store(0, std::memory_order_relaxed);
    }
};

/// Module-owned storage; allocated once, never in process().
struct WavetableOwner {
    std::shared_ptr<WavetableBank> waveBank = std::make_shared<WavetableBank>();
    virtual ~WavetableOwner() = default;
};

#endif  // RACK_EXTENSIONS_WAVETABLE_HPP_
