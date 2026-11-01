#!/usr/bin/env python3
"""Run production OSC Variables groups and nodes with real OF parameters.

The group methods and the OSC node are taken from the sibling ofxOceanodeOSC
addon and run with ASan/UBSan against a small node-model, parameter-wrapper,
connection and ofxOsc adapter. Covers table order, rename, retype, reordering,
reserved pin names, one message per change, and receiving at a renamed address.
No GUI, socket or container is used. Skipped when the addon is not present.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

from of_environment import ROOT, of_root
from new_preset_regression import block


def addon_source():
    for base in (of_root() / "addons", ROOT.parent):
        for name in ("ofxOceanodeOSC", "ofxOceanodeOsc"):
            if (base / name / "src/oscVariables.cpp").exists():
                return base / name / "src"
    return None


def header_declarations(header):
    start = header.index("enum class OscMode")
    end = header.index("//-------------------------------------------------------------------------\nclass ofxOceanodeOSCVariablesController")
    return header[start:end]


def node_class(header):
    start = header.index("class oscVariables : public ofxOceanodeNodeModel")
    return header[start:header.rindex("#endif")]


def without_includes(source):
    return re.sub(r"^#include .*$", "", source, flags=re.M)


def production(controller):
    parts = [block(controller, "namespace {")]
    for signature in (
        "const char* oscVariableTypeName(int type)",
        "int oscVariableTypeOf(const ofAbstractParameter& parameter)",
        "std::shared_ptr<ofAbstractParameter> createOscVariable(int type",
        "std::string oscVariablesGroup::modelName()",
        "void oscVariablesGroup::addNode(",
        "void oscVariablesGroup::removeNode(",
        "void oscVariablesGroup::listen(",
        "void oscVariablesGroup::addParameter(",
        "void oscVariablesGroup::addFloatParameter(",
        "void oscVariablesGroup::addFloatVectorParameter(",
        "void oscVariablesGroup::addIntParameter(",
        "void oscVariablesGroup::addIntVectorParameter(",
        "void oscVariablesGroup::addStringParameter(",
        "void oscVariablesGroup::addStringVectorParameter(",
        "void oscVariablesGroup::removeParameter(",
        "bool oscVariablesGroup::isParameterNameAvailable(",
        "bool oscVariablesGroup::renameParameter(",
        "bool oscVariablesGroup::setParameterType(",
        "bool oscVariablesGroup::moveParameter(",
        "void oscVariablesGroup::update()",
    ):
        parts.append(block(controller, signature))
    return "\n\n".join(parts)


ADAPTER = r'''
#include "ofMain.h"
#include <cassert>
#include <cfloat>
#include <climits>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
using namespace std;

// ofxOsc: the sender records messages; the receiver hands out queued ones.
enum ofxOscArgType { OFXOSC_TYPE_INT32 = 'i', OFXOSC_TYPE_FLOAT = 'f', OFXOSC_TYPE_STRING = 's' };
class ofxOscMessage {
public:
    struct Argument { ofxOscArgType type; float f = 0; int32_t i = 0; string s; };
    void setAddress(const string& a){ address = a; }
    string getAddress() const { return address; }
    void addFloatArg(float v){ arguments.push_back({OFXOSC_TYPE_FLOAT, v}); }
    void addIntArg(int32_t v){ arguments.push_back({OFXOSC_TYPE_INT32, 0, v}); }
    void addStringArg(const string& v){ arguments.push_back({OFXOSC_TYPE_STRING, 0, 0, v}); }
    size_t getNumArgs() const { return arguments.size(); }
    ofxOscArgType getArgType(size_t i) const { return arguments[i].type; }
    float getArgAsFloat(size_t i) const { return arguments[i].f; }
    int32_t getArgAsInt32(size_t i) const { return arguments[i].i; }
    string getArgAsString(size_t i) const { return arguments[i].s; }
    string address;
    vector<Argument> arguments;
};
class ofxOscSender {
public:
    void sendMessage(const ofxOscMessage& m, bool = true){ sent.push_back(m); }
    vector<ofxOscMessage> sent;
};
class ofxOscReceiver {
public:
    bool hasWaitingMessages(){ return !queue.empty(); }
    bool getNextMessage(ofxOscMessage& m){ m = queue.front(); queue.pop_front(); return true; }
    deque<ofxOscMessage> queue;
};

using ofxOceanodeParameterFlags = int;
enum {
    ofxOceanodeParameterFlags_ReadOnly = 1 << 0,
    ofxOceanodeParameterFlags_DisableSavePreset = 1 << 1,
    ofxOceanodeParameterFlags_DisableInConnection = 1 << 2,
    ofxOceanodeParameterFlags_Output = 1 << 9
};
class ofxOceanodeNodeModel;
class ofxOceanodeAbstractConnection;

class ofxOceanodeAbstractParameter : public ofAbstractParameter {
public:
    vector<ofxOceanodeAbstractConnection*> inConnections;
    vector<ofxOceanodeAbstractConnection*> outConnections;
    int flags = 0;
    ofxOceanodeNodeModel* nodeModel = nullptr;
    vector<ofxOceanodeAbstractConnection*> getOutConnections(){ return outConnections; }
    const vector<ofxOceanodeAbstractConnection*>& getInConnections() const { return inConnections; }
    ofxOceanodeNodeModel* getNodeModel(){ return nodeModel; }
    void removeAllConnections();
};

class ofxOceanodeAbstractConnection {
public:
    ofxOceanodeAbstractParameter* source;
    ofxOceanodeAbstractParameter* sink;
    bool persistent = false;
    ofxOceanodeAbstractParameter& getSourceParameter(){ return *source; }
    ofxOceanodeAbstractParameter& getSinkParameter(){ return *sink; }
    bool getIsPersistent(){ return persistent; }
    void setIsPersistent(bool p){ persistent = p; }
};

void ofxOceanodeAbstractParameter::removeAllConnections(){
    for(auto* c : outConnections){
        auto& v = c->sink->inConnections;
        v.erase(remove(v.begin(), v.end(), c), v.end());
        delete c;
    }
    outConnections.clear();
    for(auto* c : inConnections){
        auto& v = c->source->outConnections;
        v.erase(remove(v.begin(), v.end(), c), v.end());
        delete c;
    }
    inConnections.clear();
}

// Forwards to the bound ofParameter, as ofxOceanodeParameter does.
template<typename T>
class oceanodeParameter : public ofxOceanodeAbstractParameter {
public:
    shared_ptr<ofParameter<T>> parameter;
    explicit oceanodeParameter(shared_ptr<ofParameter<T>> p) : parameter(p) {}
    ~oceanodeParameter(){ removeAllConnections(); }
    string getName() const override { return parameter->getName(); }
    bool setName(const string& name) override { return parameter->setName(name); }
    string toString() const override { return parameter->toString(); }
    void fromString(const string& str) override { parameter->fromString(str); }
    string type() const override { return parameter->type(); }
    string getEscapedName() const override { return parameter->getEscapedName(); }
    string valueType() const override { return parameter->valueType(); }
    bool isInit() const override { return parameter->isInit(); }
    void setParent(ofParameterGroup& parent) override { parameter->setParent(parent); }
    bool isSerializable() const override { return parameter->isSerializable(); }
    bool isReadOnly() const override { return parameter->isReadOnly(); }
    shared_ptr<ofAbstractParameter> newReference() const override {
        auto reference = make_shared<oceanodeParameter<T>>(parameter);
        reference->flags = flags;
        reference->nodeModel = nodeModel;
        return reference;
    }
    bool isReferenceTo(const ofAbstractParameter& other) const override { return parameter->isReferenceTo(other); }
    ofEvent<string>& nameChangedEvent() override { return parameter->nameChangedEvent(); }
protected:
    const ofParameterGroup getFirstParent() const override { return parameter->getFirstParent(); }
    void setSerializable(bool s) override { parameter->setSerializable(s); }
    string escape(const string&) const override { return ""; }
    const void* getInternalObject() const override { return parameter->getInternalObject(); }
};

// Connections are accepted between equal types, and from numbers to numbers.
class ofxOceanodeContainer {
public:
    ofxOceanodeAbstractConnection* createConnection(ofxOceanodeAbstractParameter& source, ofxOceanodeAbstractParameter& sink){
        auto numeric = [](const string& t){ return t == typeid(float).name() || t == typeid(int).name(); };
        if(source.valueType() != sink.valueType() && !(numeric(source.valueType()) && numeric(sink.valueType()))) return nullptr;
        auto* connection = new ofxOceanodeAbstractConnection{&source, &sink};
        source.outConnections.push_back(connection);
        sink.inConnections.push_back(connection);
        return connection;
    }
};

#define ofxOceanodeNodeModel_h
class ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeModel(string name){ parameters.setName(name); }
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup(){}
    virtual void update(ofEventArgs&){}
    ofParameterGroup& getParameterGroup(){ return parameters; }
    ofxOceanodeContainer* getHostContainer() const { return host; }
    // Pins share the parameter they wrap, as the types registry binds them.
    template<typename T>
    shared_ptr<ofxOceanodeAbstractParameter> addParameter(ofParameter<T>& p, ofxOceanodeParameterFlags flags = 0){
        return add(make_shared<oceanodeParameter<T>>(make_shared<ofParameter<T>>(p)), flags);
    }
    shared_ptr<ofxOceanodeAbstractParameter> addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags = 0){
        shared_ptr<ofxOceanodeAbstractParameter> pin;
        if(!pin) pin = wrap<float>(p);
        if(!pin) pin = wrap<int>(p);
        if(!pin) pin = wrap<string>(p);
        if(!pin) pin = wrap<vector<float>>(p);
        if(!pin) pin = wrap<vector<int>>(p);
        if(!pin) pin = wrap<vector<string>>(p);
        return pin ? add(pin, flags) : nullptr;
    }
    bool removeParameter(string name){ parameters.remove(name); return true; }
    void deleteSelf(){}
    ofEvent<void> parameterGroupChanged;
    ofxOceanodeContainer* host = nullptr;
protected:
    string description;
    static constexpr ofxOceanodeParameterFlags outputParameterFlags =
        ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_Output;
private:
    template<typename T>
    shared_ptr<ofxOceanodeAbstractParameter> wrap(ofAbstractParameter& p){
        if(p.valueType() != typeid(T).name()) return nullptr;
        return make_shared<oceanodeParameter<T>>(make_shared<ofParameter<T>>(p.cast<T>()));
    }
    shared_ptr<ofxOceanodeAbstractParameter> add(shared_ptr<ofxOceanodeAbstractParameter> pin, int flags){
        pin->flags = flags;
        pin->nodeModel = this;
        parameters.add(*pin);
        return dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(parameters.end() - 1));
    }
    ofParameterGroup parameters;
};

class oscVariables;
'''

TESTS = r'''
void expect(bool ok, const string& what){
    if(!ok){ cerr << "FAILED: " << what << endl; abort(); }
}
ofxOceanodeAbstractParameter& pin(oscVariables& node, const string& name){
    expect(node.getParameterGroup().contains(name), "pin " + name + " exists");
    return static_cast<ofxOceanodeAbstractParameter&>(node.getParameterGroup().get(name));
}
template<typename T> ofParameter<T>& value(oscVariables& node, const string& name){
    auto* wrapper = dynamic_cast<oceanodeParameter<T>*>(&pin(node, name));
    expect(wrapper != nullptr, "pin " + name + " has the expected type");
    return *wrapper->parameter;
}
vector<string> pins(oscVariables& node){
    vector<string> names;
    for(int i = 0; i < node.getParameterGroup().size(); i++) names.push_back(node.getParameterGroup().get(i).getName());
    return names;
}
vector<string> rows(oscVariablesGroup& group){
    vector<string> names;
    for(auto& p : group.parameters) names.push_back(p->getName());
    return names;
}
vector<string> with(vector<string> head, const vector<string>& tail){
    head.insert(head.end(), tail.begin(), tail.end());
    return head;
}
ofxOscMessage message(const string& address, float f){
    ofxOscMessage m;
    m.setAddress(address);
    m.addFloatArg(f);
    return m;
}
int main(){
    ofxOceanodeContainer host;

    // Sender group: two nodes share each variable.
    auto out = make_shared<oscVariablesGroup>();
    out->name = "Out";
    out->oscMode = OscMode::Sender;
    out->addFloatParameter("speed", 1);
    out->addIntVectorParameter("steps", {1, 2});
    oscVariables a("Out", out);
    a.host = &host;
    a.setup();
    oscVariables b("Out", out);
    b.host = &host;
    b.setup();
    const vector<string> senderPins = {"Host IP", "Sender Port"};
    expect(out->modelName() == "OSC Sender Out", "sender model name");
    expect(pins(a) == with(senderPins, {"speed", "steps"}), "sender pins follow table order");
    expect(pins(b) == pins(a), "second node matches");
    expect((pin(a, "speed").flags & ofxOceanodeParameterFlags_Output) == 0, "sender variable is an input");

    // Two sender nodes still send once per change.
    value<float>(a, "speed") = 3.0f;
    expect(out->sender.sent.size() == 1, "one message per change");
    expect(out->sender.sent[0].getAddress() == "/speed" && out->sender.sent[0].getArgAsFloat(0) == 3.0f, "float sent");
    expect(value<float>(b, "speed").get() == 3.0f, "nodes share the variable");
    value<vector<int>>(b, "steps") = vector<int>{4, 5, 6};
    expect(out->sender.sent.size() == 2 && out->sender.sent[1].getNumArgs() == 3 &&
           out->sender.sent[1].getArgAsInt32(2) == 6, "int vector sent");

    // A deleted node doesn't block renaming the variables it shared.
    {
        oscVariables gone("Out", out);
        gone.host = &host;
        gone.setup();
        expect(pins(gone) == pins(a), "temporary node matches");
    }
    expect(out->renameParameter("steps", "stairs"), "rename after a node was deleted");
    expect(pins(a) == with(senderPins, {"speed", "stairs"}) && pins(b) == pins(a), "remaining nodes renamed");
    expect(out->renameParameter("stairs", "steps"), "rename back");

    // Rename: pins follow, the incoming link stays, and the address changes.
    ofParameter<float> upstreamValue("Upstream", 0);
    oceanodeParameter<float> upstream(make_shared<ofParameter<float>>(upstreamValue));
    host.createConnection(upstream, pin(a, "speed"));
    expect(out->renameParameter("speed", "rate"), "rename variable");
    expect(rows(*out) == vector<string>({"rate", "steps"}), "row renamed");
    expect(pins(a) == with(senderPins, {"rate", "steps"}) && pins(b) == pins(a), "pins renamed in every node");
    expect(pin(a, "rate").getInConnections().size() == 1, "rename keeps the incoming link");
    out->sender.sent.clear();
    value<float>(a, "rate") = 5.0f;
    expect(out->sender.sent.size() == 1 && out->sender.sent[0].getAddress() == "/rate", "renamed variable sends at its new address");

    // Names are compared escaped and may not take a node pin's name.
    expect(!out->isParameterNameAvailable("Sender Port") && !out->isParameterNameAvailable("Sender_Port"), "port pin name reserved");
    expect(!out->isParameterNameAvailable("Host IP") && !out->isParameterNameAvailable("Receiver Port"), "host and receiver pin names reserved");
    expect(!out->renameParameter("rate", "Host_IP"), "rename onto a pin name rejected");
    expect(!out->renameParameter("rate", "steps"), "duplicate rejected");
    out->addFloatParameter("My Var", 0);
    expect(!out->isParameterNameAvailable("My_Var"), "escaped duplicate rejected");
    expect(!out->isParameterNameAvailable(""), "empty name rejected");
    expect(out->renameParameter("rate", "rate"), "same name is a no-op");

    // Moving rows reorders the variable pins behind the host and port pins.
    expect(out->moveParameter("My Var", 0), "move to the top");
    expect(rows(*out) == vector<string>({"My Var", "rate", "steps"}), "row moved");
    expect(pins(a) == with(senderPins, {"My Var", "rate", "steps"}) && pins(b) == pins(a), "pins follow the move");
    expect(!out->moveParameter("My Var", 0), "same row is a no-op");
    expect(out->moveParameter("My Var", 99), "past the end moves to the last row");
    expect(pins(a) == with(senderPins, {"rate", "steps", "My Var"}), "pins follow the move to the end");
    expect(pin(a, "rate").getInConnections().size() == 1, "moved pin keeps its link");

    // Retype keeps the row, the input role and the links the type accepts.
    expect(out->setParameterType("rate", oscVariableType_Int), "retype float to int");
    expect(rows(*out) == vector<string>({"rate", "steps", "My Var"}), "retyped row keeps place");
    expect(pins(a) == with(senderPins, {"rate", "steps", "My Var"}) && pins(b) == pins(a), "retyped pin keeps place");
    expect(pin(a, "rate").valueType() == typeid(int).name(), "pin is int");
    expect(pin(a, "rate").getInConnections().size() == 1, "numeric link restored after retype");
    expect((pin(a, "rate").flags & ofxOceanodeParameterFlags_Output) == 0, "retyped sender pin is still an input");
    out->sender.sent.clear();
    value<int>(b, "rate") = 7;
    expect(out->sender.sent.size() == 1 && out->sender.sent[0].getArgType(0) == OFXOSC_TYPE_INT32 &&
           out->sender.sent[0].getArgAsInt32(0) == 7, "retyped variable sends its new type once");
    expect(out->setParameterType("rate", oscVariableType_String), "retype int to string");
    expect(pin(a, "rate").getInConnections().empty(), "link the new type can't take is dropped");

    // Removal takes the pin from every node and stops sending.
    auto steps = out->parameters[1];
    out->removeParameter("steps");
    expect(rows(*out) == vector<string>({"rate", "My Var"}), "row removed");
    expect(pins(a) == with(senderPins, {"rate", "My Var"}) && pins(b) == pins(a), "pin removed from every node");
    out->sender.sent.clear();
    steps->cast<vector<int>>() = vector<int>{9};
    expect(out->sender.sent.empty(), "removed variable no longer sends");

    // Receiver group: variables are outputs, written from OSC.
    auto in = make_shared<oscVariablesGroup>();
    in->name = "In";
    in->oscMode = OscMode::Receiver;
    in->addFloatParameter("level", 0);
    in->addStringVectorParameter("words", {});
    oscVariables r("In", in);
    r.host = &host;
    r.setup();
    expect(in->modelName() == "OSC Receiv. In", "receiver model name");
    expect(pins(r) == vector<string>({"Receiver Port", "level", "words"}), "receiver pins");
    expect((pin(r, "level").flags & ofxOceanodeParameterFlags_Output) != 0, "receiver variable is an output");
    ofParameter<float> sinkValue("Sink", 0);
    oceanodeParameter<float> sink(make_shared<ofParameter<float>>(sinkValue));
    host.createConnection(pin(r, "level"), sink);

    expect(in->renameParameter("level", "gain"), "rename receiver variable");
    in->receiver.queue.push_back(message("/level", 0.25f));
    in->receiver.queue.push_back(message("/gain", 0.5f));
    in->update();
    expect(value<float>(r, "gain").get() == 0.5f, "renamed variable receives at its new address");
    expect(in->sender.sent.empty(), "a receiver never sends");

    expect(in->setParameterType("gain", oscVariableType_Int), "retype receiver variable");
    expect((pin(r, "gain").flags & ofxOceanodeParameterFlags_Output) != 0, "retyped receiver pin is still an output");
    expect(pin(r, "gain").getOutConnections().size() == 1, "numeric outgoing link restored");
    expect(in->moveParameter("words", 0), "move receiver row");
    expect(pins(r) == vector<string>({"Receiver Port", "words", "gain"}), "receiver pins follow the move");

    // A node joining later gets the current names, types and order.
    oscVariables late("In", in);
    late.host = &host;
    late.setup();
    expect(pins(late) == pins(r), "late node matches the table");
    expect((pin(late, "words").flags & ofxOceanodeParameterFlags_Output) != 0, "late receiver pin is an output");

    cout << "OSC Variables regression passed: table order, rename, retype, reorder, reserved names, sending once and receiving.\n";
}
'''


def main():
    source_dir = addon_source()
    if source_dir is None:
        print("SKIP: ofxOceanodeOSC addon not found")
        return
    controller = (source_dir / "ofxOceanodeOSCVariablesController.cpp").read_text()
    controller_header = (source_dir / "ofxOceanodeOSCVariablesController.h").read_text()
    node_header = (source_dir / "oscVariables.h").read_text()
    node_source = (source_dir / "oscVariables.cpp").read_text()

    declarations = header_declarations(controller_header)
    # The group's container, sockets and registry are never touched by the spliced methods.
    declarations = declarations.replace("    ~oscVariablesGroup();\n", "    ~oscVariablesGroup(){}\n", 1)
    source = ADAPTER + declarations + node_class(node_header) + production(controller) + without_includes(node_source) + TESTS

    of = of_root()
    includes = sorted({p.parent for p in (of / "libs/openFrameworks").rglob("*.h")})
    includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    libdir = of / "libs/openFrameworksCompiled/lib/macos"
    frameworks = ["Accelerate", "AppKit", "ApplicationServices", "AudioToolbox",
                  "AVFoundation", "Cocoa", "CoreAudio", "CoreFoundation", "CoreMedia",
                  "CoreServices", "CoreVideo", "IOKit", "OpenGL", "QuartzCore",
                  "Security", "SystemConfiguration"]
    with tempfile.TemporaryDirectory(prefix="oceanode-osc-variables-") as folder:
        folder = Path(folder)
        path = folder / "regression.cpp"
        path.write_text(source)
        command = [os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g", "-w",
                   "-DOF_NO_FMOD", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                   *["-I" + str(p) for p in includes], str(path),
                   str(of / "libs/openFrameworks/types/ofParameter.cpp"),
                   str(of / "libs/openFrameworks/types/ofParameterGroup.cpp"),
                   *[str(p) for p in sorted(libdir.glob("*.a"))], "-Wl,-dead_strip"]
        for framework in frameworks:
            command += ["-framework", framework]
        subprocess.run(command + ["-o", str(folder / "regression")], check=True)
        subprocess.run([str(folder / "regression")], check=True,
                       env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})


if __name__ == "__main__":
    main()
