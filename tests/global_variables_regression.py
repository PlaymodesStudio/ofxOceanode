#!/usr/bin/env python3
"""Run production Global Variables groups and nodes with real OF parameters.

The group methods, type conversion and the Globals node are spliced out of
src/ and run with ASan/UBSan against a small node-model, parameter-wrapper and
connection adapter. Covers table order, rename, retype, reordering and name
validation.
No GUI or container is launched.
"""
import os
from pathlib import Path
import subprocess
import tempfile

from of_environment import ROOT, of_root
from new_preset_regression import block

CONTROLLER = (ROOT / "src/Controls/ofxOceanodeGlobalVariablesController.cpp").read_text()
CONTROLLER_HEADER = (ROOT / "src/Controls/ofxOceanodeGlobalVariablesController.h").read_text()
NODE_HEADER = (ROOT / "src/Nodes/Default_Nodes/globalVariables.h").read_text()


def header_declarations():
    start = CONTROLLER_HEADER.index("// Variable types")
    end = CONTROLLER_HEADER.index("class ofxOceanodeGlobalVariablesController")
    return CONTROLLER_HEADER[start:end]


def node_class():
    start = NODE_HEADER.index("class globalVariables : public ofxOceanodeNodeModel")
    return NODE_HEADER[start:NODE_HEADER.rindex("#endif")]


def production():
    parts = [block(CONTROLLER, "namespace {")]
    for signature in (
        "const char* globalVariableTypeName(int type)",
        "int globalVariableTypeOf(const ofAbstractParameter& parameter)",
        "std::shared_ptr<ofAbstractParameter> createGlobalVariable(int type",
        "void globalVariables::replaceVariable(",
        "void globalVariables::sortVariables(",
        "void globalVariablesGroup::addNode(",
        "void globalVariablesGroup::removeNode(",
        "void globalVariablesGroup::addParameter(",
        "void globalVariablesGroup::addFloatParameter(",
        "void globalVariablesGroup::addIntParameter(",
        "void globalVariablesGroup::addBoolParameter(",
        "void globalVariablesGroup::addStringParameter(",
        "void globalVariablesGroup::addOfColorParameter(",
        "void globalVariablesGroup::addOfFloatColorParameter(",
        "void globalVariablesGroup::removeParameter(",
        "bool globalVariablesGroup::isParameterNameAvailable(",
        "bool globalVariablesGroup::renameParameter(",
        "bool globalVariablesGroup::setParameterType(",
        "bool globalVariablesGroup::moveParameter(",
    ):
        parts.append(block(CONTROLLER, signature))
    return "\n\n".join(parts)


ADAPTER = r'''
#include "ofMain.h"
#include <cassert>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
using namespace std;

enum { ofxOceanodeParameterFlags_ReadOnly = 1, ofxOceanodeParameterFlags_DisableSavePreset = 2 };
class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;

struct ofxOceanodeAbstractConnection {
    ofxOceanodeAbstractParameter* source;
    ofxOceanodeAbstractParameter* sink;
    bool persistent = false;
    ofxOceanodeAbstractParameter& getSinkParameter(){ return *sink; }
    bool getIsPersistent(){ return persistent; }
    void setIsPersistent(bool p){ persistent = p; }
};

class ofxOceanodeAbstractParameter : public ofAbstractParameter {
public:
    vector<ofxOceanodeAbstractConnection*> outConnections;
    vector<ofxOceanodeAbstractConnection*> getOutConnections(){ return outConnections; }
    void removeAllConnections(){
        for(auto* c : outConnections) delete c;
        outConnections.clear();
    }
};

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
    shared_ptr<ofAbstractParameter> newReference() const override { return make_shared<oceanodeParameter<T>>(parameter); }
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
        auto numeric = [](const string& t){ return t == typeid(float).name() || t == typeid(int).name() || t == typeid(bool).name(); };
        if(source.valueType() != sink.valueType() && !(numeric(source.valueType()) && numeric(sink.valueType()))) return nullptr;
        auto* connection = new ofxOceanodeAbstractConnection{&source, &sink};
        source.outConnections.push_back(connection);
        return connection;
    }
};

#define ofxOceanodeNodeModel_h
class ofxOceanodeNodeModel {
public:
    ofxOceanodeNodeModel(string name){ parameters.setName(name); }
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup(){}
    ofParameterGroup& getParameterGroup(){ return parameters; }
    ofxOceanodeContainer* getHostContainer() const { return host; }
    template<typename T>
    void addOutputParameter(shared_ptr<ofParameter<T>> p, int){ parameters.add(*make_shared<oceanodeParameter<T>>(p)); }
    bool removeParameter(string name){ parameters.remove(name); return true; }
    ofEvent<void> parameterGroupChanged;
    ofxOceanodeContainer* host = nullptr;
    int groupChanges = 0;
protected:
    string description;
private:
    ofParameterGroup parameters;
};

struct globalVariablesGroup;
class globalVariables;
'''

