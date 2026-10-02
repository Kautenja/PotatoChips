// An independent OPM channel with bounded register scheduling and resampling.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DSP_YAMAHA_YM2151_VOICE_HPP_
#define DSP_YAMAHA_YM2151_VOICE_HPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../../../dep/ymfm/ymfm_opm.h"

namespace YamahaYM2151 {

/// @brief One hardware channel 8 per modular voice; no shared chip state.
/// @details Allocates only in ymfm construction. reset/process/write never
/// allocate. One bus transaction per 64 master clocks respects ymfm busy.
/// Controls coalesce by register; accepted key events remain ordered.
class Voice : private ymfm::ymfm_interface {
 public:
    static constexpr double MASTER_CLOCK = 3579545.0;
    static constexpr double NATIVE_RATE = MASTER_CLOCK / 64.0;
    static constexpr unsigned EVENT_CAPACITY = 64;
    static constexpr unsigned FILTER_TAPS = 32;

 private:
    ymfm::ym2151 chip;
    std::vector<uint8_t> reset_state;
    bool pitch_pending = false;
    uint8_t pitch_fraction = 0;
    std::array<uint8_t, 256> desired{};
    std::array<bool, 256> dirty{};
    std::array<uint8_t, 256> controls{};
    std::array<uint8_t, EVENT_CAPACITY> events{};
    unsigned control_read = 0, control_count = 0;
    unsigned event_read = 0, event_count = 0;
    unsigned startup_writes = 0;
    uint64_t clocks = 0, busy_end = 0;
    double phase = 0, host_rate = 48000;
    std::array<double, FILTER_TAPS> coefficients{};
    float history[2][FILTER_TAPS]{};
    float previous[2]{}, current[2]{};
    unsigned history_pos = 0;
    uint64_t accepted_keys = 0, rejected_events = 0;

    void ymfm_set_busy_end(uint32_t duration) override { busy_end = clocks + duration; }
    bool ymfm_is_busy() override { return clocks < busy_end; }

    /// @brief Transfer one address/data pair; virtual 0x1a selects PM depth.
    void transfer(uint8_t address, uint8_t value) {
        chip.write_address(address == 0x1a ? 0x19 : address);
        chip.write_data(address == 0x1a ? value | 0x80 : value);
    }

    void native_step() {
        clocks += 64;
        if (!ymfm_is_busy()) {
            if (pitch_pending) {
                transfer(0x37, pitch_fraction);
                pitch_pending = false;
            } else if (event_count && startup_writes == 0) {
                transfer(8, events[event_read]);
                event_read = (event_read + 1) % EVENT_CAPACITY;
                --event_count;
                ++accepted_keys;
            } else if (control_count) {
                uint8_t address = controls[control_read];
                control_read = (control_read + 1) % controls.size();
                --control_count;
                dirty[address] = false;
                transfer(address, desired[address]);
                if (address == 0x2f) {
                    pitch_fraction = desired[0x37];
                    pitch_pending = true;
                }
                if (startup_writes) --startup_writes;
            }
        }
        ymfm::ym2151::output_data output;
        chip.generate(&output);
        for (unsigned side = 0; side < 2; ++side) {
            history[side][history_pos] = output.data[side] / 32768.f;
            double sum = 0;
            for (unsigned tap = 0; tap < FILTER_TAPS; ++tap)
                sum += coefficients[tap] * history[side][(history_pos + FILTER_TAPS - tap) % FILTER_TAPS];
            previous[side] = current[side];
            current[side] = static_cast<float>(sum);
        }
        history_pos = (history_pos + 1) % FILTER_TAPS;
    }

 public:
    Voice() : chip(*this) {
        chip.reset();
        ymfm::ymfm_saved_state snapshot(reset_state, true);
        chip.save_restore(snapshot);
        set_sample_rate(48000);
        reset();
    }
    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;

    /// @brief Reset in bounded time without constructing/allocating a core.
    void reset() {
        ymfm::ymfm_saved_state snapshot(reset_state, false);
        chip.save_restore(snapshot);
        chip.invalidate_caches();
        pitch_pending = false;
        desired.fill(0); dirty.fill(false);
        control_read = control_count = event_read = event_count = 0;
        startup_writes = 0;
        clocks = busy_end = accepted_keys = rejected_events = 0;
        phase = 0; history_pos = 0;
        for (unsigned side = 0; side < 2; ++side) {
            std::fill(history[side], history[side] + FILTER_TAPS, 0.f);
            previous[side] = current[side] = 0;
        }
    }

