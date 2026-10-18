#pragma once
#include "ofJson.h"
#include <variant>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <type_traits>
#include "ofxOceanodeParameter.h"
#include <memory>
#include <stdexcept>
class ofTexture;

// Value policies and private draw-stage renderers, separate from node factories.
// Functions must not mutate graphs/transport or retain borrowed live input pointers.
namespace MacroMixerMixing {
struct Object {
    std::string type;
    std::shared_ptr<ofAbstractParameter> data;
    // Update-stage resource policies may retain the graph producing a borrowed port.
    std::shared_ptr<void> owner;
    bool operator==(const Object& other) const { return type==other.type && data==other.data; }
    bool operator!=(const Object& other) const { return !(*this==other); }
};
using Value = std::variant<float, std::vector<float>, int, std::vector<int>, bool, ofTexture*, Object>;
template<typename T> Value object(const std::string& type,const T& value) {
    auto parameter=std::make_shared<ofParameter<T>>(); parameter->set(type,value);
    return Object{type,std::move(parameter)};
}
template<typename T> const T& object(const Value& value) { return std::get<Object>(value).data->cast<T>().get(); }
constexpr size_t maxVectorSize = 4096;
template<typename T> const char* typeName();
template<> inline const char* typeName<float>() { return "Float"; }
template<> inline const char* typeName<std::vector<float>>() { return "Float Vector"; }
template<> inline const char* typeName<int>() { return "Integer"; }
template<> inline const char* typeName<std::vector<int>>() { return "Integer Vector"; }
template<> inline const char* typeName<bool>() { return "Boolean"; }
template<> inline const char* typeName<ofTexture*>() { return "Texture"; }
// Built-in value/port adapters, not a node or host-type registry.
const std::vector<std::string>& supportedTypes();
std::string routerModel(const std::string& type);
Value neutral(const std::string& type);
// GPU inputs are borrowed live values. Persistence records null, never addresses/pixels.
ofJson toJson(const Value& value);
void drawIsolated(const std::function<void()>& draw);
inline const char* valueType(const Value& value) {
    return std::visit([](const auto& v) {
        if constexpr(std::is_same_v<std::decay_t<decltype(v)>,Object>) return v.type.c_str();
        else return typeName<std::decay_t<decltype(v)>>();
    },value);
}
struct TypeAdapter {
    std::string type,model,description;
    std::function<Value()> neutral;
    std::function<Value(ofxOceanodeAbstractParameter&)> read;
    std::function<void(ofxOceanodeAbstractParameter&,const Value&)> write;
    std::function<std::shared_ptr<ofxOceanodeAbstractParameter>(const std::string&,const Value&)> makePort;
    std::function<void(const Value&)> validate;
};
template<typename T> struct Definition {
    std::string type,policy,label;
    std::function<T(const T&,const T&,float,const ofJson&)> mix;
    std::function<void(const T&)> validate=[](const T&) {};
    std::function<ofJson(const ofJson&)> options=[](const ofJson& o) {
        if(!o.is_object() || !o.empty()) throw std::runtime_error("This mixing policy takes no options");
        return ofJson::object();
    };
    bool allowSwitch=false;
};
struct Policy {
    using Function = std::function<Value(const Value&, const Value&, float, const ofJson&)>;
    std::string id, label, type;
    std::function<Value(const Value&, const Value&, float, const ofJson&)> mix;
    // Validate and return canonical options, including defaults. Reject unknown
    // fields rather than silently changing a saved policy's meaning.
    std::function<ofJson(const ofJson&)> options;
    // Resource policies create one private renderer per runtime/router. Invoked
    // only in draw(), with the host GL context current; never global GPU state.
    std::function<Function()> createRenderer;
    // Private per-router resource processing during update (e.g. server audio).
    std::function<Function()> createProcessor;
    // Resource backends may need one graph-rebuild acknowledgement before fading.
    std::function<bool(const Value&)> ready;
};
class Registry {
public:
    Registry();
    // Resource policies supply createRenderer; its closure owns the output and
    // all GPU resources for one router in one runtime. No global/shared FBOs.
    void registerPolicy(Policy policy,bool makeDefault=false);
    void registerSwitchPolicy(const std::string& type);
    const TypeAdapter& adapter(const std::string& type) const;
    bool registerAdapter(TypeAdapter adapter);
    template<typename T> bool registerValueType(const std::string& type,const std::string& model,
        std::function<void(const T&)> validate,T initial=T()) {
        TypeAdapter a; a.type=type; a.model=model; a.description=typeid(T).name();
        a.neutral=[type,initial] { return object(type,initial); };
        a.read=[type](ofxOceanodeAbstractParameter& p) { return object(type,p.cast<T>().getParameter().get()); };
        a.write=[](ofxOceanodeAbstractParameter& p,const Value& v) { p.cast<T>().getParameter()=object<T>(v); };
        a.makePort=[](const std::string& name,const Value& v)->std::shared_ptr<ofxOceanodeAbstractParameter> {
            ofParameter<T> p; p.set(name,object<T>(v));
            auto port=std::make_shared<ofxOceanodeParameter<T>>(); port->bindParameter(p); return port;
        };
        a.validate=[type,validate](const Value& v) {
            const auto* boxed=std::get_if<Object>(&v);
            if(!boxed || boxed->type!=type || !boxed->data || boxed->data->valueType()!=typeid(T).name())
                throw std::runtime_error("Invalid custom mixing value: "+type);
            validate(object<T>(v));
        };
        return registerAdapter(std::move(a));
    }
    template<typename T> void registerValuePolicy(const std::string& type,const std::string& id,const std::string& label,
        std::function<T(const T&,const T&,float,const ofJson&)> function,
        std::function<ofJson(const ofJson&)> options,bool makeDefault=false) {
        registerPolicy({id,label,type,[type,function](const Value& a,const Value& b,float t,const ofJson& o) {
            return object(type,function(object<T>(a),object<T>(b),t,o));
        },std::move(options)},makeDefault);
    }
    template<typename T>
    void registerPolicy(const std::string& id, const std::string& label,
        std::function<T(const T&,const T&,float,const ofJson&)> function,
        std::function<ofJson(const ofJson&)> options, bool makeDefault=false) {
        add({id,label,typeName<T>(),
            [function](const Value& a,const Value& b,float t,const ofJson& o)->Value {
                return function(std::get<T>(a),std::get<T>(b),t,o);
            },std::move(options)},makeDefault);
    }
    const Policy& policy(const std::string& type,const std::string& id="") const;
    std::vector<Policy> policies(const std::string& type) const;
    void setDefault(const std::string& type,const std::string& id);
    Value mix(const std::string& type,const std::string& id,const Value& a,
        const Value& b,float progress,const ofJson& options=ofJson::object()) const;
    static void validate(const std::string& type,const Value& value);
private:
    void add(Policy policy,bool makeDefault);
    void validateValue(const std::string& type,const Value& value) const;
    std::map<std::string,Policy> entries;
    std::map<std::string,std::string> defaults;
    std::map<std::string,TypeAdapter> adapters;
};
Registry& registry();
}