TESTS = r'''
void expect(bool ok, const string& what){
    if(!ok){ cerr << "FAILED: " << what << endl; abort(); }
}
ofxOceanodeAbstractParameter& pin(globalVariables& node, const string& name){
    return static_cast<ofxOceanodeAbstractParameter&>(node.getParameterGroup().get(name));
}
// Pins wrap the variable, so read them through the wrapper, not ofAbstractParameter::cast.
template<typename T> T value(globalVariables& node, const string& name){
    auto* wrapper = dynamic_cast<oceanodeParameter<T>*>(&pin(node, name));
    expect(wrapper != nullptr, "pin " + name + " has the expected type");
    return wrapper->parameter->get();
}
vector<string> pins(globalVariables& node){
    vector<string> names;
    for(int i = 0; i < node.getParameterGroup().size(); i++) names.push_back(node.getParameterGroup().get(i).getName());
    return names;
}
vector<string> rows(globalVariablesGroup& group){
    vector<string> names;
    for(auto& p : group.parameters) names.push_back(p->getName());
    return names;
}
int main(){
    ofxOceanodeContainer host;
    auto group = make_shared<globalVariablesGroup>();
    group->name = "Scene";

    // A node built after the variables lists them in table order, not grouped by type.
    group->addOfColorParameter("Tint", ofColor(255, 0, 0, 128));
    group->addFloatParameter("Speed", 2.5f);
    group->addOfFloatColorParameter("Wash", ofFloatColor(0.25f, 0.5f, 0.75f, 1.0f));
    group->addIntParameter("Count", 3);
    globalVariables node("Scene", group);
    node.host = &host;
    node.setup();
    expect(pins(node) == vector<string>({"Tint", "Speed", "Wash", "Count"}), "pins follow table order");
    expect(pin(node, "Wash").valueType() == typeid(ofFloatColor).name(), "ofFloatColor variable");

    // Values follow the group parameter.
    group->parameters[2]->cast<ofFloatColor>() = ofFloatColor(1, 0, 0, 0.5f);
    expect(value<ofFloatColor>(node, "Wash") == ofFloatColor(1, 0, 0, 0.5f), "ofFloatColor value forwarded");

    // Rename keeps the connection and the pin is found by its new name.
    ofParameter<float> sinkValue("In", 0);
    oceanodeParameter<float> sink(make_shared<ofParameter<float>>(sinkValue));
    host.createConnection(pin(node, "Speed"), sink);
    expect(group->renameParameter("Speed", "Rate"), "rename variable");
    expect(rows(*group) == vector<string>({"Tint", "Rate", "Wash", "Count"}), "table renamed in place");
    expect(pins(node) == vector<string>({"Tint", "Rate", "Wash", "Count"}), "pin renamed in place");
    expect(node.getParameterGroup().contains("Rate") && !node.getParameterGroup().contains("Speed"), "renamed pin lookup");
    expect(pin(node, "Rate").getOutConnections().size() == 1, "rename keeps connection");
    group->parameters[1]->cast<float>() = 7.0f;
    expect(value<float>(node, "Rate") == 7.0f, "renamed variable still forwards");

    // Names are compared escaped, as node parameters are.
    expect(!group->renameParameter("Rate", "Count"), "duplicate name rejected");
    group->addFloatParameter("My Var", 1);
    expect(!group->isParameterNameAvailable("My_Var"), "escaped duplicate rejected");
    expect(!group->renameParameter("Rate", "My_Var"), "escaped duplicate rename rejected");
    expect(!group->renameParameter("Rate", ""), "empty name rejected");
    expect(group->renameParameter("Rate", "Rate"), "same name is a no-op");
    expect(!group->isParameterNameAvailable(""), "empty name unavailable");

    // Retype keeps the row and pin position and converts the value.
    expect(group->setParameterType("Rate", globalVariableType_Int), "retype float to int");
    expect(rows(*group) == vector<string>({"Tint", "Rate", "Wash", "Count", "My Var"}), "retyped row keeps place");
    expect(pins(node) == vector<string>({"Tint", "Rate", "Wash", "Count", "My Var"}), "retyped pin keeps place");
    expect(group->parameters[1]->valueType() == typeid(int).name(), "row is int");
    expect(value<int>(node, "Rate") == 7, "float value converted to int");
    expect(pin(node, "Rate").getOutConnections().size() == 1, "numeric connection restored after retype");
    group->parameters[1]->cast<int>() = 9;
    expect(value<int>(node, "Rate") == 9, "retyped variable forwards");

    // A type the sink can't take drops the connection.
    expect(group->setParameterType("Rate", globalVariableType_String), "retype int to string");
    expect(value<string>(node, "Rate") == "9", "int value converted to text");
    expect(pin(node, "Rate").getOutConnections().empty(), "incompatible connection dropped");
    expect(group->setParameterType("Rate", globalVariableType_Float), "retype string to float");
    expect(value<float>(node, "Rate") == 9.0f, "text value converted to float");

    // Colors convert to each other.
    expect(group->setParameterType("Tint", globalVariableType_FloatColor), "retype ofColor to ofFloatColor");
    const ofFloatColor tint = value<ofFloatColor>(node, "Tint");
    expect(fabs(tint.r - 1.0f) < 0.01f && fabs(tint.a - 128 / 255.0f) < 0.01f, "ofColor converted to ofFloatColor");
    expect(group->setParameterType("Wash", globalVariableType_Color), "retype ofFloatColor to ofColor");
    expect(value<ofColor>(node, "Wash") == ofColor(255, 0, 0, 127) ||
           value<ofColor>(node, "Wash") == ofColor(255, 0, 0, 128), "ofFloatColor converted to ofColor");
    expect(pins(node) == vector<string>({"Tint", "Rate", "Wash", "Count", "My Var"}), "order after color retypes");
    expect(createGlobalVariable(globalVariableType_Int, "Big", group->parameters[1].get()) != nullptr, "conversion helper");
    ofParameter<float> huge("Huge", FLT_MAX);
    expect(createGlobalVariable(globalVariableType_Int, "Huge", &huge)->cast<int>().get() == INT_MAX, "float clamped into int range");

    // A second node joining later gets the current names, types and order.
    globalVariables late("Scene", group);
    late.host = &host;
    late.setup();
    expect(pins(late) == pins(node), "late node matches table");

    // Removing a variable removes it from every node.
    group->removeParameter("Count");
    expect(rows(*group) == vector<string>({"Tint", "Rate", "Wash", "My Var"}), "row removed");
    expect(pins(node) == rows(*group) && pins(late) == rows(*group), "pin removed from all nodes");
    group->parameters[1]->cast<float>() = 3.0f;
    expect(value<float>(late, "Rate") == 3.0f, "remaining variables still forward");

    // Moving a row reorders every node's pins and keeps their connections.
    host.createConnection(pin(node, "Rate"), sink);
    expect(group->moveParameter("My Var", 0), "move last row to the top");
    expect(rows(*group) == vector<string>({"My Var", "Tint", "Rate", "Wash"}), "row moved up");
    expect(pins(node) == rows(*group) && pins(late) == rows(*group), "pins follow moved row");
    expect(group->moveParameter("Tint", 2), "move a row down");
    expect(rows(*group) == vector<string>({"My Var", "Rate", "Tint", "Wash"}), "row moved down");
    expect(pins(node) == rows(*group) && pins(late) == rows(*group), "pins follow row moved down");
    expect(!group->moveParameter("Tint", 2), "moving to the same row changes nothing");
    expect(group->moveParameter("My Var", 99), "index past the end moves to the last row");
    expect(rows(*group) == vector<string>({"Rate", "Tint", "Wash", "My Var"}), "row moved to the end");
    expect(pins(node) == rows(*group), "pins follow row moved to the end");
    expect(!group->moveParameter("Missing", 0), "unknown variable not moved");
    expect(pin(node, "Rate").getOutConnections().size() == 1, "moved pin keeps connection");
    group->parameters[0]->cast<float>() = 4.0f;
    expect(value<float>(node, "Rate") == 4.0f && value<float>(late, "Rate") == 4.0f, "moved variable still forwards");
    expect(group->setParameterType("Wash", globalVariableType_FloatColor), "retype after a move");
    expect(pins(node) == rows(*group), "retype after a move keeps the order");
    globalVariables third("Scene", group);
    third.host = &host;
    third.setup();
    expect(pins(third) == rows(*group), "node created after a move uses the new order");

    cout << "Global Variables regression passed: table order, ofFloatColor, rename, retype, reorder, conversion, validation and removal.\n";
}
'''


