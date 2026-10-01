// Native widget rendering shared by panel and envelope-scope inspections.
// Adapted from RackNES tools/capture and Fourier test/rack/inspect_panels.
// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CAPTURE_RENDER_HPP_
#define CAPTURE_RENDER_HPP_
#include <rack.hpp>
#include <fstream>
#include <stdexcept>
#include <vector>

/// Check component caches and SVG resources, including late child controls.
static bool ready(Widget* widget, bool nested = false) {
    if (auto svg = dynamic_cast<SvgWidget*>(widget))
        if (!svg->svg || !svg->svg->handle)
            throw std::runtime_error("Missing widget SVG resource");
    bool settled = true;
    if (auto cache = dynamic_cast<FramebufferWidget*>(widget)) {
        if (!nested && !cache->bypassed)
            settled = !cache->dirty && cache->getImageHandle() > 0;
        nested = true;
    }
    for (auto child : widget->children) settled = ready(child, nested) && settled;
    return settled;
}

/// Draw production layers until every component framebuffer settles.
static int capture(ModuleWidget* widget, const std::string& filename,
    float zoom = 1.f, float brightness = 1.f) {
    const int canvas_width = int(std::ceil(widget->box.size.x * zoom)) + 20;
    const int canvas_height = int(std::ceil(380 * zoom)) + 40;
    glfwSetWindowSize(APP->window->win, canvas_width, canvas_height);
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(APP->window->win, &width, &height);
    if (width % canvas_width || height != canvas_height * (width / canvas_width))
        throw std::runtime_error("Unexpected desktop framebuffer dimensions");
    const int ratio = width / canvas_width;
    if (ratio < 1) throw std::runtime_error("Invalid desktop pixel ratio");
    APP->window->pixelRatio = ratio;
    bool settled = false;
    for (int frame = 0; frame < 160; ++frame) {
        widget->step();
        auto vg = APP->window->vg;
        nvgBeginFrame(vg, canvas_width, canvas_height, ratio);
        Widget::DrawArgs args = {};
        args.vg = vg;
        args.clipBox = Rect(Vec(-10.f / zoom, -20.f / zoom),
            Vec(canvas_width / zoom, canvas_height / zoom));
        APP->window->fbCount() = 0;
        nvgSave(vg);
        nvgTranslate(vg, 10.f, 20.f);
        nvgScale(vg, zoom, zoom);
        widget->draw(args);
        // Rack's room dimming outside the mouse spotlight, before emissive lights.
        if (brightness < 1.f) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, widget->box.size.x, widget->box.size.y);
            nvgFillColor(vg, nvgRGBAf(0, 0, 0, 1.f - brightness));
            nvgFill(vg);
        }
        widget->drawLayer(args, 1);
        nvgRestore(vg);
        glViewport(0, 0, width, height);
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgEndFrame(vg);
        glFinish();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL draw failed");
        if (ready(widget) && frame >= 3) { settled = true; break; }
    }
    if (!settled) throw std::runtime_error("Component framebuffers did not settle");
    std::vector<unsigned char> pixels(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Framebuffer read failed");
    std::ofstream output(filename, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        output.write(reinterpret_cast<const char*>(pixels.data() + y * width * 3), width * 3);
    if (!output) throw std::runtime_error("Cannot write " + filename);
    return ratio;
}

#endif  // CAPTURE_RENDER_HPP_
