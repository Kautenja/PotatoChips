// A VCV Rack widget for displaying indexed SVG frames in a buffer.
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
#include <cstdint>

#ifndef WIDGETS_INDEXED_FRAME_DISPLAY_HPP_
#define WIDGETS_INDEXED_FRAME_DISPLAY_HPP_

/// A display for showing indexed images from a frame buffer.
template<typename T>
struct IndexedFrameDisplay : TransparentWidget {
 private:
    /// TODO:
    T* module = nullptr;
    /// the SVG images representing the algorithms
    std::vector<NSVGimage*> frames;
    /// the background color for the widget
    NVGcolor background;
    /// the border color for the widget
    NVGcolor border;

 public:
    /// Initialize a new image display.
    ///
    /// @param module_ the module to get the index data from
    /// @param path the path to the directory containing the frames
    /// @param num_images the number of frames to load from disk
    /// @param position the position of the image display
    /// @param size the size of the image display
    /// @param background_ the background color for the widget
    /// @param border_ the border color for the widget
    ///
    IndexedFrameDisplay(
        T* module_,
        const std::string& path,
        unsigned num_images,
        rack::Vec position,
        rack::Vec size,
        NVGcolor background_ = {{{0.f,  0.f,  0.f,  1.f}}},
        NVGcolor border_ =     {{{0.2f, 0.2f, 0.2f, 1.f}}}
    ) : module(module_), background(background_), border(border_) {
        setPosition(position);
        setSize(size);
        for (unsigned i = 0; i < num_images; i++) {  // load each image
            auto imagePath = asset::plugin(plugin_instance, path + std::to_string(i) + ".svg");
            // TODO: rescale images appropriately in Sketch and remove 16.5mm
            frames.push_back(nsvgParseFromFile(imagePath.c_str(), "mm", 16.5));
        }
    }

    /// @brief Draw the display on the main context.
    ///
    /// @param args the arguments for the draw context for this widget
    ///
    void draw(const DrawArgs &args) override {
        // create the frame of the display
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 2.0);
        nvgFillColor(args.vg, background);
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.0);
        nvgStrokeColor(args.vg, border);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // draw the image for the selected index
        nvgBeginPath(args.vg);
        auto index = (module == nullptr) ? 0 : module->algorithm;
        svgDraw(args.vg, frames[index]);
        nvgClosePath(args.vg);
    }
};

#endif  // WIDGETS_INDEXED_FRAME_DISPLAY_HPP_
