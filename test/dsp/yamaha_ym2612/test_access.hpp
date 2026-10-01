// Read-only YM2612 state inspection for deterministic regressions.
// Copyright (c) 2026 Christian Kauten. MIT license; see docs/licenses/MIT-TESTS.txt.
#ifndef TEST_YM2612_ACCESS_HPP_
#define TEST_YM2612_ACCESS_HPP_
#include "../../../src/dsp/yamaha_ym2612/feedback_operator.hpp"
#include "../../../src/dsp/yamaha_ym2612/voice4op.hpp"
namespace YamahaYM2612 {
struct Snapshot {
    int stage;
    int attenuation;
    uint32_t phase;
    bool gate;
    bool loop;
    bool operator==(const Snapshot& b) const {
        return stage == b.stage && attenuation == b.attenuation &&
            phase == b.phase && gate == b.gate && loop == b.loop;
    }
};
/// Tests can observe states without setters, logging or audio-thread counters.
struct TestAccess {
    static Snapshot read(const Operator& op) {
        return {int(op.env_stage), op.volume, op.phase, op.is_gate_open, op.ssg_enabled};
    }
    static const OperatorContext& context(const FeedbackOperator& op) { return op.state; }
    static const OperatorContext& context(const Voice4Op& voice) { return voice.state; }
    static const Operator& op(const Voice4Op& voice, int index) {
        return voice.operators[OPERATOR_INDEXES[index]];
    }
};
}
#endif
