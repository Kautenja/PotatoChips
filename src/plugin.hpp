// The NES Oscillators VCVRack plugin.
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

#include "rack.hpp"
#include "kautenja_rack/helpers.hpp"
#include "kautenja_rack/param_quantity.hpp"

#ifndef PLUGIN_HPP
#define PLUGIN_HPP

using namespace rack;

/// the base clock rate of the VCV Rack environment
static constexpr uint32_t CLOCK_RATE = 768000;

/// the global instance of the plug-in
extern rack::Plugin* plugin_instance;

extern rack::Model *modelBlocks;
extern rack::Model *modelJairasullator;
extern rack::Model *modelInfiniteStairs;
extern rack::Model *modelPotKeys;
extern rack::Model *modelStepSaw;
extern rack::Model *modelPulses;
extern rack::Model *modelNameCorpOctalWaveGenerator;
extern rack::Model *modelPalletTownWavesSystem;
extern rack::Model *modelMegaTone;
extern rack::Model *modelBossFight;
extern rack::Model *modelMiniBoss;
extern rack::Model *modelSuperSynth;
extern rack::Model *modelSuperEcho;
extern rack::Model *modelSuperADSR;
extern rack::Model *modelSuperSampler;
extern rack::Model *modelSuperVCA;

extern rack::Model *modelChipS_SMP_Blank1;
extern rack::Model *modelBossFight_Blank1;

#endif  // PLUGIN_HPP
