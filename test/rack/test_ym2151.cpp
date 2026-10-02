// Real Voice 2151 event, polyphony, lifecycle and callback measurements.
// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: MIT
#include <engine/Engine.hpp>
#undef PRIVATE
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "../../src/YM2151.cpp"
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
Plugin* plugin_instance = nullptr;
namespace {
struct Host {
    rack::Context context;
    rack::Plugin plugin;
    Host() {
        rack::contextSet(&context);context.engine=new rack::engine::Engine;
        plugin.slug="KautenjaDSP-PotatoChips";plugin.version="2.1.0";
        plugin.addModel(modelYM2151);rack::plugin::plugins.push_back(&plugin);
    }
    ~Host() {rack::plugin::plugins.clear();plugin.models.clear();modelYM2151->plugin=nullptr;rack::contextSet(nullptr);}
};
void process(YM2151& module, float rate=48000) {
    Module::ProcessArgs args={};args.sampleRate=rate;args.sampleTime=1/rate;module.process(args);
}
void connect(YM2151& module,int lanes=1) {
    module.inputs[YM2151::GATE_INPUT].channels=lanes;
    module.inputs[YM2151::RETRIG_INPUT].channels=lanes;
    for(auto& output:module.outputs) output.channels=lanes;
}
}
CATCH_TEST_CASE("Voice 2151 recognizes every divider-offset pulse and event precedence") {
    Host host;
    for(float rate : {44100.f,48000.f,96000.f}) for(int offset=0;offset<16;++offset) {
        YM2151 module;connect(module);
        for(int frame=0;frame<128+offset;++frame) process(module,rate);
        module.inputs[YM2151::GATE_INPUT].setVoltage(5);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(5);
        process(module,rate);
        for(int frame=0;frame<16;++frame) process(module,rate);
        CATCH_CHECK(module.voices[0].key_writes()==1);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(0);process(module,rate);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(5);process(module,rate);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(0);process(module,rate);
        for(int frame=0;frame<16;++frame) process(module,rate);
        CATCH_CHECK(module.voices[0].key_writes()==3);
        module.inputs[YM2151::GATE_INPUT].setVoltage(0);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(5);process(module,rate);
        for(int frame=0;frame<16;++frame) process(module,rate);
        CATCH_CHECK(module.voices[0].key_writes()==4);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(0);process(module,rate);
        module.inputs[YM2151::RETRIG_INPUT].setVoltage(5);process(module,rate);
        for(int frame=0;frame<16;++frame) process(module,rate);
        CATCH_CHECK(module.voices[0].key_writes()==4);
    }
}
CATCH_TEST_CASE("Voice 2151 broadcasts mono CV and clears removed lanes") {
    Host host;YM2151 module;connect(module,4);
    module.inputs[YM2151::ALGORITHM_INPUT].channels=1;
    module.inputs[YM2151::ALGORITHM_INPUT].setVoltage(8);
    module.inputs[YM2151::PITCH_INPUT].channels=2;
    module.inputs[YM2151::PITCH_INPUT].setVoltage(1,1);
    for(int lane=0;lane<4;++lane) module.inputs[YM2151::GATE_INPUT].setVoltage(5,lane);
    for(int frame=0;frame<1000;++frame) process(module);
    CATCH_CHECK(module.outputs[0].getChannels()==4);
    CATCH_CHECK(module.outputs[1].getChannels()==4);
    for(int lane=0;lane<4;++lane) {
        CATCH_CHECK((module.voices[lane].requested_register(0x27)&7)==7);
        CATCH_CHECK(module.voices[lane].key_writes()==1);
    }
    CATCH_CHECK(module.voices[1].requested_register(0x2f) != module.voices[0].requested_register(0x2f));
    CATCH_CHECK(module.voices[2].requested_register(0x2f) == module.voices[0].requested_register(0x2f));
    connect(module);module.inputs[YM2151::PITCH_INPUT].channels=1;process(module);
    CATCH_CHECK(module.voices[3].key_writes()==0);
    connect(module,4);
    for(int lane=1;lane<4;++lane) module.inputs[YM2151::GATE_INPUT].setVoltage(0,lane);
    for(int frame=0;frame<1000;++frame) process(module);
    for(int lane=1;lane<4;++lane) CATCH_CHECK(module.outputs[0].getVoltage(lane)==0);
    module.onReset();process(module);CATCH_CHECK(module.active==4);
}
CATCH_TEST_CASE("Voice 2151 default is audible, releases, and survives rate changes and JSON") {
    Host host;YM2151 module;connect(module);
    module.inputs[YM2151::GATE_INPUT].setVoltage(5);
    double energy=0;
    for(float rate : {44100.f,96000.f,48000.f}) for(int i=0;i<4096;++i) {
        process(module,rate);float v=module.outputs[0].getVoltage();
        CATCH_REQUIRE(std::isfinite(v));CATCH_REQUIRE(std::abs(v)<=10);
        CATCH_CHECK(v==module.outputs[1].getVoltage());energy+=v*v;
    }
    CATCH_CHECK(energy>1);
    module.inputs[YM2151::GATE_INPUT].setVoltage(0);
    for(int i=0;i<24000;++i) process(module);
    CATCH_CHECK(std::abs(module.outputs[0].getVoltage())<.001);
    module.params[YM2151::NOISE].setValue(1);
    module.model=modelYM2151;
    json_t* saved=module.toJson();YM2151 restored;restored.model=modelYM2151;restored.fromJson(saved);json_decref(saved);
    for(int i=0;i<YM2151::NUM_PARAMS;++i) CATCH_CHECK(module.params[i].getValue()==restored.params[i].getValue());
    restored.dataFromJson(nullptr);
}
CATCH_TEST_CASE("Voice 2151 callback budget", "[.][benchmark]") {
    Host host;
    std::puts("rate,frames,lanes,median_us,p99_us,max_us,budget_us,module_bytes");
    for(float rate : {44100.f,48000.f,96000.f}) for(int frames : {64,256}) for(int lanes : {1,4,16}) {
        YM2151 module;connect(module,lanes);
        module.params[YM2151::LFO].setValue(230);module.params[YM2151::AMD].setValue(127);
        module.params[YM2151::PMD].setValue(127);module.params[YM2151::AMS].setValue(3);
        module.params[YM2151::PMS].setValue(7);module.params[YM2151::NOISE].setValue(1);
        for(int lane=0;lane<lanes;++lane)module.inputs[YM2151::GATE_INPUT].setVoltage(5,lane);
        for(int i=0;i<2048;++i)process(module,rate);
        std::vector<double> times;
        for(int repeat=0;repeat<300;++repeat) {
            auto start=std::chrono::steady_clock::now();
            for(int i=0;i<frames;++i) process(module,rate);
            times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());
        std::printf("%.0f,%d,%d,%.3f,%.3f,%.3f,%.3f,%zu\n",rate,frames,lanes,times[150],times[297],times.back(),frames/rate*1e6,sizeof(module)+16*module.voices[0].heap_bytes());
        CATCH_CHECK(times[150] < frames/rate*1e6*.50);
    }
}

