// Real Rack YM2612 event delivery, routing and state regressions for spec 009.
// Copyright (c) 2026 Christian Kauten. MIT license; see docs/licenses/MIT-TESTS.txt.
#include <engine/Engine.hpp>
#undef PRIVATE
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/MiniBoss.cpp"
#include "../../src/BossFight.cpp"
#include "../dsp/yamaha_ym2612/test_access.hpp"
#include <iostream>
#include <memory>
#include <vector>
Plugin* plugin_instance = nullptr;
struct YM2612ModuleTestAccess {
    static const YamahaYM2612::FeedbackOperator& voice(const MiniBoss& m, int c) { return m.apu[c]; }
    static const YamahaYM2612::Voice4Op& voice(const BossFight& m, int c) { return m.apu[c]; }
};
namespace {
using YamahaYM2612::TestAccess;
using YamahaYM2612::Snapshot;
struct Host {
    rack::Context context;
    Host(float rate = 48000) {
        rack::contextSet(&context);
        context.engine = new rack::engine::Engine;
        context.engine->setSampleRate(rate);
    }
    ~Host() { rack::contextSet(nullptr); }
};
void process(Module& m, float rate) {
    Module::ProcessArgs args = {}; args.sampleRate = rate; args.sampleTime = 1.f / rate;
    m.process(args);
}
void configure(MiniBoss& m, int channels, bool soft, bool loop) {
    m.prevent_clicks = soft;
    m.params[MiniBoss::PARAM_AR].setValue(15); m.params[MiniBoss::PARAM_D1].setValue(31);
    m.params[MiniBoss::PARAM_SL].setValue(5); m.params[MiniBoss::PARAM_D2].setValue(7);
    m.params[MiniBoss::PARAM_RR].setValue(0); m.params[MiniBoss::PARAM_FREQ].setValue(-1.18500173f);
    m.params[MiniBoss::PARAM_FB].setValue(3); m.params[MiniBoss::PARAM_SSG_ENABLE].setValue(loop);
    m.inputs[MiniBoss::INPUT_GATE].channels = channels;
    m.inputs[MiniBoss::INPUT_RETRIG].channels = channels;
    m.inputs[MiniBoss::INPUT_VOCT].channels = channels;
    m.inputs[MiniBoss::INPUT_FM].channels = channels; // The issue's zero-depth feedback cable.
    m.outputs[0].channels = channels;
}
void configure(BossFight& m, int channels, bool soft, bool loop) {
    m.prevent_clicks = soft; m.params[BossFight::PARAM_AL].setValue(7);
    m.params[BossFight::PARAM_FB].setValue(3);
    for (int op = 0; op < 4; ++op) {
        m.params[BossFight::PARAM_AR + op].setValue(15); m.params[BossFight::PARAM_D1 + op].setValue(31);
        m.params[BossFight::PARAM_SL + op].setValue(5); m.params[BossFight::PARAM_D2 + op].setValue(7);
        m.params[BossFight::PARAM_RR + op].setValue(0); m.params[BossFight::PARAM_FREQ + op].setValue(-1.18500173f);
        m.params[BossFight::PARAM_SSG_ENABLE + op].setValue(loop);
    }
    m.inputs[BossFight::INPUT_GATE].channels = channels;
    m.inputs[BossFight::INPUT_RETRIG].channels = channels;
    m.inputs[BossFight::INPUT_PITCH].channels = channels;
    m.outputs[0].channels = channels;
}
int gatePort(const MiniBoss&) { return MiniBoss::INPUT_GATE; }
int gatePort(const BossFight&) { return BossFight::INPUT_GATE; }
int retrigPort(const MiniBoss&) { return MiniBoss::INPUT_RETRIG; }
int retrigPort(const BossFight&) { return BossFight::INPUT_RETRIG; }
int count(const YamahaYM2612::FeedbackOperator&) { return 1; }
int count(const YamahaYM2612::Voice4Op&) { return 4; }
Snapshot read(const YamahaYM2612::FeedbackOperator& v, int) { return TestAccess::read(v); }
Snapshot read(const YamahaYM2612::Voice4Op& v, int op) { return TestAccess::read(TestAccess::op(v, op)); }
void gate(YamahaYM2612::FeedbackOperator& v, int, bool high, bool soft) { v.set_gate(high, soft); }
void gate(YamahaYM2612::Voice4Op& v, int op, bool high, bool soft) { v.set_gate(op, high, soft); }
void frequency(YamahaYM2612::FeedbackOperator& v, MiniBoss& m) { v.set_frequency(m.getFrequency(15)); }
void frequency(YamahaYM2612::Voice4Op& v, BossFight& m) {
    for (int op = 0; op < 4; ++op) v.set_frequency(op, Math::Eurorack::voct2freq(m.params[BossFight::PARAM_FREQ + op].getValue()));
}
void rebind(YamahaYM2612::FeedbackOperator&) {}
void rebind(YamahaYM2612::Voice4Op& v) { v.set_algorithm(7); }
/// Clone a warmed-up voice for an oracle receiving explicit chip key writes.
/// Rebind Voice4Op's internal routing pointers after copying. Source remains alive.
template<class Voice> std::unique_ptr<Voice> reference(const Voice& source) {
    std::unique_ptr<Voice> v(new Voice(source)); rebind(*v); return v;
}
template<class Voice> void compare(const Voice& a, const Voice& b) {
    for (int op = 0; op < count(a); ++op) {
        CATCH_INFO("operator " << op << ", actual stage " << read(a, op).stage << ", expected " << read(b, op).stage);
        CATCH_REQUIRE(read(a, op) == read(b, op));
    }
    CATCH_REQUIRE(TestAccess::context(a).eg_cnt == TestAccess::context(b).eg_cnt);
    CATCH_REQUIRE(TestAccess::context(a).eg_timer == TestAccess::context(b).eg_timer);
    CATCH_REQUIRE(TestAccess::context(a).lfo_timer == TestAccess::context(b).lfo_timer);
}
template<class M> void timing() {
    for (float rate : {44100.f, 48000.f, 96000.f}) for (bool soft : {false, true})
    for (bool loop : {false, true}) for (int channels : {1, 4, 16})
    for (int offset = 0; offset < 16; ++offset) for (int width : {1, int(rate / 1000), 128}) {
        CATCH_INFO(rate << " Hz, soft " << soft << ", loop " << loop << ", channels " << channels
            << ", offset " << offset << ", width " << width);
        Host host(rate); M m; configure(m, channels, soft, loop);
        for (int c = 0; c < channels; ++c) m.inputs[gatePort(m)].setVoltage(5, c);
        for (int n = 0; n < 256; ++n) process(m, rate);
        auto expected = reference(YM2612ModuleTestAccess::voice(m, channels - 1));
        auto untouched = reference(YM2612ModuleTestAccess::voice(m, 0));
        for (int n = 0; n < 176; ++n) {
            m.inputs[retrigPort(m)].setVoltage(n >= offset && n < offset + width ? 5 : 0, channels - 1);
            if (n == offset) for (int op = 0; op < count(*expected); ++op) {
                gate(*expected, op, false, soft); gate(*expected, op, true, soft);
            }
            expected->step(); untouched->step(); process(m, rate);
            compare(YM2612ModuleTestAccess::voice(m, channels - 1), *expected);
            if (channels > 1) compare(YM2612ModuleTestAccess::voice(m, 0), *untouched);
            for (int c = 0; c < channels; ++c) CATCH_REQUIRE(std::abs(m.outputs[0].getVoltage(c)) <= 5);
        }
    }
}
template<class M> void ordering() {
    for (bool soft : {false, true}) {
        Host host; M m; configure(m, 4, soft, true); process(m, 48000);
        auto expected = reference(YM2612ModuleTestAccess::voice(m, 0));
        // Explicit key events: simultaneous rise, held high, key-off plus RTRG,
        // RTRG alone, sampled low to rearm, threshold hysteresis, then release.
        const float gates[] = {5,5,5,5,0,0,0,0,0,0,1.99f,2,.02f,.01f,0};
        const float retrigs[] = {5,5,0,5,0,5,5,0,5,0,0,0,0,5,0};
        const bool key_on[] = {1,0,0,1,0,1,0,0,1,0,0,1,0,1,0};
        const bool key_off[] = {0,0,0,0,1,0,1,0,0,1,0,0,0,0,1};
        for (int n = 0; n < 15; ++n) {
            m.inputs[gatePort(m)].setVoltage(gates[n]); m.inputs[retrigPort(m)].setVoltage(retrigs[n]);
            for (int op = 0; op < count(*expected); ++op) {
                if (key_off[n] || key_on[n]) gate(*expected, op, false, soft);
                if (key_on[n]) gate(*expected, op, true, soft);
            }
            expected->step(); process(m, 48000); compare(YM2612ModuleTestAccess::voice(m, 0), *expected);
        }
    }
}
}
CATCH_TEST_CASE("Operator 2612 receives each edge at every CV divider offset") { timing<MiniBoss>(); }
CATCH_TEST_CASE("Voice 2612 receives each edge at every CV divider offset") { timing<BossFight>(); }
CATCH_TEST_CASE("YM2612 coincident and gate-low events have deterministic priority") {
    ordering<MiniBoss>(); ordering<BossFight>();
}
CATCH_TEST_CASE("YM2612 issue 82 minimized state trace", "[.trace]") {
    Host host; MiniBoss m; configure(m, 4, true, true);
    std::cout << "frame,voice,gate_v,retrig_v,key_open,loop,stage,attenuation,eg_count,eg_timer\n";
    for (int n = 0; n < 130; ++n) {
        m.inputs[MiniBoss::INPUT_GATE].setVoltage(5);
        m.inputs[MiniBoss::INPUT_RETRIG].setVoltage(n == 65 || (n >= 96 && n < 120) ? 5 : 0);
        process(m, 48000);
        const auto& v = YM2612ModuleTestAccess::voice(m, 0); const auto s = read(v, 0);
        if (n >= 60) std::cout << n << ",0,5," << m.inputs[MiniBoss::INPUT_RETRIG].getVoltage() << ','
            << s.gate << ',' << s.loop << ',' << s.stage << ',' << s.attenuation << ','
            << TestAccess::context(v).eg_cnt << ',' << TestAccess::context(v).eg_timer << '\n';
    }
}
namespace {
template<class M> void stress() {
    for (bool soft : {false, true}) {
        Host host; M m; configure(m, 4, soft, true); process(m, 48000);
        using Voice = typename std::remove_const<typename std::remove_reference<decltype(YM2612ModuleTestAccess::voice(m, 0))>::type>::type;
        std::vector<std::unique_ptr<Voice>> expected;
        for (int c = 0; c < 4; ++c) expected.push_back(reference(YM2612ModuleTestAccess::voice(m, c)));
        bool held[4] = {}; bool equal = true; int accepted = 0;
        int loops[4][4] = {};
        const int end_events = 10000 * 521;
        for (int frame = 0; frame < end_events + 100000; ++frame) {
            const bool event = frame < end_events && frame % 521 == 0;
            const int index = frame / 521;
            const int target = index % 4;
            if (event) { held[target] = index % 7 != 6; ++accepted; }
            if (frame == end_events) for (auto& high : held) high = true;
            Snapshot before[4][4];
            for (int c = 0; c < 4; ++c) {
                const bool pulse = event && c == target;
                m.inputs[gatePort(m)].setVoltage(held[c] ? 5 : 0, c);
                m.inputs[retrigPort(m)].setVoltage(pulse ? 5 : 0, c);
                for (int op = 0; op < count(*expected[c]); ++op) {
                    before[c][op] = read(YM2612ModuleTestAccess::voice(m, c), op);
                    if (pulse) gate(*expected[c], op, false, soft);
                    gate(*expected[c], op, held[c] || pulse, soft);
                }
                expected[c]->step();
            }
            process(m, 48000);
            for (int c = 0; c < 4; ++c) for (int op = 0; op < count(*expected[c]); ++op) {
                const auto actual = read(YM2612ModuleTestAccess::voice(m, c), op);
                if (!(actual == read(*expected[c], op))) equal = false;
                if (frame > end_events && before[c][op].stage != 4 && actual.stage == 4) ++loops[c][op];
                if (frame > end_events && actual.stage <= 1) equal = false;
            }
        }
        CATCH_INFO("soft " << soft << ", 10000 scheduled note/retrigger events, four rotating voices");
        CATCH_CHECK(accepted == 10000); CATCH_CHECK(equal);
        for (int c = 0; c < 4; ++c) for (int op = 0; op < count(*expected[c]); ++op) CATCH_CHECK(loops[c][op] >= 2);
    }
}
template<class M> void lifecycle() {
    Host host; M m; configure(m, 16, true, true);
    for (int c = 0; c < 16; ++c) m.inputs[gatePort(m)].setVoltage(5, c);
    for (int n = 0; n < 700; ++n) process(m, 48000);
    // Reset keeps the running DSP and detector state, and restores phase policy.
    const auto before = read(YM2612ModuleTestAccess::voice(m, 15), 0);
    m.onReset();
    CATCH_CHECK_FALSE(m.prevent_clicks);
    CATCH_CHECK(read(YM2612ModuleTestAccess::voice(m, 15), 0) == before);
    // Retire all but one lane, including a held-high retrigger detector.
    m.inputs[retrigPort(m)].setVoltage(5, 15); process(m, 48000);
    for (auto& input : m.inputs) input.channels = input.channels ? 1 : 0;
    process(m, 48000);
    CATCH_CHECK_FALSE(read(YM2612ModuleTestAccess::voice(m, 15), 0).gate);
    configure(m, 16, true, true);
    m.inputs[gatePort(m)].setVoltage(0, 15); m.inputs[retrigPort(m)].setVoltage(0, 15);
    process(m, 48000);
    CATCH_CHECK_FALSE(read(YM2612ModuleTestAccess::voice(m, 15), 0).gate);
    m.inputs[gatePort(m)].setVoltage(5, 15); m.inputs[retrigPort(m)].setVoltage(5, 15);
    process(m, 48000);
    CATCH_CHECK(read(YM2612ModuleTestAccess::voice(m, 15), 0).gate);
    auto expected = reference(YM2612ModuleTestAccess::voice(m, 15));
    host.context.engine->setSampleRate(96000); m.onSampleRateChange(); expected->set_sample_rate(96000, CLOCK_RATE);
    frequency(*expected, m);
    for (int n = 0; n < 1000; ++n) { process(m, 96000); expected->step(); compare(YM2612ModuleTestAccess::voice(m, 15), *expected); }
    // Disconnect ignores residual voltage and must not leave a held key.
    for (auto& input : m.inputs) input.channels = 0;
    process(m, 96000);
    for (int c = 0; c < 16; ++c) CATCH_CHECK_FALSE(read(YM2612ModuleTestAccess::voice(m, c), 0).gate);
    for (bool soft : {false, true}) {
        m.prevent_clicks = soft;
        json_t* data = m.dataToJson(); M restored; restored.dataFromJson(data); json_decref(data);
        CATCH_CHECK(restored.prevent_clicks == soft);
        process(restored, 96000);
        CATCH_CHECK(read(YM2612ModuleTestAccess::voice(restored, 0), 0).stage == 0);
    }
}
}
CATCH_TEST_CASE("YM2612 four-voice stress matches explicit chip events and continues looping") {
    stress<MiniBoss>(); stress<BossFight>();
}
CATCH_TEST_CASE("YM2612 reset, rate changes, voice retirement and reload preserve key state") {
    lifecycle<MiniBoss>(); lifecycle<BossFight>();
}
CATCH_TEST_CASE("Voice 2612 retrigger overrides preserve operator order and normalling") {
    for (int target = 0; target < 4; ++target) for (bool soft : {false, true}) {
        Host host; BossFight m; configure(m, 16, soft, true);
        // Explicit overrides stop the retrigger normalling chain after its target.
        for (int op = 0; op < 4; ++op) m.inputs[BossFight::INPUT_RETRIG + op].channels = 16;
        for (int c = 0; c < 16; ++c) m.inputs[BossFight::INPUT_GATE].setVoltage(5, c);
        for (int n = 0; n < 1000; ++n) process(m, 48000);
        auto expected = reference(YM2612ModuleTestAccess::voice(m, 9));
        m.inputs[BossFight::INPUT_RETRIG + target].setVoltage(5, 9);
        expected->set_gate(target, false, soft); expected->set_gate(target, true, soft);
        for (int n = 0; n < 100; ++n) { expected->step(); process(m, 48000); compare(YM2612ModuleTestAccess::voice(m, 9), *expected); }
        m.inputs[BossFight::INPUT_GATE + target].channels = 16;
        for (int c = 0; c < 16; ++c) m.inputs[BossFight::INPUT_GATE + target].setVoltage(0, c);
        process(m, 48000);
        for (int op = 0; op < 4; ++op) CATCH_CHECK(read(YM2612ModuleTestAccess::voice(m, 9), op).gate == (op < target));
    }
}
