#!/usr/bin/env python3
"""Native CGL pixels, retained uploads, optional async readback and mapping cache.

Does not launch Lightnet or publish Syphon. Uses production addon/Lightnet nodes
with ASan/UBSan. Oscillator/index precision is deliberately outside the policy.
"""
import os
# OF is precompiled without libc++ vector container annotations; keep ASan heap
# checks, but disable container annotations across that mixed library boundary.
os.environ.setdefault("ASAN_OPTIONS", "detect_container_overflow=0")
import indexer_texture_gl_regression as fixture
from of_environment import of_root, first_existing

native = fixture.native
native.PRELUDE += r'''
#include "textureReader.h"
#include "vectorToTexture.h"
#include "ColorTexture.h"
#include "textureResize.h"
#include "textureRegionMapper.h"
#include "mixer.h"
#include "mixerAlpha.h"
#include "simpleEffect.h"
#include "Gradient.h"
#include "Gradient2.h"
#include "ColorByValue.h"
#include "TextureChannels.h"
#include "injectAlpha.h"
#include "subTexture.h"
#include "noiseTexture.h"
#include "textureFlip.h"
'''
# Reuse only the established CGL/OF fixture setup.
native.CASES = native.CASES[:native.CASES.index('    auto& model =')]+r'''
    auto make = [&](auto value) -> ofxOceanodeNodeModel& {
        return parent.createNode(std::move(value), 1).getNodeModel();
    };
    auto& vector = make(std::make_unique<vectorToTexture>());
    auto& reader = make(std::make_unique<textureReader>());
    auto& mapper = make(std::make_unique<textureRegionMapper>());
    auto& color = make(std::make_unique<ColorTexture>());
    auto& resize = make(std::make_unique<textureResize>());
    ofEventArgs args;
    auto near = [](float a, float b, float tolerance = .00001f){
        if(std::abs(a-b)>tolerance){ std::cerr << a << " != " << b << std::endl; std::abort(); }
    };
    auto output = [&](ofxOceanodeNodeModel& m, const char* name){
        auto* tex = m.getParameter<ofTexture*>(name).get();
        assert(tex && tex->isAllocated()); return tex;
    };
    vector.getParameter<int>("Width") = 2;
    vector.getParameter<int>("Height") = 2;
    vector.getParameter<std::vector<float>>("Input") = {.1f,.3f,.6f,.9f,1.0f};
    vector.draw(args);
    auto* texture = output(vector, "Output Tex");
    const auto id = texture->getTextureData().textureID;
    vector.draw(args);
    assert(id == output(vector, "Output Tex")->getTextureData().textureID);
    // Compare the direct upload with the previous upload+FBO path, including orientation.
    ofTexture oldUpload;
    oldUpload.allocate(2,2,GL_RGBA32F);
    float referenceData[] = {.1f,.1f,.1f,1,.3f,.3f,.3f,1,.6f,.6f,.6f,1,.9f,.9f,.9f,1};
    oldUpload.loadData(referenceData,2,2,GL_RGBA);
    oldUpload.setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    ofFbo reference; reference.allocate(2,2,GL_RGBA32F);
    reference.begin(); ofClear(0,0,0,255); oldUpload.draw(0,0,2,2); reference.end();
    ofFloatPixels oldPixels, newPixels;
    reference.readToPixels(oldPixels); texture->readToPixels(newPixels);
    for(size_t i=0;i<oldPixels.size();++i) near(oldPixels[i],newPixels[i]);
    reader.getParameter<ofTexture*>("Input") = texture;
    reader.draw(args);
    const auto values = reader.getParameter<std::vector<float>>("Output").get();
    assert(values.size() == 4);
    near(values[0], .1f); near(values[1], .3f); near(values[2], .6f); near(values[3], .9f);
    reader.getInspectorParameter<bool>("Add Separator") = true;
    reader.draw(args);
    auto separated = reader.getParameter<std::vector<float>>("Output").get();
    assert(separated.size() == 6); near(separated[2], -1); near(separated[5], -1);
    reader.getInspectorParameter<bool>("Async Readback") = true;
    vector.getParameter<std::vector<float>>("Input") = {.2f,.4f,.5f,.8f};
    vector.draw(args);
    reader.draw(args); // Submit only; the old output is still available.
    assert(reader.getParameter<std::vector<float>>("Output").get() == separated);
    glFinish(); // Test controls GPU completion; production never waits this way.
    reader.draw(args);
    auto delayed = reader.getParameter<std::vector<float>>("Output").get();
    near(delayed[0], .2f); near(delayed[1], .4f); near(delayed[2], -1); near(delayed[3], .5f);
    reader.deactivate(); // Pending PBOs/fences are safely discarded.
    reader.getInspectorParameter<bool>("Async Readback") = false;
    vector.getParameter<std::vector<float>>("Input") = {.7f};
    vector.draw(args); reader.draw(args);
    near(reader.getParameter<std::vector<float>>("Output").get()[0], .7f);
    near(reader.getParameter<std::vector<float>>("Output").get()[1], 0);
    float paddedReference[] = {.7f,.7f,.7f,1,0,0,0,0,0,0,0,0,0,0,0,0};
    oldUpload.loadData(paddedReference,2,2,GL_RGBA);
    reference.begin(); ofClear(0,0,0,255); oldUpload.draw(0,0,2,2); reference.end();
    reference.readToPixels(oldPixels); output(vector,"Output Tex")->readToPixels(newPixels);
    for(size_t i=0;i<oldPixels.size();++i) near(oldPixels[i],newPixels[i]);
    // Format changes reuse dimensions and preserve alpha, with expected quantization.
    color.getParameter<int>("Res X") = 2; color.getParameter<int>("Res Y") = 2;
    color.getParameter<ofFloatColor>("Color") = ofFloatColor(.2f,.4f,.6f,.3f);
    resize.getParameter<int>("Width") = 3; resize.getParameter<int>("Height") = 2;
    for(int precision : {32,16,8,32}){
        ofxOceanodeShared::setColorTexturePrecision(precision);
        vector.draw(args); color.draw(args);
        auto* c = output(color,"Output");
        resize.getParameter<ofTexture*>("Input") = c; resize.draw(args);
        for(auto* tex : {output(vector,"Output Tex"), c, output(resize,"Output")}){
            assert(tex->getTextureData().glInternalFormat == ofxOceanodeShared::getColorTextureInternalFormat());
        }
        ofFloatPixels p; c->readToPixels(p);
        near(p.getColor(0,0).a, .3f, precision==8 ? .005f : .001f);
        ofPushStyle(); ofDisableAlphaBlending();
        mapper.getParameter<ofTexture*>("Tex In Front") = c;
        mapper.getParameter<ofTexture*>("Tex In A") = c;
        mapper.getParameter<ofTexture*>("Tex In B") = c;
        mapper.getParameter<std::vector<int>>("Connections") = {1};
        mapper.getParameter<std::vector<int>>("Group Pos") = {1};
        mapper.getParameter<std::vector<float>>("Op. Fr") = {1};
        mapper.getParameter<std::vector<float>>("Op. A") = {1};
        mapper.getParameter<std::vector<float>>("Op. B") = {1};
        mapper.update(args);
        auto* mapped = output(mapper,"Tex Out Front");
        assert(mapped->getTextureData().glInternalFormat == ofxOceanodeShared::getColorTextureInternalFormat());
        mapped->readToPixels(p);
        // Mapping flips Y; find the connected row rather than depending on OF's orientation.
        float brightest=0;
        for(size_t i=0;i<p.size();i+=4) brightest=std::max(brightest,p[i]);
        near(brightest,.2f, precision==8 ? .005f : .001f);
        mapper.getParameter<std::vector<float>>("Op. Fr") = {.5f};
        mapper.update(args); mapped->readToPixels(p); brightest=0;
        for(size_t i=0;i<p.size();i+=4) brightest=std::max(brightest,p[i]);
        near(brightest,.1f,precision==8 ? .005f : .001f);
        // A clean cached frame must produce identical pixels.
        auto before=p;
        mapper.update(args); mapped->readToPixels(p);
        assert(p.size()==before.size());
        for(size_t i=0;i<p.size();++i) near(p[i],before[i]);
        assert(glGetError()==GL_NO_ERROR);
        ofPopStyle();
    }
    // Empty/scalar vectors must remain safe when passed to fixed-size uniforms.
    mapper.getParameter<std::vector<int>>("Tex Inv") = {};
    mapper.getParameter<std::vector<float>>("Op. A") = {};
    mapper.getParameter<std::vector<int>>("Connections") = {999};
    mapper.update(args);
    assert(glGetError()==GL_NO_ERROR);
    // Monochrome sources must have identical RGB semantics in both modes.
    ofTexture mono; mono.allocate(1,1,GL_R32F);
    float monoValue=.45f; mono.loadData(&monoValue,1,1,GL_RED);
    reader.getParameter<ofTexture*>("Input") = &mono;
    reader.draw(args);
    near(reader.getParameter<std::vector<float>>("Green").get()[0],.45f);
    reader.getInspectorParameter<bool>("Async Readback") = true;
    reader.draw(args); glFinish(); reader.draw(args);
    near(reader.getParameter<std::vector<float>>("Green").get()[0],.45f);
    reader.deactivate();
    reader.getParameter<ofTexture*>("Input") = output(vector,"Output Tex");
    // Async queues must discard samples when dimensions or input disappear.
    reader.getInspectorParameter<bool>("Async Readback") = true;
    reader.draw(args);
    vector.getParameter<int>("Width") = 1; vector.draw(args);
    reader.draw(args); glFinish(); reader.draw(args);
    assert(reader.getParameter<std::vector<float>>("Output").get().size()==4); // (1+separator)*2
    reader.getParameter<ofTexture*>("Input") = nullptr; reader.draw(args);
    parent.clearContainer();
    canvas.end();
    std::cout << "PASS retained texture upload, sync/async pixels and latency, precision/alpha, mapping cache and empty inputs\n";
}
'''

if __name__ == '__main__':
    of = of_root()
    textures = of / 'addons/ofxOceanodeTextures'
    lightnet = first_existing(of / 'apps/Playmodes/Lightnet/src', of / 'apps/SIGNES/Lightnet/src')
    native.main(extra_sources=[str(lightnet/'textureRegionMapper.cpp')],
                extra_includes=[textures/'src',textures/'src/shaders',lightnet])
