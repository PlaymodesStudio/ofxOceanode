#include "ofxOceanodeShared.h"
#include "MacroMixerMixing.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include "ofMain.h"

namespace MacroMixerMixing {
namespace {
void require(bool value,const std::string& message) { if(!value) throw std::runtime_error(message); }
ofJson noOptions(const ofJson& options) {
    require(options.is_object() && options.empty(),"This mixing policy takes no options"); return ofJson::object();
}
ofJson switchOptions(const ofJson& options) {
    require(options.is_object() && (options.empty() || (options.size()==1 && options.contains("threshold"))),"Switch accepts only threshold");
    const auto v=options.value("threshold",ofJson(.5));
    require(v.is_number(),"Switch threshold must be numeric"); const double t=v.get<double>();
    require(std::isfinite(t) && t>=0 && t<=1,"Switch threshold must be in [0,1]"); return {{"threshold",t}};
}
ofJson textureOptions(const ofJson& options,ofJson defaults) {
    require(options.is_object(),"Texture transition options must be an object");
    for(auto it=options.begin();it!=options.end();++it) {
        require(defaults.contains(it.key()),"Unknown texture transition option: "+it.key());
        require(it.value().is_number(),"Texture transition options must be numeric");
        if(defaults[it.key()].is_number_integer()) require(it.value().is_number_integer(),it.key()+" must be an integer");
        defaults[it.key()]=it.value();
    }
    for(auto it=defaults.begin();it!=defaults.end();++it) {
        const double v=it.value().get<double>();
        const double maximum=it.key()=="angle" ? 360 : it.key()=="cells" ? 512 : it.key()=="seed" ? 65535 : 1;
        const double minimum=it.key()=="cells" ? 1 : 0;
        require(std::isfinite(v) && v>=minimum && v<=maximum,it.key()+" must be in ["+std::to_string(int(minimum))+","+std::to_string(int(maximum))+"]");
        it.value()=(it.key()=="cells" || it.key()=="seed") ? ofJson(int(v)) : ofJson(v);
    }
    return defaults;
}
ofJson blendOptions(const ofJson& options) { return textureOptions(options,{{"strength",1.0}}); }
ofJson wipeOptions(const ofJson& options) { return textureOptions(options,{{"angle",0.0},{"softness",.05}}); }
ofJson noiseOptions(const ofJson& options) { return textureOptions(options,{{"cells",64},{"seed",0},{"softness",.05}}); }
ofJson resampleOptions(const ofJson& options) {
    require(options.is_object() && (options.empty() || (options.size()==1 && options.contains("size"))),"Resample accepts only size");
    const auto v=options.value("size",ofJson(0));
    require(v.is_number_integer(),"Resample size must be an integer");
    const double n=v.get<double>(); require(n>=0 && n<=maxVectorSize,"Resample size must be 0..4096 (0 uses the larger live length)");
    return {{"size",int(n)}};
}
float interpolate(float a,float b,double t) { return float(double(a)+(double(b)-a)*t); }
template<typename T> double sample(const std::vector<T>& v,size_t i,size_t n) {
    if(v.empty()) return 0;
    if(v.size()==1 || n<=1) return v.front();
    const double position=double(i)*(v.size()-1)/double(n-1);
    const auto low=size_t(std::floor(position)), high=std::min(low+1,v.size()-1);
    return double(v[low])+(double(v[high])-double(v[low]))*(position-low);
}
// Index / zero fill reads a 1-value vector as that value at every index, as
// nodes do, so a 1-value endpoint does not fade the other indices to zero.
template<typename T> T indexed(const std::vector<T>& v,size_t i) {
    return v.size()==1 ? v.front() : i<v.size() ? v[i] : T(0);
}
int rounded(double value) {
    // Inputs are bounded ints; use double throughout resampling/blending so
    // INT_MIN/INT_MAX and negative halfway values are safe and deterministic.
    const double result=std::round(value);
    require(std::isfinite(result) && result>=std::numeric_limits<int>::lowest()
        && result<=std::numeric_limits<int>::max(),"Integer mix overflow");
    return int(result);
}
double blend(double a,double b,float t) { return a+(b-a)*t; }
std::vector<std::string>& typeNames() {
    static std::vector<std::string> names{"Float","Float Vector","Integer","Integer Vector","Boolean","Texture"};
    return names;
}
// Never return a graph's borrowed texture to the host: graph promotion/failure
// can destroy it before the next draw. This surface owns a stable output object.
enum class TextureMode { Crossfade, Switch, Screen, Add, Multiply, Difference, Wipe, Noise, FadeOut };
class TextureSurface {
    ofFbo fbo;
    ofTexture black,published;
    ofShader shaders[4];
    ofVbo quad;
public:
    Value render(const Value& av,const Value& bv,float t,const ofJson& options,TextureMode mode) {
        auto* a=std::get<ofTexture*>(av); auto* b=std::get<ofTexture*>(bv);
        if(a && !a->isAllocated()) a=nullptr;
        if(b && !b->isAllocated()) b=nullptr;
        if(mode==TextureMode::Switch) { a=b=t==0 ? a : t==1 ? b : t<options.at("threshold").get<float>() ? a : b; t=0; }
        if(t==0) b=a;
        if(t==1) a=b;
        if(!a && !b) return static_cast<ofTexture*>(nullptr);
        const int w=std::max(a?int(a->getWidth()):1,b?int(b->getWidth()):1);
        const int h=std::max(a?int(a->getHeight()):1,b?int(b->getHeight()):1);
        require(w<=4096 && h<=4096,"Texture mixing supports dimensions up to 4096");
        if(!black.isAllocated()) {
            ofFloatPixels pixels; pixels.allocate(1,1,4); pixels.set(0);
            black.allocate(pixels,false); black.loadData(pixels);
        }
        a=a?a:&black; b=b?b:&black;
        const bool ar=a->getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
        const bool br=b->getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
        auto& shader=shaders[(ar?1:0)+(br?2:0)];
        if(!shader.isLoaded()) {
            const std::string vertex=R"(#version 150
                uniform mat4 modelViewProjectionMatrix;
                in vec4 position; in vec2 texcoord; out vec2 uv;
                void main(){ uv=texcoord; gl_Position=modelViewProjectionMatrix*position; }
            )";
            const std::string fragment=std::string("#version 150\n")+
                "uniform "+(ar?"sampler2DRect":"sampler2D")+" texA;\n"+
                "uniform "+(br?"sampler2DRect":"sampler2D")+" texB;\n"+R"(
                uniform vec2 scaleA,scaleB,offsetA,offsetB;
                uniform float progress,strength,angle,softness;
                uniform int transitionMode,cells,seed;
                in vec2 uv; out vec4 color;
                float reveal(float field) {
                    if(softness<=0.0) return step(field,progress);
                    float edge=mix(-0.5*softness,1.0+0.5*softness,progress);
                    return 1.0-smoothstep(edge-0.5*softness,edge+0.5*softness,field);
                }
                float noiseField() {
                    // Integer hash: static normalized grid, no clock or global RNG.
                    uvec2 cell=uvec2(floor(uv*float(cells)));
                    uint h=cell.x*374761393u+cell.y*668265263u+uint(seed)*2246822519u;
                    h=(h^(h>>13u))*1274126177u; h=h^(h>>16u);
                    return (float(h&0x00ffffffu)+0.5)/16777216.0;
                }
                void main(){
                    vec4 a=texture(texA,uv*scaleA+offsetA),b=texture(texB,uv*scaleB+offsetB);
                    // Every policy has exact source endpoints, including missing images.
                    if(progress<=0.0) { color=a; return; }
                    if(progress>=1.0) { color=b; return; }
                    if(transitionMode==8) {
                        // Fade RGBA to transparent black at halfway, then bring in B.
                        color=progress<0.5 ? a*(1.0-2.0*progress) : b*(2.0*progress-1.0);
                        return;
                    }
                    if(transitionMode==6 || transitionMode==7) {
                        vec2 direction=vec2(cos(radians(angle)),sin(radians(angle)));
                        float field=transitionMode==7 ? noiseField() : dot(uv-0.5,direction)/(abs(direction.x)+abs(direction.y))+0.5;
                        color=mix(a,b,reveal(field)); return;
                    }
                    color=mix(a,b,progress);
                    vec3 blended=color.rgb;
                    if(transitionMode==2) blended=1.0-(1.0-a.rgb)*(1.0-b.rgb);
                    if(transitionMode==3) blended=a.rgb+b.rgb;
                    if(transitionMode==4) blended=a.rgb*b.rgb;
                    if(transitionMode==5) blended=abs(a.rgb-b.rgb);
                    // A -> blend(A,B) -> B. Alpha remains the ordinary crossfade;
                    // blend equations act only on RGB. Add retains HDR values.
                    color.rgb=mix(color.rgb,blended,4.0*progress*(1.0-progress)*strength);
                }
            )";
            require(shader.setupShaderFromSource(GL_VERTEX_SHADER,vertex)
                && shader.setupShaderFromSource(GL_FRAGMENT_SHADER,fragment),"Texture mixer requires an OpenGL 3.2 programmable renderer");
            shader.bindDefaults(); require(shader.linkProgram(),"Texture mixer shader link failed");
        }
        if(!fbo.isAllocated() || fbo.getWidth()!=w || fbo.getHeight()!=h || fbo.getTexture().getTextureData().glInternalFormat!=ofxOceanodeShared::getColorTextureInternalFormat()) {
            ofFbo::Settings settings; settings.width=w; settings.height=h;
            settings.internalformat=ofxOceanodeShared::getColorTextureInternalFormat(); settings.textureTarget=GL_TEXTURE_2D;
            settings.useDepth=false; settings.useStencil=false; settings.numSamples=0;
            settings.minFilter=GL_LINEAR; settings.maxFilter=GL_LINEAR;
            fbo.allocate(settings); require(fbo.isAllocated(),"Texture mixer allocation failed");
        }
        // Render straight RGBA channels, with no host tint/alpha/depth/scissor.
        // OF's style stack restores its blend mode; preserve raw test flags too.
        const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),stencil=glIsEnabled(GL_STENCIL_TEST);
        GLboolean mask[4]; glGetBooleanv(GL_COLOR_WRITEMASK,mask);
        ofPushStyle(); ofDisableAlphaBlending(); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glColorMask(true,true,true,true);
        fbo.begin();
        shader.begin(); shader.setUniformTexture("texA",*a,0); shader.setUniformTexture("texB",*b,1);
        const auto coordinates=[&](const char* scale,const char* offset,ofTexture* texture) {
            auto size=texture->getCoordFromPercent(1,1);
            const bool flip=texture->getTextureData().bFlipTexture;
            shader.setUniform2f(scale,size.x,flip?-size.y:size.y);
            shader.setUniform2f(offset,0,flip?size.y:0);
        };
        coordinates("scaleA","offsetA",a); coordinates("scaleB","offsetB",b);
        shader.setUniform1f("progress",t);
        shader.setUniform1i("transitionMode",int(mode));
        shader.setUniform1f("strength",options.value("strength",1.f));
        shader.setUniform1f("angle",options.value("angle",0.f));
        shader.setUniform1f("softness",options.value("softness",.05f));
        shader.setUniform1i("cells",options.value("cells",64));
        shader.setUniform1i("seed",options.value("seed",0));
        const glm::vec3 vertices[]={{0,0,0},{float(w),0,0},{float(w),float(h),0},{0,float(h),0}};
        const glm::vec2 uv[]={{0,0},{1,0},{1,1},{0,1}};
        quad.setVertexData(vertices,4,GL_STREAM_DRAW); quad.setTexCoordData(uv,4,GL_STREAM_DRAW);
        quad.draw(GL_TRIANGLE_FAN,0,4); shader.end(); fbo.end(); ofPopStyle();
        for(const auto& state:{std::make_pair(GL_DEPTH_TEST,depth),{GL_CULL_FACE,cull},{GL_SCISSOR_TEST,scissor},{GL_STENCIL_TEST,stencil}})
            if(state.second) glEnable(state.first); else glDisable(state.first);
        glColorMask(mask[0],mask[1],mask[2],mask[3]);
        published=fbo.getTexture(); // Shared GPU ownership survives failed reallocations.
        return &published;
    }
};
}
const std::vector<std::string>& supportedTypes() {
    return typeNames();
}
std::string routerModel(const std::string& type) {
    if(type=="Float") return "Router f";
    if(type=="Float Vector") return "Router v_f";
    if(type=="Integer") return "Router i";
    if(type=="Integer Vector") return "Router v_i";
    if(type=="Boolean") return "Router b";
    if(type=="Texture") return "Router Texture";
    return registry().adapter(type).model;
}
Value neutral(const std::string& type) {
    if(type=="Float") return 0.f;
    if(type=="Float Vector") return std::vector<float>{0.f};
    if(type=="Integer") return 0;
    if(type=="Integer Vector") return std::vector<int>{0};
    if(type=="Boolean") return false;
    if(type=="Texture") return static_cast<ofTexture*>(nullptr);
    return registry().adapter(type).neutral();
}
ofJson toJson(const Value& value) {
    return std::visit([](const auto& v)->ofJson {
        if constexpr(std::is_same_v<std::decay_t<decltype(v)>,ofTexture*> || std::is_same_v<std::decay_t<decltype(v)>,Object>) return nullptr;
        else return v;
    },value);
}
void drawIsolated(const std::function<void()>& draw) {
    struct Scope {
        GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),stencil=glIsEnabled(GL_STENCIL_TEST),blend=glIsEnabled(GL_BLEND),mask[4];
        GLint srcRGB,dstRGB,srcAlpha,dstAlpha,equationRGB,equationAlpha;
        GLfloat blendColor[4];
        Scope() {
            glGetBooleanv(GL_COLOR_WRITEMASK,mask); glGetFloatv(GL_BLEND_COLOR,blendColor);
            glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB); glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
            glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
            ofPushView(); ofPushStyle(); ofDisableAlphaBlending(); ofSetColor(255);
            glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glColorMask(true,true,true,true);
        }
        ~Scope() {
            ofPopStyle(); ofPopView();
            glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha); glBlendEquationSeparate(equationRGB,equationAlpha);
            glBlendColor(blendColor[0],blendColor[1],blendColor[2],blendColor[3]);
            for(const auto& state:{std::make_pair(GL_DEPTH_TEST,depth),{GL_CULL_FACE,cull},{GL_SCISSOR_TEST,scissor},{GL_STENCIL_TEST,stencil},{GL_BLEND,blend}})
                if(state.second) glEnable(state.first); else glDisable(state.first);
            glColorMask(mask[0],mask[1],mask[2],mask[3]);
        }
    } scope;
    draw();
}
Registry::Registry() {
    struct TexturePolicy { TextureMode mode; const char* id; const char* label; std::function<ofJson(const ofJson&)> options; };
    for(const auto& spec:std::vector<TexturePolicy>{
        {TextureMode::Crossfade,"crossfade","RGBA crossfade",noOptions},
        {TextureMode::Switch,"switch","Switch",switchOptions},
        {TextureMode::Screen,"screen","Screen",blendOptions},
        {TextureMode::Add,"add","Add",blendOptions},
        {TextureMode::Multiply,"multiply","Multiply",blendOptions},
        {TextureMode::Difference,"difference","Difference",blendOptions},
        {TextureMode::Wipe,"wipe","Wipe",wipeOptions},
        {TextureMode::Noise,"noise_dissolve","Noise Dissolve",noiseOptions},
        {TextureMode::FadeOut,"fade_out","FadeOut",noOptions}}) {
        Policy p{spec.id,spec.label,"Texture",
            [](const Value&,const Value&,float,const ofJson&)->Value { throw std::runtime_error("Texture policies must run in draw with a private renderer"); },
            spec.options};
        p.createRenderer=[mode=spec.mode] {
            auto surface=std::make_shared<TextureSurface>();
            return [surface,mode](const Value& a,const Value& b,float t,const ofJson& options)->Value {
                return surface->render(a,b,t,options,mode);
            };
        };
        add(std::move(p),spec.mode==TextureMode::Crossfade);
    }
    registerPolicy<float>("linear","Linear",[](float a,float b,float t,const ofJson&){return interpolate(a,b,t);},noOptions,true);
    registerPolicy<float>("switch","Switch",[](float a,float b,float t,const ofJson& o){return t<o.at("threshold").get<double>() ? a : b;},switchOptions);
    registerPolicy<std::vector<float>>("index_zero","Index / zero fill",[](const auto& a,const auto& b,float t,const ofJson&) {
        std::vector<float> result(std::max(a.size(),b.size()));
        for(size_t i=0;i<result.size();++i) result[i]=interpolate(indexed(a,i),indexed(b,i),t);
        return result;
    },noOptions,true);
    registerPolicy<std::vector<float>>("resample","Resample",[](const auto& a,const auto& b,float t,const ofJson& o) {
        const auto size=o.at("size").get<size_t>();
        std::vector<float> result(size ? size : std::max(a.size(),b.size()));
        for(size_t i=0;i<result.size();++i) result[i]=interpolate(sample(a,i,result.size()),sample(b,i,result.size()),t);
        return result;
    },resampleOptions);
    registerPolicy<std::vector<float>>("resample_length","Resample / changing length",[](const auto& a,const auto& b,float t,const ofJson&) {
        // Blend lengths in floating point: subtracting unsigned sizes would
        // underflow when the destination is shorter. Round to the nearest count.
        const auto size=size_t(std::lround(double(a.size())+(double(b.size())-double(a.size()))*t));
        std::vector<float> result(size);
        for(size_t i=0;i<result.size();++i) result[i]=interpolate(sample(a,i,size),sample(b,i,size),t);
        return result;
    },noOptions);
    registerPolicy<std::vector<float>>("switch","Switch whole vector",[](const auto& a,const auto& b,float t,const ofJson& o) {
        return t<o.at("threshold").template get<double>() ? a : b;
    },switchOptions);
    registerPolicy<int>("linear","Linear / round",[](int a,int b,float t,const ofJson&) {
        return rounded(blend(a,b,t));
    },noOptions,true);
    registerPolicy<int>("switch","Switch",[](int a,int b,float t,const ofJson& o) {
        return t<o.at("threshold").get<double>() ? a : b;
    },switchOptions);
    registerPolicy<bool>("switch","Switch",[](bool a,bool b,float t,const ofJson& o) {
        return t<o.at("threshold").get<double>() ? a : b;
    },switchOptions,true);
    registerPolicy<std::vector<int>>("index_zero","Index / zero fill / round",[](const auto& a,const auto& b,float t,const ofJson&) {
        std::vector<int> result(std::max(a.size(),b.size()));
        for(size_t i=0;i<result.size();++i) result[i]=rounded(blend(indexed(a,i),indexed(b,i),t));
        return result;
    },noOptions,true);
    registerPolicy<std::vector<int>>("resample","Resample / round",[](const auto& a,const auto& b,float t,const ofJson& o) {
        const auto size=o.at("size").get<size_t>();
        std::vector<int> result(size ? size : std::max(a.size(),b.size()));
        for(size_t i=0;i<result.size();++i) result[i]=rounded(blend(sample(a,i,result.size()),sample(b,i,result.size()),t));
        return result;
    },resampleOptions);
    registerPolicy<std::vector<int>>("resample_length","Resample / changing length / round",[](const auto& a,const auto& b,float t,const ofJson&) {
        const auto size=size_t(std::lround(double(a.size())+(double(b.size())-double(a.size()))*t));
        std::vector<int> result(size);
        for(size_t i=0;i<size;++i) result[i]=rounded(blend(sample(a,i,size),sample(b,i,size),t));
        return result;
    },noOptions);
    registerPolicy<std::vector<int>>("switch","Switch whole vector",[](const auto& a,const auto& b,float t,const ofJson& o) {
        return t<o.at("threshold").template get<double>() ? a : b;
    },switchOptions);
}
void Registry::add(Policy policy,bool makeDefault) {
    require(!policy.id.empty() && !policy.label.empty() && bool(policy.mix) && bool(policy.options),"Invalid mixing policy registration");
    if(!adapters.count(policy.type)) routerModel(policy.type);
    const auto key=policy.type+"/"+policy.id;
    require(!entries.count(key),"Mixing policy is already registered: "+key);
    policy.options(ofJson::object()); // A registered policy must define its defaults.
    if(makeDefault || !defaults.count(policy.type)) defaults[policy.type]=policy.id;
    entries.emplace(key,std::move(policy));
}
void Registry::registerPolicy(Policy policy,bool makeDefault) { add(std::move(policy),makeDefault); }
void Registry::registerSwitchPolicy(const std::string& type) {
    add({"switch","Switch",type,[](const Value& a,const Value& b,float progress,const ofJson& options) {
        return progress<options.at("threshold").get<double>() ? a : b;
    },switchOptions},false);
}
const TypeAdapter& Registry::adapter(const std::string& type) const {
    const auto found=adapters.find(type); require(found!=adapters.end(),"Unsupported mixing type: "+type);
    return found->second;
}
bool Registry::registerAdapter(TypeAdapter a) {
    require(!a.type.empty() && !a.model.empty() && !a.description.empty() && bool(a.neutral) && bool(a.read)
        && bool(a.write) && bool(a.makePort) && bool(a.validate),"Incomplete mixing type adapter");
    const auto found=adapters.find(a.type);
    if(found!=adapters.end()) {
        require(found->second.description==a.description && found->second.model==a.model,"Conflicting mixing type adapter: "+a.type);
        return false; // Several host instances can register the same ordinary addon type.
    }
    require(std::find(typeNames().begin(),typeNames().end(),a.type)==typeNames().end(),"Built-in mixing adapter cannot be replaced");
    a.validate(a.neutral()); typeNames().push_back(a.type); adapters.emplace(a.type,std::move(a)); return true;
}
const Policy& Registry::policy(const std::string& type,const std::string& id) const {
    auto selected=id;
    if(selected.empty()) {
        const auto found=defaults.find(type); require(found!=defaults.end(),"Unsupported mixing type: "+type); selected=found->second;
    }
    const auto found=entries.find(type+"/"+selected);
    require(found!=entries.end(),"Unknown mixing policy: "+type+"/"+selected); return found->second;
}
std::vector<Policy> Registry::policies(const std::string& type) const {
    std::vector<Policy> result; for(const auto& entry:entries) if(entry.second.type==type) result.push_back(entry.second); return result;
}
void Registry::setDefault(const std::string& type,const std::string& id) { const auto& p=policy(type,id); defaults[type]=p.id; }
void Registry::validate(const std::string& type,const Value& value) { registry().validateValue(type,value); }
void Registry::validateValue(const std::string& type,const Value& value) const {
    if(type=="Float") require(std::holds_alternative<float>(value) && std::isfinite(std::get<float>(value)),"Expected a finite Float");
    else if(type=="Float Vector") {
        require(std::holds_alternative<std::vector<float>>(value),"Expected a Float Vector");
        const auto& v=std::get<std::vector<float>>(value); require(v.size()<=maxVectorSize,"Float Vector exceeds 4096 values");
        for(float item:v) require(std::isfinite(item),"Float Vector contains non-finite values");
    } else if(type=="Integer") require(std::holds_alternative<int>(value),"Expected an Integer");
    else if(type=="Boolean") require(std::holds_alternative<bool>(value),"Expected a Boolean");
    else if(type=="Integer Vector") {
        require(std::holds_alternative<std::vector<int>>(value),"Expected an Integer Vector");
        require(std::get<std::vector<int>>(value).size()<=maxVectorSize,"Integer Vector exceeds 4096 values");
    } else if(type=="Texture") {
        require(std::holds_alternative<ofTexture*>(value),"Expected a Texture");
        auto* texture=std::get<ofTexture*>(value);
        if(texture && texture->isAllocated()) {
            const auto target=texture->getTextureData().textureTarget;
            require(target==GL_TEXTURE_2D || target==GL_TEXTURE_RECTANGLE_ARB,"Texture mixer supports 2D and rectangle textures");
            require(texture->getWidth()<=4096 && texture->getHeight()<=4096,"Texture mixing supports dimensions up to 4096");
        }
    }
    else adapter(type).validate(value);
}
Value Registry::mix(const std::string& type,const std::string& id,const Value& a,const Value& b,float t,const ofJson& options) const {
    const auto& p=policy(type,id); const auto settings=p.options(options);
    require(std::isfinite(t) && t>=0 && t<=1,"Mix progress must be in [0,1]"); validateValue(type,a); validateValue(type,b);
    // Endpoints bypass every policy (including resampling and threshold=0/1).
    Value result=t==0 ? a : t==1 ? b : p.mix(a,b,t,settings); validateValue(type,result); return result;
}
std::string portType(ofxOceanodeAbstractParameter& port) {
    for(const auto& type:supportedTypes()) {
        bool matches=false;
        std::visit([&](const auto& zero) {
            using T=std::decay_t<decltype(zero)>;
            if constexpr(std::is_same_v<T,Object>) matches=registry().adapter(type).description==port.valueType();
            else matches=port.valueType()==typeid(T).name();
        },neutral(type));
        if(matches) return type;
    }
    return "";
}
Value readPort(const std::string& type, ofxOceanodeAbstractParameter& port) {
    return std::visit([&](const auto& zero)->Value {
        using T=std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,Object>) return registry().adapter(type).read(port);
        else return port.cast<T>().getParameter().get();
    },neutral(type));
}
void writePort(const std::string& type, ofxOceanodeAbstractParameter& port, const Value& value, bool silent) {
    std::visit([&](const auto& zero) {
        using T=std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,Object>) registry().adapter(type).write(port,value);
        else {
            auto p=port.cast<T>().getParameter();
            if(p.get()==std::get<T>(value)) return;
            if(silent) p.setWithoutEventNotifications(std::get<T>(value)); else p=std::get<T>(value);
        }
    },neutral(type));
}
std::shared_ptr<ofxOceanodeAbstractParameter> makePort(const std::string& type, const std::string& name, const Value& initial) {
    return std::visit([&](const auto& zero)->std::shared_ptr<ofxOceanodeAbstractParameter> {
        using T=std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,Object>) return registry().adapter(type).makePort(name,initial);
        else {
            ofParameter<T> p; p.set(name,std::get<T>(initial));
            auto port=std::make_shared<ofxOceanodeParameter<T>>(); port->bindParameter(p); return port;
        }
    },neutral(type));
}
Registry& registry() { static Registry instance; return instance; }
}
