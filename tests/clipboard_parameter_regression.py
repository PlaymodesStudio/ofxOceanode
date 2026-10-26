#!/usr/bin/env python3
"""Exercise production connections and node JSON save/load with isolated OF stubs.

Checks clipboard values before connections, scalar/vector conversions, save flags,
normal preset serialization, and that copying never mutates the live graph.
Uses sibling nlohmann JSON and ASan/UBSan; does not build or run the host app.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
JSON_INCLUDE = ROOT.parent.parent / "libs/json/include"

FIXTURE = r'''
#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
using namespace std;
using ofJson = nlohmann::json;
struct ofEventListener {
    shared_ptr<bool> active;
    void unsubscribe(){if(active) *active=false;}
};
template<class T> struct ofEvent {void notify(){}};
struct ofAbstractParameter {
    virtual ~ofAbstractParameter()=default;
    virtual string getName() const=0;
    string getEscapedName() const {auto n=getName(); replace(n.begin(),n.end(),' ','_'); return n;}
    virtual string valueType() const=0;
    virtual string toString() const=0;
    virtual bool fromString(const string&) {return false;}
    virtual bool isSerializable() const {return true;}
};
template<class T> string valueString(const T& v){ostringstream s; s<<v; return s.str();}
template<class T> string valueString(const vector<T>& v){return ofJson(v).dump();}
template<class T> struct ofParameter : ofAbstractParameter {
    struct State {
        T value{}, min{}, max{}; string name;
        vector<pair<shared_ptr<bool>,function<void(T&)>>> listeners;
    };
    shared_ptr<State> state=make_shared<State>();
    string getName() const override {return state->name;}
    string valueType() const override {return typeid(T).name();}
    string toString() const override {return valueString(state->value);}
    ofParameter& set(string n,const T& v){state->name=n;set(v);return *this;}
    void set(const T& v){state->value=v;for(auto& l:state->listeners) if(*l.first) l.second(state->value);}
    const T& get() const {return state->value;}
    T getMin(){return state->min;} T getMax(){return state->max;}
    void setMin(T v){state->min=v;} void setMax(T v){state->max=v;}
    const T* operator->() const {return &state->value;}
    operator const T&() const {return get();}
    ofParameter& operator=(const T& v){set(v);return *this;}
    ofParameter& operator=(const ofParameter& p){set(p.get());return *this;}
    template<class F> ofEventListener newListener(F f){
        auto active=make_shared<bool>(true);state->listeners.push_back({active,f});return {active};
    }
};
template<> struct ofParameter<void> : ofAbstractParameter {
    string name="Trigger";
    string getName() const override{return name;}
    string valueType() const override{return typeid(void).name();}
    string toString() const override{return "";}
    template<class F> ofEventListener newListener(F){return {};}
    void trigger(){}
};
void ofSerialize(ofJson& j,const ofAbstractParameter& p){if(p.isSerializable()) j[p.getEscapedName()]=p.toString();}
void ofDeserialize(const ofJson&,ofAbstractParameter&){}
float ofToFloat(const string& s){try{return stof(s);}catch(...){return 0;}}
ostream& ofLogWarning(const string&){return cerr;}
class ofxOceanodeAbstractConnection;
constexpr int ofxOceanodeParameterFlags_DisableSavePreset=1;
constexpr int ofxOceanodeParameterFlags_DisableSaveProject=2;
template<class T> class ofxOceanodeParameter;
struct ofxOceanodeAbstractParameter : ofAbstractParameter {
    ofxOceanodeAbstractConnection* in=nullptr; int flags=0;
    vector<ofxOceanodeAbstractConnection*> out;
    ofxOceanodeAbstractParameter* getForwardedSource(){return nullptr;}
    void setInConnection(ofxOceanodeAbstractConnection* c){in=c;}
    bool hasInConnection(){return in!=nullptr;}
    bool allowsMultipleInputs() const {return false;}
    bool refreshCombinedInput(){return false;}
    auto getInConnection(){return in;}
    void removeInConnection(ofxOceanodeAbstractConnection* c){if(in==c) in=nullptr;}
    void addOutConnection(ofxOceanodeAbstractConnection* c){out.push_back(c);}
    void removeOutConnection(ofxOceanodeAbstractConnection* c){out.erase(remove(out.begin(),out.end(),c),out.end());}
    int getFlags(){return flags;}
    void connectedParameter(){} void disconnectedParameter(){}
    void receiveParameter(ofAbstractParameter*){}
    template<class T> ofxOceanodeParameter<T>& cast(){return static_cast<ofxOceanodeParameter<T>&>(*this);}
};
template<class T> struct ofxOceanodeParameter : ofxOceanodeAbstractParameter {
    ofParameter<T> p;
    ofxOceanodeParameter(string n,T v){p.set(n,v);}
    auto& getParameter(){return p;}
    string getName() const override{return p.getName();}
    string valueType() const override{return typeid(T).name();}
    string toString() const override{return p.toString();}
};
template<> struct ofxOceanodeParameter<void> : ofxOceanodeAbstractParameter {
    ofParameter<void> p;
    auto& getParameter(){return p;}
    string getName() const override{return p.getName();}
    string valueType() const override{return typeid(void).name();}
    string toString() const override{return "";}
};
'''

NODE_FIXTURE = r'''
struct ParameterGroup {
    vector<ofxOceanodeAbstractParameter*> entries;
    size_t size(){return entries.size();}
    auto& get(int i){return *entries[i];}
    bool contains(const string& key){for(auto* p:entries) if(p->getEscapedName()==key) return true;return false;}
    auto& get(const string& key){for(auto* p:entries) if(p->getEscapedName()==key) return *p;abort();}
};
struct Model {
    string nodeName(){return "Ranger";} int getNumIdentifier(){return 1;}
    void presetSave(ofJson&){} void macroSave(ofJson&,const string&){}
};
struct Gui {bool getExpanded(){return true;}};
ofJson written;
void ofStringReplace(string& s,const string& from,const string& to){
    size_t p=0;while((p=s.find(from,p))!=string::npos){s.replace(p,from.size(),to);p+=to.size();}
}
string ofToString(int n){return to_string(n);}
namespace oceanodePresetSave {
struct Scope { bool finish(){return true;} };
bool saveJson(const string&,const ofJson& j){written=j;return true;}
void record(bool success){assert(success);}
}
struct ofxOceanodeNode {
    ParameterGroup group; Model model; Gui gui;
    Model* nodeModel=&model;Gui* nodeGui=&gui;
    ParameterGroup& getParameters(){return group;}
    void saveInspectorParametersToJson(ofJson&){}
    void savePreset(string,bool=false);
    ofJson saveParametersToJson(bool=false,bool=false);
    void saveParameterToJson(ofJson&,ofxOceanodeAbstractParameter&,bool=false,bool=false);
    bool loadParametersFromJson(ofJson,bool=false);
    void deserializeParameter(ofJson&,ofxOceanodeAbstractParameter&,bool=false);
};
'''

CASES = r'''
int main(){
    ofxOceanodeParameter<vector<float>> source("Vector",{1,2,3}), sink("Min In",{0.25f});
    ofxOceanodeNode original; original.group.entries={&sink};
    int notifications=0;
    auto listener=sink.p.newListener([&](vector<float>&){++notifications;});
    {
        ofxOceanodeConnection<vector<float>,vector<float>> c(source,sink,true);
        assert(sink.p.get()==vector<float>({1,2,3}));
        source.p=vector<float>({4,5,6,7});
        const auto live=sink.p.get(); const int before=notifications;
        original.savePreset("clipboard",true);
        assert(written["Min_In"]==0.25f);
        assert(sink.p.get()==live && notifications==before && sink.in==&c);
        assert(source.out.size()==1 && source.out[0]==&c);
        // Pasting without the connection restores the edited singleton, not factory 0.
        ofxOceanodeParameter<vector<float>> pasted("Min In",{0});
        ofxOceanodeNode copy;copy.group.entries={&pasted};
        copy.loadParametersFromJson(written);
        assert(pasted.p.get()==vector<float>({0.25f}) && !pasted.hasInConnection());
        // Pasting with a connection still receives its live vector value.
        ofxOceanodeParameter<vector<float>> connected("Min In",{0});
        ofxOceanodeNode linked;linked.group.entries={&connected};
        ofxOceanodeConnection<vector<float>,vector<float>> link(source,connected,true);
        linked.loadParametersFromJson(written);
        assert(connected.p.get()==live);
        // Ordinary saves retain their existing runtime-value format.
        original.savePreset("preset");assert(written["Min_In"]==live);
        sink.flags=ofxOceanodeParameterFlags_DisableSavePreset;
        assert(original.saveParametersToJson(false,true).is_null());
        sink.flags=ofxOceanodeParameterFlags_DisableSaveProject;
        assert(original.saveParametersToJson(true,true).is_null());
        sink.flags=0;
    }
    assert(sink.p.get()==vector<float>({0.25f}) && !sink.hasInConnection());
    // Real vectors edited before connecting must retain their full shape and contents.
    sink.p=vector<float>({8,9});
    {
        ofxOceanodeConnection<vector<float>,vector<float>> c(source,sink,false);
        assert(original.saveParametersToJson(false,true)["Min_In"]==vector<float>({8,9}));
        c.setActive(true);
        assert(original.saveParametersToJson(false,true)["Min_In"]==vector<float>({8,9}));
    }
    sink.p=vector<float>{};
    {
        ofxOceanodeConnection<vector<float>,vector<float>> c(source,sink,true);
        assert(original.saveParametersToJson(false,true)["Min_In"]==ofJson::array());
    }
    ofxOceanodeParameter<vector<int>> ints("Ints",{7});
    ofxOceanodeNode integerNode;integerNode.group.entries={&ints};
    {
        ofxOceanodeConnection<vector<float>,vector<int>> c(source,ints,true);
        assert(integerNode.saveParametersToJson(false,true)["Ints"]==7);
    }
    ofxOceanodeParameter<float> scalar("Scalar",2.5f);
    {
        ofxOceanodeConnection<float,vector<int>> c(scalar,ints,true);
        assert(integerNode.saveParametersToJson(false,true)["Ints"]==7);
    }
    ofxOceanodeNode scalarNode;scalarNode.group.entries={&scalar};
    {
        ofxOceanodeConnection<vector<float>,float> c(source,scalar,true);
        assert(scalarNode.saveParametersToJson(false,true)["Scalar"]=="2.5");
    }
    ofxOceanodeParameter<float> floatSource("Float",12);
    {
        ofxOceanodeConnection<float,float> c(floatSource,scalar,true);
        assert(scalarNode.saveParametersToJson(false,true)["Scalar"]=="2.5");
    }
    ofxOceanodeParameter<string> stringSource("Text","driven"),stringSink("Text","edited");
    ofxOceanodeNode stringNode;stringNode.group.entries={&stringSink};
    {
        ofxOceanodeConnection<string,string> c(stringSource,stringSink,true);
        assert(stringNode.saveParametersToJson(false,true)["Text"]=="edited");
    }
    // Custom and trigger connections have no stored editable value: keep prior behavior.
    {
        ofxOceanodeCustomConnection<float> c(floatSource,sink,true);
        assert(original.saveParametersToJson(false,true)["Min_In"]==ofJson::array());
    }
    ofxOceanodeParameter<void> trigger;
    {
        ofxOceanodeConnection<void,float> c(trigger,scalar,true);
        assert(scalarNode.saveParametersToJson(false,true)["Scalar"]=="2.5");
    }
    // Disconnected manual vectors are copied intact as well.
    sink.p=vector<float>({10,11});
    assert(original.saveParametersToJson(false,true)["Min_In"]==vector<float>({10,11}));
    cout<<"Clipboard parameter regression passed\n";
}
'''


def function(source, marker):
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end] + "\n"


def main():
    node = (ROOT / "src/Nodes/ofxOceanodeNode.cpp").read_text()
    functions = "\n".join(function(node, marker) for marker in [
        "void ofxOceanodeNode::savePreset(",
        "ofJson ofxOceanodeNode::saveParametersToJson(",
        "void ofxOceanodeNode::saveParameterToJson(",
        "bool ofxOceanodeNode::loadParametersFromJson(",
        "template<typename T>\nstatic bool oceanodeJsonToVector(",
        "void ofxOceanodeNode::deserializeParameter(",
    ])
    container = (ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
    clipboard_save = function(container, "void ofxOceanodeContainer::saveClipboardModulesAndConnections(")
    assert "node->savePreset(presetFolderPath, true);" in clipboard_save
    with tempfile.TemporaryDirectory(prefix="oceanode-clipboard-") as tmp:
        folder = Path(tmp)
        (folder / "ofMain.h").write_text(FIXTURE)
        (folder / "ofxOceanodeParameter.h").write_text('#pragma once\n#include "ofMain.h"\n')
        (folder / "ofxOceanodeScheduling.h").write_text('#pragma once\nnamespace ofxOceanodeScheduling { inline bool isScheduledSendActive(){ return false; } }\n')
        cpp = folder / "regression.cpp"
        cpp.write_text('#include "ofxOceanodeConnection.h"\n' + NODE_FIXTURE + functions + CASES)
        binary = folder / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O1",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I" + tmp, "-I" + str(JSON_INCLUDE),
                        "-I" + str(ROOT / "src/Connections"), str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