def main():
    declarations = header_declarations()
    # The group's container is never touched by the spliced methods.
    source = ADAPTER + declarations + node_class() + production() + TESTS
    source = source.replace("struct globalVariablesGroup : std::enable_shared_from_this<globalVariablesGroup> {",
                            "struct globalVariablesGroup : std::enable_shared_from_this<globalVariablesGroup> {\n    globalVariablesGroup(){}\n    ~globalVariablesGroup(){}", 1)
    source = source.replace("    globalVariablesGroup();\n", "", 1)
    source = source.replace("    ~globalVariablesGroup();\n", "", 1)
    source = source.replace("    globalVariablesGroup(string _name, shared_ptr<ofxOceanodeContainer> _container);\n", "", 1)

    of = of_root()
    includes = sorted({p.parent for p in (of / "libs/openFrameworks").rglob("*.h")})
    includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    libdir = of / "libs/openFrameworksCompiled/lib/macos"
    frameworks = ["Accelerate", "AppKit", "ApplicationServices", "AudioToolbox",
                  "AVFoundation", "Cocoa", "CoreAudio", "CoreFoundation", "CoreMedia",
                  "CoreServices", "CoreVideo", "IOKit", "OpenGL", "QuartzCore",
                  "Security", "SystemConfiguration"]
    with tempfile.TemporaryDirectory(prefix="oceanode-global-variables-") as folder:
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
