// The NES Oscillators VCVRack plugin.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
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

extern rack::Model *modelChip106;
extern rack::Model *modelChip2413;
extern rack::Model *modelChip2612;
extern rack::Model *modelChip2A03;
extern rack::Model *modelChipAY_3_8910;
extern rack::Model *modelChipFME7;
extern rack::Model *modelChipGBS;
extern rack::Model *modelChipPOKEY;
extern rack::Model *modelChipS_SMP;
extern rack::Model *modelChipS_SMP_Echo;
extern rack::Model *modelChipSCC;
extern rack::Model *modelChipSN76489;
extern rack::Model *modelChipTurboGrafx16;
extern rack::Model *modelChipVRC6;

#endif  // PLUGIN_HPP
