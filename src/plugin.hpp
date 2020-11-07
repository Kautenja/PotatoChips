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
#include "components.hpp"

#ifndef PLUGIN_HPP
#define PLUGIN_HPP

using namespace rack;

/// the base clock rate of the VCV Rack environment
static constexpr uint32_t CLOCK_RATE = 768000;

/// the global instance of the plug-in
extern rack::Plugin* plugin_instance;

// pointers to each module in the plug-in

// current releases
extern rack::Model *modelJairasullator;
extern rack::Model *modelTerracillator;
extern rack::Model *modelPotillator;
extern rack::Model *modelEscillator;
extern rack::Model *modelGleeokillator;
extern rack::Model *modelNameCorpOctalWaveGenerator;
extern rack::Model *modelPalletTownWavesSystem;
extern rack::Model *modelMegaTone;
extern rack::Model *modelBossFight;

// blanks
extern rack::Model *modelChipS_SMP_Blank1;
extern rack::Model *modelBossFight_Blank1;

// beta versions / WIPs
extern rack::Model *modelChipS_SMP;
extern rack::Model *modelChipS_SMP_ADSR;
extern rack::Model *modelChipS_SMP_BRR;
extern rack::Model *modelChipS_SMP_Echo;
extern rack::Model *modelChipS_SMP_Gauss;
extern rack::Model *modelChipSCC;
extern rack::Model *modelChipTurboGrafx16;

#endif  // PLUGIN_HPP
