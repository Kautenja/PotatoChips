// Native Rack widget/Scope replay using Core's MIDI parser, without a device.
// Copyright (c) 2026 Christian Kauten. MIT license; see docs/licenses/MIT-TESTS.txt.
#include <engine/Engine.hpp>
#undef PRIVATE
#include "src/MiniBoss.cpp"
#include "src/BossFight.cpp"
#include "test/dsp/yamaha_ym2612/test_access.hpp"
#include "tools/capture/render.hpp"
#include <dlfcn.h>
#include <iostream>
#include <memory>
Plugin* plugin_instance = nullptr;
struct YM2612ModuleTestAccess {
    static const YamahaYM2612::FeedbackOperator& voice(const MiniBoss& m, int c) { return m.apu[c]; }
    static const YamahaYM2612::Voice4Op& voice(const BossFight& m, int c) { return m.apu[c]; }
};
using YamahaYM2612::TestAccess;
int main(int argc, char** argv) {
    if (argc != 5) return 1; // Rack root, plugin root, Fundamental root, output.
    Context context; contextSet(&context); context.engine = new engine::Engine;
    context.engine->setSampleRate(48000); context.event = new widget::EventState;
    context.history = new history::State;
    asset::systemDir = argv[1]; asset::userDir = std::string(argv[4]) + "/user";
    Plugin plugin; plugin.slug = "KautenjaDSP-PotatoChips"; plugin.path = argv[2]; plugin_instance = &plugin;
    plugin.addModel(modelMiniBoss); plugin.addModel(modelBossFight);
    Plugin fundamental; fundamental.path = argv[3]; fundamental.slug = "Fundamental";
#ifdef __APPLE__
    const auto library = fundamental.path + "/plugin.dylib";
#else
    const auto library = fundamental.path + "/plugin.so";
#endif
    auto handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) { std::cerr << dlerror() << '\n'; return 2; }
    auto init = reinterpret_cast<void (*)(Plugin*)>(dlsym(handle, "init"));
    if (!init || !glfwInit()) return 2;
    init(&fundamental);
    context.window = new window::Window; glfwHideWindow(context.window->win);
    context.scene = new app::Scene;
    int64_t nextId = 1;
    for (Model* model : {modelMiniBoss, modelBossFight}) for (bool soft : {false, true}) {
        auto module = model->createModule(); module->id = nextId++;
        context.engine->addModule(module);
        auto mini = dynamic_cast<MiniBoss*>(module); auto boss = dynamic_cast<BossFight*>(module);
        const int ops = mini ? 1 : 4;
        if (mini) {
            mini->prevent_clicks = soft;
            mini->params[MiniBoss::PARAM_FB].setValue(3);
        } else {
            boss->prevent_clicks = soft;
            boss->params[BossFight::PARAM_FB].setValue(3);
            boss->params[BossFight::PARAM_AL].setValue(7);
        }
        for (int op = 0; op < ops; ++op) {
            module->params[(mini ? MiniBoss::PARAM_AR : BossFight::PARAM_AR) + op].setValue(15);
            module->params[(mini ? MiniBoss::PARAM_D1 : BossFight::PARAM_D1) + op].setValue(31);
            module->params[(mini ? MiniBoss::PARAM_SL : BossFight::PARAM_SL) + op].setValue(5);
            module->params[(mini ? MiniBoss::PARAM_D2 : BossFight::PARAM_D2) + op].setValue(7);
            module->params[(mini ? MiniBoss::PARAM_RR : BossFight::PARAM_RR) + op].setValue(0);
            module->params[(mini ? MiniBoss::PARAM_FREQ : BossFight::PARAM_FREQ) + op].setValue(-1.18500173f);
            module->params[(mini ? MiniBoss::PARAM_SSG_ENABLE : BossFight::PARAM_SSG_ENABLE) + op].setValue(1);
        }
        const int gp = mini ? MiniBoss::INPUT_GATE : BossFight::INPUT_GATE;
        const int rp = mini ? MiniBoss::INPUT_RETRIG : BossFight::INPUT_RETRIG;
        const int pp = mini ? MiniBoss::INPUT_VOCT : BossFight::INPUT_PITCH;
        for (int port : {gp, rp, pp}) module->inputs[port].channels = 4;
        if (mini) module->inputs[MiniBoss::INPUT_FM].channels = 4;
        module->outputs[0].channels = 4;
        auto scopeModel = fundamental.getModel("Scope"); auto scope = scopeModel->createModule();
        scope->id = nextId++; context.engine->addModule(scope);
        scope->params[4].setValue(-std::log2(.25f)); scope->params[7].setValue(1);
        scope->inputs[0].channels = scope->inputs[1].channels = 4;
        scope->params[1].setValue(5); scope->params[3].setValue(-5);
        rack::dsp::MidiParser<16> midi; midi.channels = 4;
        std::string base = std::string(argv[4]) + "/" + model->slug + (soft ? "-soft" : "-hard");
        std::ofstream trace(base + ".csv");
        trace << "frame,voice,gate_v,retrig_v,pitch_v,stage,attenuation,eg_count,eg_timer\n";
        float lastGate[4] = {}, lastRetrig[4] = {}, lastPitch[4] = {};
        int previousStage[4][4] = {}, loops[4][4] = {};
        Module::ProcessArgs args = {}; args.sampleRate = 48000; args.sampleTime = 1.f / 48000;
        auto message = [&](int status, int note) {
            midi::Message msg; msg.setStatus(status); msg.setNote(note); msg.setValue(status == 9 ? 100 : 0);
            midi.processMessage(msg);
        };
        for (int frame = 0; frame < 48000; ++frame) {
            args.frame = frame;
            if (frame <= 576 && frame % 192 == 0) message(9, 60 + frame / 192 * 4);
            if (frame == 2000) for (int note : {76, 79, 83, 88}) message(9, note);
            if (frame == 4000) for (int note : {76, 79, 83, 88}) message(8, note);
            if (frame == 5000) message(9, 60);
            if (frame == 6000) message(8, 60);
            if (frame == 7000) for (int note : {60, 64, 67, 72}) message(9, note);
            midi.processFilters(args.sampleTime);
            for (int c = 0; c < 4; ++c) {
                module->inputs[gp].setVoltage(midi.gates[c] ? 10 : 0, c);
                module->inputs[rp].setVoltage(midi.retriggerPulses[c].isHigh() ? 10 : 0, c);
                module->inputs[pp].setVoltage(midi.getPitchVoltage(c), c);
                if (mini) module->inputs[MiniBoss::INPUT_FM].setVoltage(module->outputs[0].getVoltage(c), c);
            }
            module->process(args);
            for (int c = 0; c < 4; ++c) {
                const auto& chip = mini ? static_cast<const YamahaYM2612::Operator&>(YM2612ModuleTestAccess::voice(*mini, c)) : TestAccess::op(YM2612ModuleTestAccess::voice(*boss, c), 0);
                const auto state = TestAccess::read(chip);
                const auto& clock = mini ? TestAccess::context(YM2612ModuleTestAccess::voice(*mini, c)) : TestAccess::context(YM2612ModuleTestAccess::voice(*boss, c));
                const float g = module->inputs[gp].getVoltage(c), r = module->inputs[rp].getVoltage(c), p = module->inputs[pp].getVoltage(c);
                if (frame == 0 || g != lastGate[c] || r != lastRetrig[c] || p != lastPitch[c])
                    trace << frame << ',' << c << ',' << g << ',' << r << ',' << p << ',' << state.stage << ',' << state.attenuation << ',' << clock.eg_cnt << ',' << clock.eg_timer << '\n';
                lastGate[c] = g; lastRetrig[c] = r; lastPitch[c] = p;
                for (int op = 0; op < ops; ++op) {
                    const auto s = mini ? state : TestAccess::read(TestAccess::op(YM2612ModuleTestAccess::voice(*boss, c), op));
                    if (frame > 7000 && s.stage <= 1) return 3;
                    if (frame > 7000 && previousStage[c][op] != 4 && s.stage == 4) ++loops[c][op];
                    previousStage[c][op] = s.stage;
                }
                scope->inputs[0].setVoltage(module->outputs[0].getVoltage(c), c);
                scope->inputs[1].setVoltage(5.f * (1023 - state.attenuation) / 1023.f, c);
            }
            if (frame >= 20000 && frame < 20000 + 47 * 256) scope->process(args);
            midi.processPulses(args.sampleTime);
        }
        for (int c = 0; c < 4; ++c) for (int op = 0; op < ops; ++op) if (loops[c][op] < 2) return 4;
        std::unique_ptr<ModuleWidget> panel(model->createModuleWidget(module));
        std::unique_ptr<ModuleWidget> view(scopeModel->createModuleWidget(scope));
        std::unique_ptr<ModuleWidget> scene(new ModuleWidget);
        scene->box.size = Vec(panel->box.size.x + view->box.size.x, 380);
        view->box.pos.x = panel->box.size.x; scene->addChild(panel.release()); scene->addChild(view.release());
        capture(scene.get(), base + ".ppm");
        std::cout << model->slug << ", soft " << soft << ": all four voices loop; native Scope rendered\n";
    }
    std::cout << "Rack " << APP_VERSION << '\n';
    delete context.scene; context.scene = nullptr; delete context.window; context.window = nullptr;
    glfwTerminate(); contextSet(nullptr);
}
