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

#ifndef PLUGIN_HPP
#define PLUGIN_HPP

using namespace rack;

/// the base clock rate of the VCV Rack environment
static constexpr uint32_t CLOCK_RATE = 768000;

/// the global instance of the plug-in
extern rack::Plugin* plugin_instance;

// pointers to each module in the plug-in

// current releases
extern rack::Model *modelNameCorpOctalWaveGenerator;
extern rack::Model *modelJairasullator;
extern rack::Model *modelBuzzyBeetle;
extern rack::Model *modelPalletTownWavesSystem;
extern rack::Model *modelTroglocillator;
extern rack::Model *modelEscillator;
extern rack::Model *modelGleeokillator;
extern rack::Model *modelBossFight;
extern rack::Model *modelMegaTone;

// blanks
extern rack::Model *modelChipS_SMP_Blank;
extern rack::Model *model2612_Blank;

// beta versions / WIPs
extern rack::Model *modelChipS_SMP;
extern rack::Model *modelChipS_SMP_ADSR;
extern rack::Model *modelChipS_SMP_BRR;
extern rack::Model *modelChipS_SMP_Echo;
extern rack::Model *modelChipS_SMP_Gauss;
extern rack::Model *modelChipSCC;
extern rack::Model *modelChipTurboGrafx16;

/// @brief Create a parameter that snaps to integer values.
///
/// @tparam P the type of the parameter to initialize
/// @tparam Args the type of arguments to pass to the `createParam` function
/// @tparam args the arguments to pass to the `createParam` function
/// @returns a pointer to the freshly allocated parameter
///
template<typename P, typename... Args>
inline ParamWidget* createSnapParam(Args... args) {
    auto param = createParam<P>(args...);
    param->snap = true;
    return param;
}

#endif  // PLUGIN_HPP
