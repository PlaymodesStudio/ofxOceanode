#!/usr/bin/env python3
"""Real GPU pixels, generic addon texture nodes, persistence/cables/copies (macOS CGL)."""
import macro_mixer_regression as mixer
mixer.PRELUDE += r'''
#include <OpenGL/OpenGL.h>
#include "ofGLProgrammableRenderer.h"
#include "ColorTexture.h"
#include "textureFlip.h"
#include "vectorToTexture.h"
#include "textureInfo.h"
#include "textureReader.h"
#include "simpleEffect.h"
#include "mixerAlpha.h"
class OtherAddonTexture : public ofxOceanodeNodeModel {
    ofParameter<std::vector<ofTexture*>> input;
    ofParameter<ofTexture*> output;
    ofParameter<std::string> label;
public:
    OtherAddonTexture(std::string name="Other Addon Texture"):ofxOceanodeNodeModel(name){}
    void setup() override { addParameter(input.set("Input",{nullptr})); addOutputParameter(output.set("Output",nullptr)); addInspectorParameter(label.set("Label","default")); }
    void draw(ofEventArgs&) override { assert(label.get()=="restored"); output=input.get().empty()?nullptr:input.get().front(); }
};
class OtherAddonColor : public ofxOceanodeNodeModel {
    ofParameter<ofFloatColor> color,output;
public:
    OtherAddonColor():ofxOceanodeNodeModel("Other Addon Color"){}
    void setup() override { addParameter(color.set("Color",ofFloatColor(1))); addOutputParameter(output.set("Output",ofFloatColor(1))); }
    void update(ofEventArgs&) override { output=color.get(); }
};
class ForbiddenTexture : public OtherAddonTexture {
public:
    ForbiddenTexture():OtherAddonTexture("Forbidden Texture"){}
    std::string macroMixerBlocker() const override { return "External service owned by this node"; }
};
'''
mixer.CASES = r'''
static void near(float a,float b) { if(!std::isfinite(a) || !std::isfinite(b) || std::abs(a-b)>.002f) {std::cerr<<a<<" != "<<b<<"\n"; std::abort();} }
// Native container order can give a draw-driven texture chain frame latency.
// Render four ordinary frames before checking steady pixels; production never
// repeats graph execution to settle a frame (which would alter feedback).
static void drawFrames(MacroMixerRuntime& runtime) { for(int frame=0;frame<4;++frame) runtime.draw(); }
static void drawFrames(ofxOceanodeContainer& container) { for(int frame=0;frame<4;++frame) container.draw(); }
static ofTexture* texture(const MacroMixerRuntime& runtime) { return std::get<ofTexture*>(runtime.outputValue("image")); }
static ofFloatColor pixel(ofTexture* texture,int x=0,int y=0) { assert(texture && texture->isAllocated()); ofFloatPixels pixels; texture->readToPixels(pixels); return pixels.getColor(x,y); }
static void rgba(ofTexture* texture,ofFloatColor expected) { const auto v=pixel(texture); for(int i=0;i<4;++i) near(v[i],expected[i]); }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem; ofDisableDataPath();
    CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,kCGLPFAAllowOfflineRenderers,(CGLPixelFormatAttribute)0};
    CGLPixelFormatObj format=nullptr; GLint screens=0; CGLContextObj context=nullptr;
    bool accelerated=CGLChoosePixelFormat(attributes,&format,&screens)==kCGLNoError && format;
    if(!accelerated) {
        CGLPixelFormatAttribute offline[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,kCGLPFAAllowOfflineRenderers,(CGLPixelFormatAttribute)0};
        assert(CGLChoosePixelFormat(offline,&format,&screens)==kCGLNoError && format);
    }
    assert(CGLCreateContext(format,nullptr,&context)==kCGLNoError && context); CGLDestroyPixelFormat(format);
    assert(CGLSetCurrentContext(context)==kCGLNoError);
    glewExperimental=GL_TRUE; assert(glewInit()==GLEW_OK); while(glGetError()!=GL_NO_ERROR) {}
    auto window=std::make_shared<ofAppNoWindow>(); ofWindowSettings settings; settings.setSize(64,64); window->setup(settings);
    ofGetMainLoop()->setCurrentWindow(window);
    auto renderer=std::make_shared<ofGLProgrammableRenderer>(window.get()); window->renderer()=renderer; renderer->setup(4,1); renderer->startRender();
    std::cout<<"GL: "<<glGetString(GL_VERSION)<<" / "<<glGetString(GL_RENDERER)<<std::endl;
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<router<ofTexture*>>("Router","Texture",nullptr);
    registry->registerModel<router<int>>("Router","i",0,0,4096);
    registry->registerModel<router<std::vector<float>>>("Router","v_f",0.f,0.f,1.f);
    registry->registerModel<ColorTexture>("Textures"); registry->registerModel<textureFlip>("Textures");
    registry->registerModel<textureInfo>("Textures"); registry->registerModel<textureReader>("Textures");
    registry->registerModel<OtherAddonColor>("Another addon");
    types->registerType<ofFloatColor>("Color");
    registry->registerModel<ForbiddenTexture>("Another addon");
    registry->registerModel<vectorToTexture>("Textures"); registry->registerModel<OtherAddonTexture>("Another addon");
    registry->registerModel<ofxOceanodeMacroMixer>("Container"); registry->registerModel<ofxOceanodeNodeMacro>("Container");
    types->registerType<ofTexture*>("Texture"); types->registerType<int>("i"); types->registerType<std::vector<float>>("v_f");
    // The real host registers in both its container and the ordinary global adapter registry.
    ofxOceanodeTypesRegistry::getInstance()->registerType<ofTexture*>("Texture");
    ofxOceanodeTypesRegistry::getInstance()->registerType<int>("i");
    ofxOceanodeTypesRegistry::getInstance()->registerType<std::vector<float>>("v_f");
    auto transport=std::make_shared<ofxOceanodeTransport>(); transport->setDriverMode(TransportDriverMode::FrameStep);
    const auto before=transport->getState();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("TestParent");
    const std::string scratch=argv[2]; const auto cwd=fs::current_path(); fs::current_path(scratch);
    auto write=[](const std::string& folder,const std::string& file,const ofJson& json){std::ofstream(folder+"/"+file)<<json.dump(2);};
    MacroMixerContract contract; contract.routers={{"image","Image","Texture"},{"height","Height","Integer"}};
    auto source=[&](const std::string& folder,bool second) {
        fs::create_directories(folder+"/assets"); // Generic node assets are references, never embedded.
        write(folder,"modules.json",{{"Other Addon Color",{{"01",{0,0}}}},{"Color Texture",{{"01",{0,0}}}},{"Texture Flip",{{"01",{0,0}}}},
            {"Other Addon Texture",{{"01",{0,0}}}},{"Texture Info",{{"01",{0,0}}}},
            {"Router Texture",{{"01",{0,0}}}},{"Router i",{{"01",{0,0}}}}});
        write(folder,"Other_Addon_Color_1.json",{{"Color",second?"0, 0, 1, 0.75":"1, 0, 0, 0.25"}});
        write(folder,"Color_Texture_1.json",{{"Res_X",second?16:32},{"Res_Y",second?32:16},{"Color",second?"0, 0, 1, 0.75":"1, 0, 0, 0.25"}});
        write(folder,"Texture_Flip_1.json",{{"H",false},{"V",false}});
        write(folder,"Other_Addon_Texture_1.json",{{"Label","restored"}});
        write(folder,"Texture_Info_1.json",ofJson::object());
        write(folder,"Router_Texture_1.json",{{"Name","Image"}});
        write(folder,"Router_i_1.json",{{"Name","Height"},{"Value",0},{"Min",0},{"Max",4096}});
        write(folder,"connections.json",{
            {"Other_Addon_Color_1",{{"Output",{{"Color_Texture_1",{{"Color",nullptr}}}}}}},
            {"Color_Texture_1",{{"Output",{{"Texture_Flip_1",{{"Input",nullptr}}}}}}},
            {"Texture_Flip_1",{{"Output",{{"Other_Addon_Texture_1",{{"Input",nullptr}}}}}}},
            {"Other_Addon_Texture_1",{{"Output",{{"Router_Texture_1",{{"Value",nullptr}}},{"Texture_Info_1",{{"Input",nullptr}}}}}}},
            {"Texture_Info_1",{{"Height",{{"Router_i_1",{{"Value",nullptr}}}}}}}});
    };
    const auto a=scratch+"/a",b=scratch+"/b"; source(a,false); source(b,true);
    {
        // Graph morphs bypass saved texture/number policies and render evolving
        // internal controls, preserving a private texture across update/draw.
        auto c=contract; c.routers[0].policy="fade_out"; c.routers[1].policy="switch"; c.routers[1].options={{"threshold",.9}};
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(c)&&runtime.importMacro(a)&&runtime.importMacro(b));
        runtime.request(a,0,0,true); runtime.update(0); drawFrames(runtime); rgba(texture(runtime),{1,0,0,.25});
        runtime.request(b,100,0,true); runtime.update(0); drawFrames(runtime); rgba(texture(runtime),{1,0,0,.25});
        runtime.update(50); drawFrames(runtime); rgba(texture(runtime),{.5,0,.5,.5});
        assert(texture(runtime)->getWidth()==24&&texture(runtime)->getHeight()==24);
        assert(std::get<int>(runtime.outputValue("height"))==24);
        assert(texture(runtime)!=std::get<ofTexture*>(runtime.destination()->read("image")));
        auto saved=runtime.saveState(); MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); drawFrames(copy);
        rgba(texture(copy),{0,0,1,.75}); assert(texture(copy)!=texture(runtime));
        const auto held=texture(runtime); runtime.finish(); runtime.update(60); assert(texture(runtime)==held);
        rgba(held,{.5,0,.5,.5}); drawFrames(runtime); rgba(texture(runtime),{0,0,1,.75});
        assert(texture(runtime)->getWidth()==16&&texture(runtime)->getHeight()==32);
        runtime.request(a,100,0,false); runtime.update(70); runtime.update(120); drawFrames(runtime); rgba(texture(runtime),{0,0,0,0});
    }
    {
        auto& mixing=MacroMixerMixing::registry(); assert(mixing.policy("Texture").id=="crossfade");
        ofFloatPixels ap,bp; ap.allocate(16,8,4); bp.allocate(32,16,4);
        const ofFloatColor ca(.2,.4,.6,.3),cb(.8,.6,.4,.9); ap.setColor(ca); bp.setColor(cb);
        ofTexture ta,tb; ta.allocate(ap,true); ta.loadData(ap); tb.allocate(bp,false); tb.loadData(bp);
        const auto scaled=[](ofFloatColor c,float gain) { for(int i=0;i<4;++i) c[i]*=gain; return c; };
        const auto reference=[&](const std::string& id,float t,float strength=1.f) {
            if(id=="fade_out") return t<.5 ? scaled(ca,1-2*t) : scaled(cb,2*t-1);
            ofFloatColor result; for(int c=0;c<4;++c) result[c]=ca[c]+(cb[c]-ca[c])*t;
            for(int c=0;c<3;++c) {
                const float v=id=="screen" ? 1-(1-ca[c])*(1-cb[c]) : id=="add" ? ca[c]+cb[c]
                    : id=="multiply" ? ca[c]*cb[c] : std::abs(ca[c]-cb[c]);
                result[c]+=(v-result[c])*4*t*(1-t)*strength;
            }
            return result;
        };
        for(const auto& id:std::vector<std::string>{"screen","add","multiply","difference","wipe","noise_dissolve","fade_out"}) {
            const auto& p=mixing.policy("Texture",id); const auto options=p.options(ofJson::object()); auto render=p.createRenderer();
            auto out=std::get<ofTexture*>(render(&ta,&tb,0,options)); rgba(out,ca); assert(out->getWidth()==16 && out->getHeight()==8);
            out=std::get<ofTexture*>(render(&ta,&tb,1,options)); rgba(out,cb); assert(out->getWidth()==32 && out->getHeight()==16);
            if(id!="wipe" && id!="noise_dissolve") for(float t:{.25f,.5f,.75f}) {
                out=std::get<ofTexture*>(render(&ta,&tb,t,options)); rgba(out,reference(id,t)); assert(out->getWidth()==32 && out->getHeight()==16);
                if(id!="fade_out") {
                    rgba(std::get<ofTexture*>(render(&ta,&tb,t,p.options({{"strength",0}}))),{ca.r+(cb.r-ca.r)*t,ca.g+(cb.g-ca.g)*t,ca.b+(cb.b-ca.b)*t,ca.a+(cb.a-ca.a)*t});
                    rgba(std::get<ofTexture*>(render(&ta,&tb,t,p.options({{"strength",.4}}))),reference(id,t,.4));
                }
            }
            auto null=static_cast<ofTexture*>(nullptr);
            assert(!std::get<ofTexture*>(render(null,null,.5,options)));
            assert(!std::get<ofTexture*>(render(null,&tb,0,options)));
            assert(!std::get<ofTexture*>(render(&ta,null,1,options)));
            if(id=="fade_out") { rgba(std::get<ofTexture*>(render(null,&tb,.75,options)),scaled(cb,.5)); rgba(std::get<ofTexture*>(render(&ta,null,.25,options)),scaled(ca,.5)); }
            // Every policy/options pair restores as a fresh destination, with private output resources.
            auto c=contract; c.routers[0].policy=id; c.routers[0].options=id=="wipe" ? ofJson{{"angle",180},{"softness",.2}}
                : id=="noise_dissolve" ? ofJson{{"cells",13},{"seed",17},{"softness",.1}}
                : id=="fade_out" ? ofJson::object() : ofJson{{"strength",.75}};
            MacroMixerRuntime runtime(parent); assert(runtime.setContract(c)); assert(runtime.importMacro(a) && runtime.importMacro(b)); auto entries=runtime.macros();
            runtime.requestMacro(entries[0]->id(),0,0); runtime.update(0); drawFrames(runtime); rgba(texture(runtime),{1,0,0,.25});
            runtime.requestMacro(entries[1]->id(),100,0); runtime.update(0); runtime.update(50); drawFrames(runtime);
            if(id=="fade_out") rgba(texture(runtime),{0,0,0,0});
            const auto saved=runtime.saveState(); MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); drawFrames(copy); rgba(texture(copy),{0,0,1,.75});
            assert(copy.contract().routers[0].policy==id && copy.contract().routers[0].options==runtime.contract().routers[0].options);
            assert(texture(copy)!=texture(runtime) && texture(copy)->getTextureData().textureID!=texture(runtime)->getTextureData().textureID);
            assert(copy.origin()->container!=runtime.destination()->container);
            runtime.finish(); runtime.update(100); drawFrames(runtime); rgba(texture(runtime),{0,0,1,.75});
        }
        auto wipe=mixing.policy("Texture","wipe").createRenderer();
        const auto checkPixel=[&](ofTexture* image,int x,int y,ofFloatColor expected) { auto actual=pixel(image,x,y); for(int c=0;c<4;++c) near(actual[c],expected[c]); };
        for(int angle:{0,90,180,270}) {
            auto image=std::get<ofTexture*>(wipe(&ta,&tb,.5,mixing.policy("Texture","wipe").options({{"angle",angle},{"softness",0}})));
            if(angle==0 || angle==180) { checkPixel(image,2,8,angle==0?cb:ca); checkPixel(image,29,8,angle==0?ca:cb); }
            else { checkPixel(image,16,1,angle==90?cb:ca); checkPixel(image,16,14,angle==90?ca:cb); }
        }
        auto softImage=std::get<ofTexture*>(wipe(&ta,&tb,.5,mixing.policy("Texture","wipe").options({{"softness",.5}})));
        auto edge=pixel(softImage,15,8); assert(edge.r>ca.r+.1 && edge.r<cb.r-.1); // Soft edge has an actual mixture.
        auto noise=mixing.policy("Texture","noise_dissolve").createRenderer();
        auto noiseOptions=mixing.policy("Texture","noise_dissolve").options({{"cells",8},{"seed",42},{"softness",0}});
        std::vector<std::vector<float>> masks;
        for(float t:{.25f,.5f,.75f,.5f}) {
            auto image=std::get<ofTexture*>(noise(&ta,&tb,t,noiseOptions)); ofFloatPixels pixels; image->readToPixels(pixels);
            // Some OF archives report float buffer size in bytes. Limit comparisons
            // to actual image components, excluding unused readback allocation.
            const auto components=size_t(image->getWidth())*size_t(image->getHeight())*4;
            masks.emplace_back(pixels.getData(),pixels.getData()+components);
        }
        int revealed=0;
        for(size_t i=0;i<masks[0].size();i+=4) {
            const bool early=masks[0][i]>.5,mid=masks[1][i]>.5,late=masks[2][i]>.5;
            assert(!early || mid); assert(!mid || late); revealed+=mid;
            assert(mid==(masks[3][i]>.5));
            for(const auto& mask:masks) for(int c=0;c<4;++c) near(mask[i+c],mask[i]>.5 ? cb[c] : ca[c]);
            for(int c=0;c<4;++c) near(masks[1][i+c],masks[3][i+c]);
        }
        assert(revealed>150 && revealed<360); // Roughly half the 512 pixels, with stable cells.
        noiseOptions["seed"]=43; auto image=std::get<ofTexture*>(noise(&ta,&tb,.5,noiseOptions)); ofFloatPixels changed; image->readToPixels(changed);
        int changedCells=0; for(size_t i=0;i<masks[1].size();i+=4) changedCells+=(masks[1][i]>.5)!=(changed[i]>.5);
        assert(changedCells>100);
        // Add preserves values above 1 in the float render target.
        bp.setColor({1.2,.6,.4,.9}); tb.loadData(bp); auto add=mixing.policy("Texture","add").createRenderer();
        rgba(std::get<ofTexture*>(add(&ta,&tb,.5,mixing.policy("Texture","add").options(ofJson::object()))),{1.4,1,1,.6});
        for(const auto& bad:std::vector<std::pair<std::string,ofJson>>{
            {"screen",{{"strength",-1}}},{"add",{{"strength",2}}},{"multiply",{{"bogus",1}}},{"difference",{{"strength","full"}}},
            {"wipe",{{"angle",361}}},{"wipe",{{"softness",-1}}},{"noise_dissolve",{{"cells",0}}},{"noise_dissolve",{{"cells",1.5}}},
            {"noise_dissolve",{{"seed",65536}}},{"noise_dissolve",{{"softness",2}}},{"fade_out",{{"strength",1}}}}) {
            bool rejected=false; try { mixing.policy("Texture",bad.first).options(bad.second); } catch(const std::exception&) { rejected=true; } assert(rejected);
        }
    }
    {
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(contract));
        assert(runtime.importMacro(a)&&runtime.importMacro(b)); auto entries=runtime.macros();
        runtime.requestMacro(entries[0]->id(),0,0); runtime.update(0); drawFrames(runtime);
        if(!runtime.diagnostics().empty()) std::cerr<<runtime.status()<<"\n";
        rgba(texture(runtime),{1,0,0,.25}); assert(texture(runtime)->getWidth()==32 && texture(runtime)->getHeight()==16);
        assert(std::get<int>(runtime.outputValue("height"))==16);
        runtime.requestMacro(entries[1]->id(),100,0); runtime.update(0); drawFrames(runtime); rgba(texture(runtime),{1,0,0,.25});
        runtime.update(50);
        glEnable(GL_SCISSOR_TEST); glScissor(0,0,0,0); glEnable(GL_DEPTH_TEST); glColorMask(false,false,false,false);
        drawFrames(runtime); assert(glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_DEPTH_TEST)); GLboolean mask[4]; glGetBooleanv(GL_COLOR_WRITEMASK,mask); assert(!mask[0]);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glColorMask(true,true,true,true);
        rgba(texture(runtime),{.5,0,.5,.5}); assert(texture(runtime)->getWidth()==32 && texture(runtime)->getHeight()==32);
        assert(std::get<int>(runtime.outputValue("height"))==24);
        auto saved=runtime.saveState(); assert(saved.dump().find("0xBAD")==std::string::npos && !saved.contains("files"));
        MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); drawFrames(copy); rgba(texture(copy),{0,0,1,.75});
        assert(texture(copy)!=texture(runtime) && texture(copy)->getTextureData().textureID!=texture(runtime)->getTextureData().textureID);
        assert(copy.origin()->container!=runtime.destination()->container);
        const auto held=texture(runtime); runtime.finish(); runtime.update(100); assert(texture(runtime)==held); // No dangling graph pointer between update/draw.
        drawFrames(runtime); rgba(texture(runtime),{0,0,1,.75}); assert(texture(runtime)->getWidth()==16 && texture(runtime)->getHeight()==32);
        fs::rename(b,b+"-moved"); auto capture=runtime.macro(entries[1]->id()); assert(!runtime.importMacro(b,true) && runtime.macro(entries[1]->id())==capture);
        runtime.update(110); drawFrames(runtime); rgba(texture(runtime),{0,0,1,.75});
        MacroMixerRuntime unavailable(parent); assert(unavailable.restoreState(saved)); unavailable.update(0); drawFrames(unavailable); assert(!unavailable.origin() && !texture(unavailable));
        fs::rename(b+"-moved",b); unavailable.requestMacro(entries[1]->id(),0,0); unavailable.update(0); drawFrames(unavailable); rgba(texture(unavailable),{0,0,1,.75});
        runtime.clear(); assert(!texture(runtime));
    }
    {
        // Real source graph from another addon: texture input + live numeric vector,
        // a GL rectangle upload, and texture-to-numeric output in the same draw.
        const auto folder=scratch+"/vector"; fs::create_directory(folder);
        write(folder,"modules.json",{{"Vector to Texture",{{"01",{0,0}}}},{"Texture Reader",{{"01",{0,0}}}},
            {"Router Texture",{{"01",{0,0}}}},{"Router v_f",{{"01",{0,0}},{"02",{0,0}}}}});
        write(folder,"Vector_to_Texture_1.json",{{"Width",2},{"Height",2},{"Input",{.1}}});
        write(folder,"Texture_Reader_1.json",{{"Add_Separator",false}});
        write(folder,"Router_Texture_1.json",{{"Name","Image"}});
        write(folder,"Router_v_f_1.json",{{"Name","Values"},{"Value",{0}},{"Min",0},{"Max",1}});
        write(folder,"Router_v_f_2.json",{{"Name","Read"},{"Value",{0}},{"Min",0},{"Max",1}});
        write(folder,"connections.json",{{"Router_v_f_1",{{"Value",{{"Vector_to_Texture_1",{{"Input",nullptr}}}}}}},
            {"Vector_to_Texture_1",{{"Output_Tex",{{"Router_Texture_1",{{"Value",nullptr}}},{"Texture_Reader_1",{{"Input",nullptr}}}}}}},
            {"Texture_Reader_1",{{"Output",{{"Router_v_f_2",{{"Value",nullptr}}}}}}}});
        MacroMixerContract c; c.routers={{"image","Image","Texture"},{"values","Values","Float Vector","",ofJson::object(),"Input"},{"read","Read","Float Vector"}};
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(c)); assert(runtime.setInputs({{"values",std::vector<float>{.2,.4,.6,.8}}}));
        assert(runtime.importMacro(folder)); runtime.requestMacro(runtime.macros()[0]->id(),0,0); runtime.update(0); drawFrames(runtime);
        auto values=std::get<std::vector<float>>(runtime.outputValue("read")); assert(values.size()==4); near(values[0],.2); near(values[3],.8);
        assert(runtime.setInputs({{"values",std::vector<float>{.7}}})); runtime.update(1); drawFrames(runtime);
        values=std::get<std::vector<float>>(runtime.outputValue("read")); near(values[0],.7); near(values[3],0);
    }
    {
        // A real shader effect is admitted through its ordinary registry entry,
        // with dynamic effect parameters and a file-backed shader source.
        const auto path=scratch+"/effect.glsl";
        std::ofstream(path)<<R"(// gain:0.5:0:1
#version 410
uniform sampler2D tSource;
uniform float gain;
uniform sampler2D gainTex;
uniform int gainTexConnected;
out vec4 out_color;
void main(){ vec4 c=texelFetch(tSource,ivec2(gl_FragCoord.xy),0); float g=gainTexConnected!=0 ? texelFetch(gainTex,ivec2(0),0).r : gain; out_color=vec4(c.rgb*g,c.a); }
)";
        registry->registerModel<simpleEffect>("Effects","Unlisted Shader Effect",std::string("gain:0.5:0:1"),path);
        const auto folder=scratch+"/effect"; fs::create_directory(folder);
        write(folder,"modules.json",{{"Color Texture",{{"01",{0,0}}}},{"Unlisted Shader Effect",{{"01",{0,0}}}},
            {"Router Texture",{{"01",{0,0}}}}});
        write(folder,"Color_Texture_1.json",{{"Res_X",4},{"Res_Y",4},{"Color","1, 0.5, 0.25, 0.5"}});
        write(folder,"Unlisted_Shader_Effect_1.json",{{"gain",.4},{"Draw_On_Event",false}});
        write(folder,"Router_Texture_1.json",{{"Name","Image"}});
        write(folder,"connections.json",{{"Color_Texture_1",{{"Output",{{"Unlisted_Shader_Effect_1",{{"Input",nullptr}}}}}}},
            {"Unlisted_Shader_Effect_1",{{"Output",{{"Router_Texture_1",{{"Value",nullptr}}}}}}}});
        MacroMixerContract c; c.routers={{"image","Image","Texture"}};
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(c)); assert(runtime.importMacro(folder));
        runtime.requestMacro(runtime.macros()[0]->id(),0,0); runtime.update(0); drawFrames(runtime);
        rgba(texture(runtime),{.4,.2,.1,.5});
        // Ordinary custom receiver: Texture -> numeric shader uniform's texture input.
        write(folder,"connections.json",{{"Color_Texture_1",{{"Output",{{"Unlisted_Shader_Effect_1",{{"Input",nullptr},{"gain",nullptr}}}}}}},
            {"Unlisted_Shader_Effect_1",{{"Output",{{"Router_Texture_1",{{"Value",nullptr}}}}}}}});
        assert(runtime.importMacro(folder,true)); runtime.requestMacro(runtime.macros()[0]->id(),0,0); runtime.update(1); drawFrames(runtime);
        rgba(texture(runtime),{1,.5,.25,.5});
    }
    {
        // Shared texture input values are live pointers; saved values must be null.
        const auto folder=scratch+"/input"; fs::create_directory(folder);
        write(folder,"modules.json",{{"Router Texture",{{"01",{0,0}},{"02",{0,0}}}}});
        write(folder,"Router_Texture_1.json",{{"Name","Input"}});
        write(folder,"Router_Texture_2.json",{{"Name","Image"}});
        write(folder,"connections.json",{{"Router_Texture_1",{{"Value",{{"Router_Texture_2",{{"Value",nullptr}}}}}}}});
        MacroMixerContract c; c.routers={{"input","Input","Texture","",ofJson::object(),"Input"},{"image","Image","Texture"}};
        ofFloatPixels pixels; pixels.allocate(2,2,4); pixels.setColor({.2,.4,.6,.3});
        ofTexture image; image.allocate(pixels,false); image.loadData(pixels);
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(c)); assert(runtime.setInputs({{"input",&image}}));
        assert(runtime.importMacro(folder)); runtime.requestMacro(runtime.macros()[0]->id(),0,0); runtime.update(0); drawFrames(runtime); rgba(texture(runtime),{.2,.4,.6,.3});
        pixels.setColor(0,0,{1,0,0,.1}); pixels.setColor(1,0,{0,1,0,.2});
        pixels.setColor(0,1,{0,0,1,.3}); pixels.setColor(1,1,{1,1,0,.4}); image.loadData(pixels);
        drawFrames(runtime); for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
            const auto actual=pixel(texture(runtime),x,y), expected=pixels.getColor(x,y);
            for(int i=0;i<4;++i) near(actual[i],expected[i]);
        }
        // The mixer image must also match ordinary OF drawing for flipped sources.
        image.getTextureData().bFlipTexture=true;
        ofFbo reference; ofFbo::Settings rs; rs.width=2; rs.height=2; rs.internalformat=GL_RGBA32F; rs.textureTarget=GL_TEXTURE_2D; rs.useDepth=false; rs.useStencil=false; reference.allocate(rs);
        MacroMixerMixing::drawIsolated([&]{ reference.begin(); image.draw(0,0,2,2); reference.end(); });
        drawFrames(runtime); for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
            const auto actual=pixel(texture(runtime),x,y), expected=pixel(&reference.getTexture(),x,y);
            for(int i=0;i<4;++i) near(actual[i],expected[i]);
        }
        image.getTextureData().bFlipTexture=false;
        pixels.setColor({.2,.4,.6,.3}); image.loadData(pixels);
        auto saved=runtime.saveState(); assert(saved["inputs"]["input"].is_null());
        MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); drawFrames(copy); assert(!texture(copy));
        assert(runtime.setInputs({{"input",static_cast<ofTexture*>(nullptr)}})); runtime.update(1); drawFrames(runtime); assert(!texture(runtime));
        // Mix both GL texture targets and exact RGBA channels including alpha.
        ofTexture rect; rect.allocate(pixels,true); rect.loadData(pixels);
        pixels.setColor({.8,.6,.4,.9}); image.loadData(pixels);
        auto mix=MacroMixerMixing::registry().policy("Texture","crossfade").createRenderer();
        rgba(std::get<ofTexture*>(mix(&rect,&image,.5,ofJson::object())),{.5,.5,.5,.6});
        rgba(std::get<ofTexture*>(mix(static_cast<ofTexture*>(nullptr),&image,.5,ofJson::object())),{.4,.3,.2,.45});
        assert(!std::get<ofTexture*>(mix(static_cast<ofTexture*>(nullptr),static_cast<ofTexture*>(nullptr),.5,ofJson::object())));
        auto switching=MacroMixerMixing::registry().policy("Texture","switch").createRenderer();
        rgba(std::get<ofTexture*>(switching(&rect,&image,.49,{{"threshold",.5}})),{.2,.4,.6,.3});
        rgba(std::get<ofTexture*>(switching(&rect,&image,.5,{{"threshold",.5}})),{.8,.6,.4,.9});
        rgba(std::get<ofTexture*>(switching(&rect,&image,0,{{"threshold",0}})),{.2,.4,.6,.3});
        rgba(std::get<ofTexture*>(switching(&rect,&image,1,{{"threshold",1}})),{.8,.6,.4,.9});
        // Event-driven effects must receive a fresh notification even when a
        // live input keeps the same texture pointer and changes only its pixels.
        write(folder,"modules.json",{{"Router Texture",{{"01",{0,0}},{"02",{0,0}}}},
            {"Unlisted Shader Effect",{{"01",{0,0}}}}});
        write(folder,"Unlisted_Shader_Effect_1.json",{{"gain",.5},{"Draw_On_Event",true}});
        write(folder,"connections.json",{{"Router_Texture_1",{{"Value",{{"Unlisted_Shader_Effect_1",{{"Input",nullptr}}}}}}},
            {"Unlisted_Shader_Effect_1",{{"Output",{{"Router_Texture_2",{{"Value",nullptr}}}}}}}});
        MacroMixerRuntime eventDriven(parent); assert(eventDriven.setContract(c)); assert(eventDriven.setInputs({{"input",&image}}));
        assert(eventDriven.importMacro(folder)); eventDriven.requestMacro(eventDriven.macros()[0]->id(),0,0); eventDriven.update(0); drawFrames(eventDriven);
        rgba(texture(eventDriven),{.4,.3,.2,.9});
        pixels.setColor({.4,.8,.2,.6}); image.loadData(pixels); eventDriven.update(1); drawFrames(eventDriven);
        rgba(texture(eventDriven),{.2,.4,.1,.6});
        // A resource veto is part of the model itself, not another registry.
        const auto bad=scratch+"/forbidden"; fs::create_directory(bad);
        write(bad,"modules.json",{{"Forbidden Texture",{{"01",{0,0}}}},{"Router Texture",{{"01",{0,0}},{"02",{0,0}}}}});
        write(bad,"Forbidden_Texture_1.json",ofJson::object());
        write(bad,"Router_Texture_1.json",{{"Name","Input"}}); write(bad,"Router_Texture_2.json",{{"Name","Image"}});
        write(bad,"connections.json",ofJson::object()); assert(!runtime.importMacro(bad));

    }
    {
        // Real external texture cables restored before playback and independent clipboard buffers.
        auto& node=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1);
        auto& mix=static_cast<ofxOceanodeMacroMixer&>(node.getNodeModel()); assert(mix.setRouterContract(contract));
        auto& sink=parent.createNode(registry->create("Router Texture"),9);
        assert(parent.createConnectionFromInfo("MacroMixer_1","Image","Router_Texture_9","Value"));
        assert(mix.importMacroFolder(a)); mix.getParameter<float>("Duration Ms")=0; mix.getParameter<void>("Transition").trigger(); parent.update(); drawFrames(parent);
        rgba(sink.getNodeModel().getParameter<ofTexture*>("Value"),{1,0,0,.25});
        const auto folder=scratch+"/saved-node"; fs::create_directory(folder); node.savePreset(folder);
        parent.saveClipboardModulesAndConnections({&node,&sink},{0,0}); assert(parent.loadClipboardModulesAndConnections({40,40},false)); parent.update(); drawFrames(parent);
        auto& duplicate=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(2)->getNodeModel());
        rgba(duplicate.getParameter<ofTexture*>("Image"),{1,0,0,.25}); assert(duplicate.getParameter<ofTexture*>("Image")!=mix.getParameter<ofTexture*>("Image"));
        duplicate.deactivate(); drawFrames(parent); rgba(mix.getParameter<ofTexture*>("Image"),{1,0,0,.25});
        parent.clearContainer(); auto& loaded=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); loaded.loadPresetBeforeConnections(folder);
        auto& restoredSink=parent.createNode(registry->create("Router Texture"),9);
        assert(parent.createConnectionFromInfo("MacroMixer_1","Image","Router_Texture_9","Value")); assert(loaded.loadPreset(folder)); parent.update(); drawFrames(parent);
        rgba(restoredSink.getNodeModel().getParameter<ofTexture*>("Value"),{1,0,0,.25}); parent.clearContainer();
    }
    {
        // Production Macro embedding, reload and independent copied GPU containers.
        auto& outer=parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1);
        auto& m=static_cast<ofxOceanodeNodeMacro&>(outer.getNodeModel()); auto child=m.getContainer();
        auto& mixNode=child->createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); auto& mix=static_cast<ofxOceanodeMacroMixer&>(mixNode.getNodeModel());
        assert(mix.setRouterContract(contract));
        auto& route=child->createNode(registry->create("Router Texture"),1).getNodeModel(); route.getParameter<std::string>("Name")="MixedImage";
        assert(child->createConnectionFromInfo("MacroMixer_1","Image","Router_Texture_1","Value"));
        assert(mix.importMacroFolder(a)); mix.getParameter<float>("Duration Ms")=0; mix.getParameter<void>("Transition").trigger();
        parent.update(); drawFrames(parent); rgba(mix.getParameter<ofTexture*>("Image"),{1,0,0,.25}); rgba(m.getParameter<ofTexture*>("MixedImage"),{1,0,0,.25});
        const auto folder=scratch+"/macro"; parent.savePreset(folder); child.reset(); parent.clearContainer(); assert(parent.loadPreset(folder)); parent.update(); drawFrames(parent);
        auto inner=[](ofxOceanodeNodeModel& model)->ofxOceanodeMacroMixer& {
            auto& m=static_cast<ofxOceanodeNodeMacro&>(model); return static_cast<ofxOceanodeMacroMixer&>(m.getContainer()->getDynamicNodes().at("MacroMixer").at(1)->getNodeModel());
        };
        auto* original=parent.getDynamicNodes().at("Macro").at(1).get(); rgba(inner(original->getNodeModel()).getParameter<ofTexture*>("Image"),{1,0,0,.25});
        parent.saveClipboardModulesAndConnections({original},{0,0}); assert(parent.loadClipboardModulesAndConnections({40,40},false)); parent.update(); drawFrames(parent);
        auto& copy=inner(parent.getDynamicNodes().at("Macro").at(2)->getNodeModel()); auto& first=inner(original->getNodeModel());
        assert(copy.getParameter<ofTexture*>("Image")!=first.getParameter<ofTexture*>("Image"));
        assert(copy.mixerRuntime()->origin()->container!=first.mixerRuntime()->origin()->container); parent.clearContainer();
    }
    // Changed internal texture cables use the same private GPU policy renderers
    // as output routers, while retaining the common native graph. Exercise a
    // direct output sink and a processing node downstream of the changed cable.
    registry->registerModel<mixerAlpha>("Textures");
    if(!accelerated) std::cout<<"Hardware GL unavailable: Mixer Alpha uses its native single-layer path; Apple's software renderer aborts in the existing two-layer composite shader. Internal connection policy shaders still execute and their pixels are checked."<<std::endl;
    for(bool downstream:{false,true}) {
        auto saveCableSource=[&](const std::string& folder,bool target) {
            ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("TextureCable");
            auto& red=c.createNode(registry->create("Color Texture"),1).getNodeModel();
            red.getParameter<int>("Res X")=16; red.getParameter<int>("Res Y")=16; red.getParameter<ofFloatColor>("Color")=ofFloatColor(1,0,0,1);
            auto& output=c.createNode(registry->create("Router Texture"),1).getNodeModel(); output.getParameter<std::string>("Name")="Image";
            auto& number=c.createNode(registry->create("Router i"),1).getNodeModel(); number.getParameter<std::string>("Name")="Height";
            if(downstream) c.createNode(registry->create("Texture Flip"),1);
            if(target) {
                auto& blue=c.createNode(registry->create("Color Texture"),2).getNodeModel();
                blue.getParameter<int>("Res X")=16; blue.getParameter<int>("Res Y")=16; blue.getParameter<ofFloatColor>("Color")=ofFloatColor(0,0,1,1);
                auto& mixer=c.createNode(registry->create("Mixer Alpha"),1).getNodeModel();
                mixer.getParameter<int>("Width")=16; mixer.getParameter<int>("Height")=16;
                if(!accelerated) mixer.getParameter<float>("Opac 2")=0;
                assert(c.createConnectionFromInfo("Color_Texture_1","Output","Mixer_Alpha_1","In 1"));
                assert(c.createConnectionFromInfo("Color_Texture_2","Output","Mixer_Alpha_1","In 0"));
            }
            assert(c.createConnectionFromInfo(target?"Mixer_Alpha_1":"Color_Texture_1","Output",downstream?"Texture_Flip_1":"Router_Texture_1",downstream?"Input":"Value"));
            if(downstream) assert(c.createConnectionFromInfo("Texture_Flip_1","Output","Router_Texture_1","Value"));
            assert(c.savePreset(folder));
        };
        const auto from=scratch+(downstream?"/texture-consumer-a":"/texture-cable-a"),to=scratch+(downstream?"/texture-consumer-b":"/texture-cable-b");
        saveCableSource(from,false); saveCableSource(to,true);
        MacroMixerRuntime r(parent); assert(r.setContract(contract)&&r.importMacro(from)&&r.importMacro(to));
        const auto targetID=r.importedMacro(to)->id();
        r.request(from,0,0,true); r.update(0); drawFrames(r); rgba(texture(r),{1,0,0,1}); const auto* graph=r.origin();
        auto* common=graph->container->getDynamicNodes().at("Color Texture").at(1).get();
        const auto report=r.sourceDetails(targetID);
        assert(std::none_of(report.begin(),report.end(),[](const auto& d){return d.kind=="fallback";}));
        assert(std::any_of(report.begin(),report.end(),[&](const auto& d){return d.kind=="mix"&&d.type=="Texture"&&d.policy=="crossfade"&&d.location.find(downstream?"Texture_Flip_1":"Router_Texture_1")!=std::string::npos;}));
        r.request(to,1000,0,true); r.update(10); assert(r.morphingInPlace()&&r.destination()==graph); drawFrames(r); rgba(texture(r),{1,0,0,1});
        assert(!r.setMorphPolicy("Texture","switch")); r.update(510); drawFrames(r); rgba(texture(r),{.5,0,.5,1});
        auto& blue=graph->container->getDynamicNodes().at("Color Texture").at(2)->getNodeModel().getParameter<ofFloatColor>("Color");
        blue=ofFloatColor(0,1,0,1); drawFrames(r); rgba(texture(r),{.5,.5,0,1}); blue=ofFloatColor(0,0,1,1); drawFrames(r);
        auto* owned=texture(r); const auto held=pixel(owned); r.update(600); rgba(owned,held); // update never changes the owned GPU frame.
        r.finish(); r.update(610); drawFrames(r); rgba(texture(r),{0,0,1,1}); assert(r.origin()==graph);
        r.request(from,1000,0,true); r.update(620); r.update(1120); drawFrames(r); rgba(texture(r),{.5,0,.5,1});
        r.finish(); r.update(1130); drawFrames(r); rgba(texture(r),{1,0,0,1});
        assert(graph->container->getDynamicNodes().at("Color Texture").at(1).get()==common);
        assert(!graph->container->getDynamicNodes().at("Mixer Alpha").count(1));
        assert(r.setMorphPolicy("Texture","switch",{{"threshold",.8}}));
        assert(!r.setMorphPolicy("Texture","switch",{{"threshold",2}}));
        r.request(to,1000,0,true); r.update(1200); r.update(1700); drawFrames(r); rgba(texture(r),{1,0,0,1});
        r.update(2100); drawFrames(r); rgba(texture(r),{0,0,1,1});
        const auto saved=r.saveState(); assert(saved.at("morphPolicies").at("Texture").at("policy")=="switch");
        MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); drawFrames(copy); rgba(texture(copy),{0,0,1,1});
        assert(copy.morphPolicies().at("Texture").options.at("threshold")==.8);
        r.finish(); r.update(2200); drawFrames(r);
        // Cancel after rendering a real temporary GPU surface; original native
        // cables are restored before that surface and the staged nodes retire.
        std::vector<MacroMixerDiagnostic> errors;
        auto revision=MacroMixerSource::importFolder(parent,from,contract,errors); auto original=MacroMixerSource::instantiate(parent,revision,errors);
        auto destination=MacroMixerSource::importFolder(parent,to,contract,errors); assert(original&&destination);
        assert(original->prepareMorphInPlace(destination)); original->applyMorph(.5f);
        for(int frame=0;frame<4;++frame) assert(original->draw());
        rgba(std::get<ofTexture*>(original->read("image")),{.5,0,.5,1}); original->cancelMorph();
        for(int frame=0;frame<4;++frame) assert(original->draw());
        rgba(std::get<ofTexture*>(original->read("image")),{1,0,0,1});
        assert(!original->container->getDynamicNodes().at("Mixer Alpha").count(1));
    }
    assert(glGetError()==GL_NO_ERROR); assert(liveGui==0); fs::current_path(cwd);
    std::cout<<"Texture GPU checks passed: Screen/Add/Multiply/Difference, Wipe, stable Noise Dissolve, FadeOut, exact RGBA/endpoints/size/HDR, options/persistence, numeric readback, generic addons, source refs, cables, independent clipboard/Macro buffers.\n";
}
'''
if __name__=='__main__':
    mixer.main(extra_includes=[mixer.ROOT.parent/'ofxOceanodeTextures/src',mixer.ROOT.parent/'ofxOceanodeTextures/src/shaders'],
)
