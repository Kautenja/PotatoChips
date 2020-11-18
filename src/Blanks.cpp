// Blank panels.
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

#include "plugin.hpp"

/// @brief the different configurations for placing screws on a panel
enum ScrewStyle { None, All, TopLeft, TopRight };

/// @brief A panel blank that shows a graphic.
/// @tparam panelPath the path to the SVG file for the panel graphic
/// @tparam style the style for rendering screws on the panel
/// @tparam Screw the type for the screw SVG to render
template<const char* panelPath, ScrewStyle style, typename Screw = ScrewSilver>
struct BlankWidget : ModuleWidget {
    /// @brief Initialize a new blank panel widget.
    ///
    /// @param module the back-end module to interact with
    ///
    explicit BlankWidget(Module *module) {
        setModule(module);
        const std::string fileName(panelPath);
        setPanel(APP->window->loadSvg(asset::plugin(plugin_instance, fileName)));
        switch (style) {  // panel screws
        case ScrewStyle::None:  // no screws
            break;
        case ScrewStyle::All:  // all screws
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        case ScrewStyle::TopLeft:  // top left + bottom right
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        case ScrewStyle::TopRight:  // top right + bottom left
            addChild(createWidget<Screw>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
            addChild(createWidget<Screw>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            break;
        }
    }
};

extern constexpr char const blank1[] = "res/S-SMP-Chip.svg";
rack::Model *modelChipS_SMP_Blank1 = createModel<Module,
    BlankWidget<blank1, ScrewStyle::All, ScrewSilver>
>("SuperSynthBlank1");

extern constexpr char const blank2[] = "res/BossFight-Envelope.svg";
rack::Model *modelBossFight_Blank1 = createModel<Module,
    BlankWidget<blank2, ScrewStyle::All, ScrewSilver>
>("2612_Blank1");