CATCH_TEST_CASE("Voice 2151 presets, randomize and concurrent instances remain bounded") {
    Host host;
    for(const auto& path : rack::system::getEntries("presets/YM2151")) {
        auto module=std::unique_ptr<YM2151>(static_cast<YM2151*>(modelYM2151->createModule()));
        json_t* data=json_load_file(path.c_str(),0,nullptr);CATCH_REQUIRE(data);
        module->fromJson(data);json_decref(data);connect(*module,16);
        for(int lane=0;lane<16;++lane) module->inputs[YM2151::GATE_INPUT].setVoltage(5,lane);
        double energy=0;
        for(int frame=0;frame<4096;++frame) {
            process(*module);
            for(int lane=0;lane<16;++lane) energy+=std::abs(module->outputs[0].getVoltage(lane));
        }
        CATCH_INFO(path);CATCH_CHECK(energy>1);
        YM2151 other;connect(other);other.inputs[YM2151::GATE_INPUT].setVoltage(5);
        for(auto* param : module->paramQuantities) param->randomize();
        for(int frame=0;frame<2048;++frame) {
            process(*module);process(other);
            for(int lane=0;lane<16;++lane) {
                CATCH_REQUIRE(std::isfinite(module->outputs[0].getVoltage(lane)));
                CATCH_REQUIRE(std::abs(module->outputs[0].getVoltage(lane))<=10);
            }
        }
        CATCH_CHECK(other.voices[0].overloads()==0);
    }
}
