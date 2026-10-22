#!/usr/bin/env python3
"""Production Indexer Texture dynamic sizes and reactivation on macOS CGL.

Uses the existing native OF fixture. Does not launch Lightnet or load user data.
The sibling ofxOceanodeTextures addon must be available.
"""
import macro_mixer_regression as native
from of_environment import of_root

native.PRELUDE = native.PRELUDE.replace(
    'void ofxOceanodeContainer::flushPendingScopeSave() { assert(false); }',
    'void ofxOceanodeContainer::flushPendingScopeSave() {}')
native.PRELUDE += r'''
#include <OpenGL/OpenGL.h>
#include "ofGLProgrammableRenderer.h"
#include "indexerTexture.h"
'''
native.CASES = r'''
int main(int argc, char** argv){
    ofDisableArbTex();
    oceanodeGlDiagnostics::enabled() = false; // Do not let tracing consume test errors.
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAAllowOfflineRenderers, (CGLPixelFormatAttribute)0};
    CGLPixelFormatObj format = nullptr;
    GLint screens = 0;
    CGLContextObj context = nullptr;
    assert(CGLChoosePixelFormat(attributes, &format, &screens) == kCGLNoError && format);
    assert(CGLCreateContext(format, nullptr, &context) == kCGLNoError && context);
    CGLDestroyPixelFormat(format);
    assert(CGLSetCurrentContext(context) == kCGLNoError);
    glewExperimental = GL_TRUE;
    assert(glewInit() == GLEW_OK);
    while(glGetError() != GL_NO_ERROR) {}

    auto window = std::make_shared<ofAppNoWindow>();
    ofWindowSettings settings;
    settings.setSize(64, 64);
    window->setup(settings);
    ofGetMainLoop()->setCurrentWindow(window);
    auto renderer = std::make_shared<ofGLProgrammableRenderer>(window.get());
    window->renderer() = renderer;
    renderer->setup(4, 1);
    renderer->startRender();
    // CGL has no window framebuffer; give subsequent OF rendering a valid target.
    ofFbo canvas;
    canvas.allocate(64, 64, GL_RGBA32F);
    canvas.begin();
    while(glGetError() != GL_NO_ERROR) {}

    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = std::make_shared<ofxOceanodeTypesRegistry>();
    auto transport = std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry, types, transport);
    auto& model = parent.createNode(std::make_unique<indexerTexture>(), 1).getNodeModel();
    ofEventArgs args;
    auto check = [&](std::vector<int> widths, std::vector<int> heights){
        model.getParameter<std::vector<int>>("Size.X") = widths;
        model.getParameter<std::vector<int>>("Size.Y") = heights;
        assert(glGetError() == GL_NO_ERROR);
        model.draw(args);
        assert(glGetError() == GL_NO_ERROR);
        auto* output = model.getParameter<ofTexture*>("Output").get();
        const int w = widths.empty() ? 0 : *std::max_element(widths.begin(), widths.end());
        const int h = heights.empty() ? 0 : *std::max_element(heights.begin(), heights.end());
        if(w <= 0 || h <= 0){
            assert(output == nullptr);
        }else{
            assert(output && output->isAllocated());
            assert(output->getWidth() == w && output->getHeight() == h);
            ofFloatPixels pixels;
            output->readToPixels(pixels);
            assert(glGetError() == GL_NO_ERROR);
            assert(pixels.getWidth() == w && pixels.getHeight() == h);
        }
    };
    check({}, {}); // Invalid input before the first GPU allocation.
    check({252}, {18}); // Dimensions used by KALEIDOS_MACRO_3.
    check({252}, {18});
    check({42}, {1});
    check({252}, {18});
    check({0}, {18});
    check({252}, {18});
    check({252}, {0});
    check({252}, {18});
    check({}, {18});
    check({252}, {});
    check({-1}, {18});
    check({252}, {-1});
    check({1}, {1});
    check({42, 84, 252}, {3, 18});
    check({252}, {18});
    for(int i = 0; i < 3; ++i){
        model.deactivate();
        check({252}, {18});
    }
    parent.clearContainer();
    canvas.end();
    std::cout << "PASS Indexer Texture: dynamic/non-square dimensions, invalid/empty inputs, recovery, pixels and reactivation\n";
}
'''
native.PRELUDE += '#include "ofxOceanodeGlDiagnostics.h"\n'

if __name__ == '__main__':
    textures = of_root() / 'addons/ofxOceanodeTextures'
    native.main(extra_sources=[str(textures / 'src/indexerTexture.cpp')],
                extra_includes=[textures / 'src', textures / 'src/shaders'])
