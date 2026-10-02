// OPM clock, bus, isolation and independent Nuked-OPM reference checks.
// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: MIT
#define CATCH_CONFIG_PREFIX_ALL
#include "catch_amalgamated.hpp"
#include "dsp/yamaha_ym2151/voice.hpp"
#include "../../../dep/Nuked-OPM/opm.h"
#include <vector>
#include <cmath>
using YamahaYM2151::Voice;
namespace {
void tone(Voice& voice, int algorithm = 7) {
    voice.write(0x27, 0xc0 | algorithm);
    voice.pitch(0);
    for (int slot : {7, 23, 15, 31}) {
        voice.write(0x40 + slot, 1);
        voice.write(0x60 + slot, slot == 31 ? 0 : 127);
        voice.write(0x80 + slot, 31);
        voice.write(0xe0 + slot, 15);
    }
    voice.prepare(); voice.key(true);
}
std::vector<float> render(Voice& voice, int frames) {
    std::vector<float> data;
    for (int i = 0; i < frames; ++i) {
        float sample[2]; voice.process(sample);
        CATCH_REQUIRE(std::isfinite(sample[0]));
        CATCH_REQUIRE(std::abs(sample[0]) < 1.5f);
        data.push_back(sample[0]);
    }
    return data;
}
double frequency(const std::vector<float>& data, double rate) {
    std::vector<double> edges;
    for (unsigned i = data.size()/4 + 1; i < data.size(); ++i)
        if (data[i-1] <= 0 && data[i] > 0)
            edges.push_back(i - data[i] / (data[i] - data[i-1]));
    CATCH_REQUIRE(edges.size() > 10);
    return rate * (edges.size()-1) / (edges.back()-edges.front());
}
double rms(const std::vector<float>& data) {
    double sum=0;
    for (unsigned i=data.size()/4;i<data.size();++i) sum+=data[i]*data[i];
    return std::sqrt(sum / (data.size()-data.size()/4));
}
}

