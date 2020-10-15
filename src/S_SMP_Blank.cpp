// A low-pass gate module based on the S-SMP chip from Nintendo SNES.
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

#include "plugin.hpp"

/// @brief a dummy module structure for creating panel blanks.
struct BlankModule : Module { };

/// @brief the different configurations for placing screws on a panel
enum ScrewStyle { All, TopLeft, TopRight };

/// @brief A panel blank that shows a graphic.
template<typename Screw, ScrewStyle screwStyle>
struct BlankWidget : ModuleWidget {
    /// @brief Initialize a new blank panel widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit BlankWidget(BlankModule *module) {
        setModule(module);
        static constexpr auto panel = "res/S-SMP-Chip.svg";
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, panel)));
        switch (screwStyle) {  // panel screws
        case ScrewStyle::All:
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        case ScrewStyle::TopLeft:
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        case ScrewStyle::TopRight:
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        }
    }
};

extern constexpr char const blank1[] = "res/S-SMP-Chip.svg";
rack::Model *modelChipS_SMP_Blank =
    createModel<BlankModule, BlankWidget<ScrewSilver, ScrewStyle::All>>("S_SMP_Blank1");