    /// @brief Causal Blackman FIR at native rate followed by linear conversion.
    /// @details Cutoff is min(20 kHz, 0.40 host rate). Rate changes preserve
    /// chip/envelope phase and filter history. No allocation or core reset.
    void set_sample_rate(double rate) {
        host_rate = std::isfinite(rate) ? std::max(8000.0, std::min(384000.0, rate)) : 48000;
        const double cutoff = std::min(20000.0, host_rate * 0.4) / NATIVE_RATE;
        const double pi = 3.14159265358979323846;
        double sum = 0;
        for (unsigned i = 0; i < FILTER_TAPS; ++i) {
            double x = i - (FILTER_TAPS - 1) * 0.5;
            double window = 0.42 - 0.5 * std::cos(2 * pi * i / (FILTER_TAPS - 1))
                + 0.08 * std::cos(4 * pi * i / (FILTER_TAPS - 1));
            coefficients[i] = std::sin(2 * pi * cutoff * x) / (pi * x) * window;
            sum += coefficients[i];
        }
        for (auto& value : coefficients) value /= sum;
    }

    /// @brief Coalesce a replaceable register, never a key event.
    void write(uint8_t address, uint8_t value) {
        if (desired[address] == value) return;
        desired[address] = value;
        if (!dirty[address]) {
            controls[(control_read + control_count) % controls.size()] = address;
            ++control_count;
            dirty[address] = true;
        }
    }

    /// @brief Finish initial configuration before accepting the first key-on.
    void prepare() { startup_writes = control_count; }

    /// @brief Enqueue gate or retrigger; reserve one slot for a later release.
    /// @returns false for an overloaded rise/retrigger, with no partial event.
    /// @details Accepted events are never coalesced. A rise/retrigger requires
    /// spare capacity for key-off. At <=1 kHz retriggers the bus has ample room.
    bool key(bool high, bool retrigger = false) {
        const unsigned needed = high && retrigger ? 2 : 1;
        if (event_count + needed > EVENT_CAPACITY - (high ? 1 : 0)) {
            ++rejected_events;
            return false;
        }
        if (high && retrigger) {
            events[(event_read + event_count++) % EVENT_CAPACITY] = 7;
        }
        events[(event_read + event_count++) % EVENT_CAPACITY] = high ? 0x7f : 7;
        return true;
    }

    /// @brief Nearest 1/64 semitone; KC zero is C#0 at the chosen clock.
    /// @details 0 V is C4. Clamp C#0 through C#8 minus 1/64 semitone.
    void pitch(float volts) {
        static const uint8_t codes[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};
        double semitones = std::isfinite(volts) ? 47.0 + 12.0 * volts : 47.0;
        int steps = static_cast<int>(std::round(std::max(0.0, std::min(95.984375, semitones)) * 64));
        int note = steps / 64;
        // Queue one transaction, then snapshot both values at its first write.
        const uint8_t kc = static_cast<uint8_t>((note / 12) * 16 + codes[note % 12]);
        const uint8_t kf = static_cast<uint8_t>((steps % 64) << 2);
        if (desired[0x2f] == kc && desired[0x37] == kf) return;
        desired[0x37] = kf;
        if (!dirty[0x2f]) {
            controls[(control_read + control_count++) % controls.size()] = 0x2f;
            dirty[0x2f] = true;
        }
        desired[0x2f] = kc;
    }

    /// @brief Render normalized decoded stereo DAC output, without normalling.
    void process(float* output) {
        phase += NATIVE_RATE / host_rate;
        while (phase >= 1) { native_step(); phase -= 1; }
        for (unsigned side = 0; side < 2; ++side)
            output[side] = previous[side] + static_cast<float>(phase) * (current[side] - previous[side]);
    }

    /// @brief Owned heap payload; allocator bookkeeping is host-dependent.
    size_t heap_bytes() const {
        return reset_state.capacity() + 8 * sizeof(ymfm::fm_channel<ymfm::opm_registers>)
            + 32 * sizeof(ymfm::fm_operator<ymfm::opm_registers>);
    }
    unsigned queued_keys() const { return event_count; }
    unsigned queued_controls() const { return control_count; }
    uint64_t key_writes() const { return accepted_keys; }
    uint64_t overloads() const { return rejected_events; }
    uint8_t requested_register(unsigned address) const { return desired[address]; }
};

}  // namespace YamahaYM2151
#endif  // DSP_YAMAHA_YM2151_VOICE_HPP_