CATCH_TEST_CASE("OPM pitch and release are host-rate independent") {
    for (double rate : {44100.,48000.,96000.}) {
        Voice voice; voice.set_sample_rate(rate); tone(voice);
        auto data=render(voice, static_cast<int>(rate * .25));
        CATCH_CHECK(frequency(data,rate) == Catch::Approx(261.625565).epsilon(.003));
        CATCH_CHECK(rms(data) > .05);
        voice.key(false);
        auto tail=render(voice, static_cast<int>(rate * .25));
        CATCH_CHECK(std::abs(tail.back()) < .0001);
    }
}
CATCH_TEST_CASE("OPM bus keeps key edges and rejects whole overloaded retriggers") {
    Voice voice; tone(voice);
    for (int i=0;i<31;++i) CATCH_REQUIRE(voice.key(true,true));
    CATCH_CHECK_FALSE(voice.key(true,true));
}
CATCH_TEST_CASE("OPM coalescing, reset, routing and independent LFO/noise state") {
    Voice a,b; tone(a); tone(b);
    for(int i=0;i<1000;++i) {
        float x[2],y[2]; a.process(x); b.process(y);
        CATCH_CHECK(x[0] == y[0]); CATCH_CHECK(x[0] == x[1]);
    }
    a.write(0x0f,0x9f); a.write(0x18,255); a.write(0x19,127); a.write(0x1a,127);
    auto changed=render(a,4096); auto untouched=render(b,4096);
    CATCH_CHECK(changed != untouched);
    CATCH_CHECK(a.requested_register(0x19) == 127);
    CATCH_CHECK(a.requested_register(0x1a) == 127);
    a.reset(); b.reset(); tone(a); tone(b);
    CATCH_CHECK(bool(render(a,4096) == render(b,4096)));
    a.write(0x27,0x47);
    for(int i=0;i<1000;++i) {float out[2];a.process(out);if(i>100) CATCH_CHECK(out[1]==0);}
    for(int i=0;i<10000;++i) a.write(0x18,i%256);
    CATCH_CHECK(a.queued_controls() == 1);
}
CATCH_TEST_CASE("OPM algorithms and chip-specific controls affect synthesis") {
    for(int algorithm=0;algorithm<8;++algorithm) {
        Voice a; tone(a,algorithm);
        for(int slot : {7,23,15,31}) a.write(0x60+slot,16);
        CATCH_CHECK(rms(render(a,4096)) > .001);
    }
    for(int wave=0;wave<4;++wave) {
        Voice a,b; tone(a);tone(b);
        a.write(0x18,240);a.write(0x19,127);a.write(0x1a,127);
        a.write(0x1b,wave);a.write(0x3f,0x73);a.write(0xbf,0x80);
        CATCH_CHECK(bool(render(a,8192) != render(b,8192)));
    }
    for(int field : {0,1,2,3,4,5}) {
        Voice a,b; tone(a,0);tone(b,0);
        for(int slot : {7,23,15,31}) {a.write(0x60+slot,16);b.write(0x60+slot,16);}
        if(field==0) a.write(0x27,0xf8); // feedback on an audible modulation chain
        if(field==1) a.write(0x5f,0x7f); // multiplier and DT1
        if(field==2) a.write(0x7f,127);  // carrier total level
        if(field==3) a.write(0x9f,0);    // zero attack cannot start a fresh envelope
        if(field==4) {a.write(0xbf,31);a.write(0xff,0xff);} // first decay and SL
        if(field==5) a.write(0xdf,0xdf); // DT2 and sustain rate
        CATCH_CHECK(bool(render(a,8192) != render(b,8192)));
    }
}
CATCH_TEST_CASE("OPM sine agrees with independent pinned Nuked decoded DAC") {
    opm_t reference; OPM_Reset(&reference,0);
    int32_t out[2];
    auto tick=[&](int count){for(int i=0;i<count;++i) OPM_Clock(&reference,out,nullptr,nullptr,nullptr);};
    auto write=[&](int address,int data){OPM_Write(&reference,0,address);tick(2);OPM_Write(&reference,1,data);tick(64);};
    write(0x27,0xc7);write(0x2f,0x3e);write(0x37,0);
    for(int slot : {7,23,15,31}) {write(0x40+slot,1);write(0x60+slot,slot==31?0:127);write(0x80+slot,31);write(0xe0+slot,15);}
    write(8,0x7f);
    std::vector<float> data;
    for(int i=0;i<14000;++i) {tick(32);data.push_back(out[0]/32768.f);}
    Voice voice;voice.set_sample_rate(Voice::NATIVE_RATE);tone(voice);
    auto actual=render(voice,14000);
    CATCH_CHECK(frequency(actual,Voice::NATIVE_RATE) == Catch::Approx(frequency(data,Voice::NATIVE_RATE)).epsilon(.003));
    CATCH_CHECK(rms(actual) == Catch::Approx(rms(data)).epsilon(.03));
}

CATCH_TEST_CASE("OPM eight algorithms match independent core energy") {
    for(int algorithm=0;algorithm<8;++algorithm) {
        opm_t reference;OPM_Reset(&reference,0);
        int32_t out[2];
        auto tick=[&](int count){for(int i=0;i<count;++i) OPM_Clock(&reference,out,nullptr,nullptr,nullptr);};
        auto write=[&](int a,int d){OPM_Write(&reference,0,a);tick(2);OPM_Write(&reference,1,d);tick(64);};
        write(0x27,0xc0|algorithm);write(0x2f,0x3e);
        Voice voice;voice.set_sample_rate(Voice::NATIVE_RATE);tone(voice,algorithm);
        for(int slot : {7,23,15,31}) {
            write(0x40+slot,1);write(0x60+slot,16);write(0x80+slot,31);write(0xe0+slot,15);
            voice.write(0x60+slot,16);
        }
        write(8,0x7f);
        std::vector<float> reference_audio;
        for(int i=0;i<8192;++i) {tick(32);reference_audio.push_back(out[0]/32768.f);}
        auto actual=render(voice,8192);
        CATCH_INFO(algorithm);
        CATCH_CHECK(rms(actual)==Catch::Approx(rms(reference_audio)).epsilon(.10));
    }
}
CATCH_TEST_CASE("OPM queued event latency is bounded under continuous CV traffic") {
    Voice voice;tone(voice);render(voice,128);
    uint64_t count=voice.key_writes();
    CATCH_REQUIRE(voice.key(true,true));
    unsigned frames=0;
    while(voice.key_writes()<count+2 && frames<128) {
        voice.pitch(frames*.01f);voice.write(0x18,frames);voice.write(0x19,frames);
        float out[2];voice.process(out);++frames;
    }
    CATCH_CHECK(frames<=3);
    CATCH_CHECK(voice.overloads()==0);
    voice.reset();tone(voice);
    for(int i=0;i<31;++i)CATCH_REQUIRE(voice.key(true,true));
    CATCH_CHECK_FALSE(voice.key(true,true));
    CATCH_REQUIRE(voice.key(false));
    render(voice,128);
    CATCH_CHECK(voice.queued_keys()==0);
    CATCH_CHECK(voice.key_writes()==64);
}

