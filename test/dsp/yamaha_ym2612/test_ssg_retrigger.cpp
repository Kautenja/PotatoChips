// YM2612 key-on and SSG quirk regressions.
// Copyright (c) 2026 Christian Kauten. MIT license; see docs/licenses/MIT-TESTS.txt.
#include "catch_amalgamated.hpp"
#include <cmath>
// The standalone DSP headers must compile without platform math extensions.
#ifdef M_PI
#undef M_PI
#endif
#include "test_access.hpp"
#include <set>
using namespace YamahaYM2612;
namespace {
void configure(FeedbackOperator& op, bool loop = true) {
    op.set_attack_rate(15); op.set_total_level(0); op.set_decay_rate(31);
    op.set_sustain_level(10); op.set_sustain_rate(7); op.set_release_rate(0);
    op.set_frequency(115.08f); op.set_multiplier(1); op.set_feedback(3);
    op.set_ssg_enabled(loop);
}
}
TEST_CASE("SSG key-on retains attenuation and shared clock, with independent phase policy") {
    for (bool soft : {false, true}) for (float rate : {44100.f, 48000.f, 96000.f}) {
        FeedbackOperator op(rate); configure(op); op.set_gate(true, soft);
        std::set<int> stages;
        int loops = 0;
        for (int n = 0; n < 100000; ++n) {
            auto before = TestAccess::read(op);
            op.step();
            auto after = TestAccess::read(op);
            stages.insert(after.stage);
            if (before.stage != 4 && after.stage == 4) ++loops;
            REQUIRE(after.stage > 1);
            if (n % 9973 == 0) {
                const auto eg = TestAccess::context(op).eg_cnt;
                const auto timer = TestAccess::context(op).eg_timer;
                const auto lfo = TestAccess::context(op).lfo_timer;
                op.set_gate(false, soft); op.set_gate(true, soft);
                const auto keyed = TestAccess::read(op);
                CHECK(keyed.attenuation == after.attenuation);
                CHECK(keyed.stage == 4);
                CHECK(keyed.phase == (soft ? after.phase : 0));
                CHECK(TestAccess::context(op).eg_cnt == eg);
                CHECK(TestAccess::context(op).eg_timer == timer);
                CHECK(TestAccess::context(op).lfo_timer == lfo);
            }
        }
        CHECK(stages.count(4)); CHECK(stages.count(3)); CHECK(stages.count(2));
        // More than one natural repetition excludes an accepted one-shot.
        CHECK(loops >= 2);
        op.set_release_rate(15);
        op.set_gate(false, soft);
        for (int n = 0; n < 200000; ++n) op.step();
        CHECK(TestAccess::read(op).stage == 0);
    }
}
TEST_CASE("SSG retains zero-rate holds and repeated phase resets above its boundary") {
    OperatorContext context; context.set_sample_rate(48000, 768000); context.reset();
    Operator op; op.reset(context); op.set_frequency(context, 440);
    op.set_attack_rate(1); op.set_ssg_enabled(true); op.refresh_phase_and_envelope();
    op.set_gate(true, true);
    for (int n = 0; n < 20; ++n) {
        op.update_phase_counters(context);
        REQUIRE(TestAccess::read(op).phase != 0);
        op.update_ssg_envelope_generator();
        CHECK(TestAccess::read(op).phase == 0);
        CHECK(TestAccess::read(op).attenuation == MAX_ATT_INDEX);
    }
    FeedbackOperator hold; configure(hold); hold.set_attack_rate(31);
    hold.set_decay_rate(0); hold.set_gate(true);
    for (int n = 0; n < 10000; ++n) hold.step();
    CHECK(TestAccess::read(hold).attenuation == 0);
    CHECK(TestAccess::read(hold).stage == 3);
    hold.reset(); CHECK_FALSE(hold.is_gate_open);
    CHECK(TestAccess::read(hold).stage == 0);
}
TEST_CASE("Voice4Op key events leave other operators and shared clocks untouched") {
    for (bool soft : {false, true}) {
        Voice4Op voice;
        for (int op = 0; op < 4; ++op) {
            voice.set_attack_rate(op, 15); voice.set_decay_rate(op, 31);
            voice.set_sustain_level(op, 10); voice.set_sustain_rate(op, 7);
            voice.set_ssg_enabled(op, true); voice.set_frequency(op, 220 + 30 * op);
            voice.set_gate(op, true, soft);
        }
        for (int n = 0; n < 300; ++n) voice.step();
        for (int target = 0; target < 4; ++target) {
            Snapshot before[4];
            for (int op = 0; op < 4; ++op) before[op] = TestAccess::read(TestAccess::op(voice, op));
            const auto context = TestAccess::context(voice);
            voice.set_gate(target, false, soft); voice.set_gate(target, true, soft);
            for (int op = 0; op < 4; ++op) {
                const auto after = TestAccess::read(TestAccess::op(voice, op));
                if (op != target) CHECK(after == before[op]);
                else {
                    CHECK(after.attenuation == before[op].attenuation);
                    CHECK(after.stage == 4);
                    CHECK(after.phase == (soft ? before[op].phase : 0));
                }
            }
            CHECK(TestAccess::context(voice).eg_cnt == context.eg_cnt);
            CHECK(TestAccess::context(voice).eg_timer == context.eg_timer);
            CHECK(TestAccess::context(voice).lfo_timer == context.lfo_timer);
        }
    }
}
TEST_CASE("YM2612 loop toggles and rate boundaries retain chip state", "[boundaries]") {
    for (int ar : {1, 15, 31}) for (int decay : {0, 1, 31}) for (int sl : {0, 10, 15})
    for (int scale : {0, 3}) {
        FeedbackOperator op; configure(op); op.set_attack_rate(ar); op.set_decay_rate(decay);
        op.set_sustain_level(sl); op.set_sustain_rate(decay); op.set_rate_scale(scale);
        op.set_gate(true);
        for (int n = 0; n < 4000; ++n) {
            if (n == 1000 || n == 2000) {
                const auto old = TestAccess::read(op);
                op.set_ssg_enabled(n == 2000);
                const auto now = TestAccess::read(op);
                CHECK(now.stage == old.stage); CHECK(now.attenuation == old.attenuation);
                CHECK(now.phase == old.phase); CHECK(now.gate == old.gate);
            }
            op.step();
            REQUIRE(TestAccess::read(op).stage > 1);
            REQUIRE(TestAccess::read(op).attenuation >= 0);
            REQUIRE(TestAccess::read(op).attenuation <= MAX_ATT_INDEX);
        }
    }
}