CATCH_TEST_CASE("OPM modulation detune noise and envelopes agree with reference statistics") {
    using Writes = std::vector<std::pair<int,int>>;
    std::vector<Writes> scenarios;
    for (int wave=0;wave<4;++wave) {
        scenarios.push_back({{0x18,230},{0x19,95},{0x3f,3},{0xbf,0x80},{0x1b,wave}});
        scenarios.push_back({{0x18,230},{0x19,0xff},{0x3f,0x70},{0x1b,wave}});
    }
    for (int detune : {0,3,4,7}) scenarios.push_back({{0x5f,(detune<<4)|1}});
    for (int detune=0;detune<4;++detune) scenarios.push_back({{0xdf,detune<<6}});
    scenarios.push_back({{0x5f,0}}); scenarios.push_back({{0x5f,15}});
    scenarios.push_back({{0x0f,0x80}});scenarios.push_back({{0x0f,0x9f}});
    scenarios.push_back({{0x9f,0}}); // AR zero: no attack from silence
    scenarios.push_back({{0xbf,12},{0xff,0x8f},{0xdf,5}});
    for (unsigned scenario=0;scenario<scenarios.size();++scenario) {
        opm_t reference;OPM_Reset(&reference,0);int32_t output[2];
        auto tick=[&](int count){for(int i=0;i<count;++i)OPM_Clock(&reference,output,nullptr,nullptr,nullptr);};
        auto bus=[&](int address,int value){OPM_Write(&reference,0,address);tick(2);OPM_Write(&reference,1,value);tick(64);};
        Voice voice;voice.set_sample_rate(Voice::NATIVE_RATE);
        auto both=[&](int address,int value){
            bus(address,value);
            if(address==0x19 && value&0x80)voice.write(0x1a,value&127);
            else voice.write(address,value);
        };
        both(0x27,0xc7);both(0x2f,0x3e);
        for(int slot : {7,23,15,31}) {
            both(0x40+slot,1);both(0x60+slot,slot==31?0:127);
            both(0x80+slot,31);both(0xe0+slot,15);
        }
        for(auto change:scenarios[scenario])both(change.first,change.second);
        voice.prepare();voice.key(true);bus(8,0x7f);
        std::vector<float> expected;
        for(int frame=0;frame<28000;++frame){tick(32);expected.push_back(output[0]/32768.f);}
        // Apply the documented reconstruction filter independently offline.
        // Comparing filtered noise to the raw DAC would measure the filter,
        // rather than agreement between the two synthesis engines.
        std::vector<float> filtered(expected.size(),0);
        double taps[32], total=0;
        const double pi=std::acos(-1.0);
        for(int tap=0;tap<32;++tap) {
            double x=tap-15.5;
            taps[tap]=std::sin(2*pi*20000/Voice::NATIVE_RATE*x)/(pi*x)
                *(.42-.5*std::cos(2*pi*tap/31)+.08*std::cos(4*pi*tap/31));
            total+=taps[tap];
        }
        for(unsigned frame=32;frame<expected.size();++frame)
            for(int tap=0;tap<32;++tap)filtered[frame]+=expected[frame-tap]*taps[tap]/total;
        auto actual=render(voice,28000);
        CATCH_INFO(scenario);
        CATCH_CHECK(rms(actual)==Catch::Approx(rms(filtered)).epsilon(.10).margin(.002));
    }
}
