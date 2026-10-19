#!/usr/bin/env python3
"""Run production canvas history against a small graph host with ASan/UBSan.

Exercises grouping, node identity reuse, incident cables, stored input values,
redo branching, macro backing-file lifetime, per-container ownership and failure
rollback. Native headers are additionally covered by integration_compile.py.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
JSON_INCLUDE = ROOT.parent.parent / "libs/json/include"

FIXTURE = r'''
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
using ofJson = nlohmann::json;
namespace glm { struct vec2 {float x=0,y=0; vec2()=default;vec2(float a,float b):x(a),y(b){} }; }
struct ofFloatColor {float r=0,g=0,b=0,a=1; ofFloatColor()=default; ofFloatColor(float R,float G,float B,float A):r(R),g(G),b(B),a(A){} };
struct ofxOceanodeContainer;
struct ofxOceanodeNode;
struct ofxOceanodeAbstractConnection;
struct Group {string key; string getEscapedName(){return key;}};
struct Gui {
    glm::vec2 position; bool expanded=true,selected=false;
    auto getPosition(){return position;} void setPosition(glm::vec2 p){position=p;}
    bool getExpanded(){return expanded;} void setExpanded(bool e){expanded=e;} void setSelected(bool s){selected=s;}
};
struct ofxOceanodeNodeModel {
    Group group; string type="Value"; int id=0; int custom=17;
    virtual ~ofxOceanodeNodeModel()=default;
    string nodeName(){return type;} int getNumIdentifier(){return id;}
    auto& getParameterGroup(){return group;}
    void presetSave(ofJson& json){json["custom"]=custom;}
    void loadBeforeConnections(ofJson& json){custom=json.at("custom");}
    virtual void macroLoad(ofJson&,string){}
    void presetRecallBeforeSettingParameters(ofJson&){} void presetRecallAfterSettingParameters(ofJson&){}
};
struct abstractRouter : ofxOceanodeNodeModel {
    struct {string value;string get(){return value;}} name;
    auto& getNameParam(){return name;}
    void loadBeforeConnections(ofJson& json){if(json.contains("Name"))name.value=json["Name"].get<string>();}
};
struct ofxOceanodeNodeMacro : ofxOceanodeNodeModel {
    int innerValue=55; string folder;
    shared_ptr<ofxOceanodeContainer> child;
    auto getContainer(){return child;}
    void saveCanvasUndoState(ofJson& json,const string& path){
        ofstream(path+"/inner.json") << innerValue;
        json["macro"]=true;
    }
    void macroLoad(ofJson& json,string path) override {
        assert(json.at("macro")==true);folder=path;
        ifstream file(path+"/inner.json"); assert(file.good());file>>innerValue;
    }
};
struct Parameter {
    ofxOceanodeNodeModel* model; string name="Value"; int value=3;
    ofxOceanodeAbstractConnection* incoming=nullptr;
    auto getNodeModel(){return model;} string getName(){return name;}
};
struct ofxOceanodeNode {
    shared_ptr<void> canvasHistoryStorage;
    unique_ptr<ofxOceanodeNodeModel> model;
    Gui gui; Parameter parameter; bool persistent=false; ofxOceanodeContainer* owner;
    ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> m,ofxOceanodeContainer* c):model(move(m)),parameter{model.get()},owner(c){}
    auto& getParameters(){return model->group;} auto& getNodeModel(){return *model;} auto& getNodeGui(){return gui;}
    bool getIsPersistent(){return persistent;}
    ofJson saveParametersToJson(bool=false,bool before=false);
    void saveInspectorParametersToJson(ofJson& j){j["inspector"]=9;}
    void loadInspectorParametersFromJson(ofJson& j){assert(j.at("inspector")==9);}
    void loadParametersFromJson(ofJson j,bool=false){if(!parameter.incoming)parameter.value=j.at("Value");}
    void presetWillBeLoaded(){} void presetHasLoaded(){} void deleteSelf();
};
struct ofxOceanodeAbstractConnection {
    Parameter *source,*sink; ofxOceanodeContainer* owner; bool active=true,persistent=false; int before;
    ofxOceanodeAbstractConnection(Parameter& a,Parameter& b,ofxOceanodeContainer* c,bool on):source(&a),sink(&b),owner(c),active(on),before(b.value){
        assert(!b.incoming);b.incoming=this;if(active)b.value=a.value;
    }
    ~ofxOceanodeAbstractConnection(){if(sink->incoming==this){sink->incoming=nullptr;sink->value=before;}}
    auto& getSourceParameter(){return *source;} auto& getSinkParameter(){return *sink;}
    bool getActive(){return active;} bool getIsPersistent(){return persistent;} void setIsPersistent(bool p){persistent=p;}
    void deleteSelf();
};
ofJson ofxOceanodeNode::saveParametersToJson(bool,bool before){return {{"Value", before && parameter.incoming?parameter.incoming->before:parameter.value}};}
struct Registry {
    map<string,int> registered{{"Value",1},{"Macro",1}};
    auto& getRegisteredModels(){return registered;}
    unique_ptr<ofxOceanodeNodeModel> create(string name){
        if(!registered.count(name))return nullptr;
        if(name=="Macro"){auto p=make_unique<ofxOceanodeNodeMacro>();p->type=name;return p;}
        return make_unique<ofxOceanodeNodeModel>();
    }
};
struct ofxOceanodeComment {
    string text="-";glm::vec2 position,size;ofFloatColor color,textColor;
};
enum class CustomGuiWidgetType { Slider, Scope };
struct CustomGuiWidget { struct {string parameterPath;} parameterRef; CustomGuiWidgetType type=CustomGuiWidgetType::Slider; };
struct CustomGuiSnapshotBank {string customGuiId;};
struct CustomGuiPanelData {string id; struct {vector<CustomGuiWidget> widgets;} layout;};
ofJson customGuiWidgetToJson(const CustomGuiWidget& widget){return {{"path",widget.parameterRef.parameterPath},{"type",int(widget.type)}};}
CustomGuiWidget customGuiWidgetFromJson(const ofJson& j){CustomGuiWidget w;w.parameterRef.parameterPath=j.at("path");w.type=CustomGuiWidgetType(j.at("type").get<int>());return w;}
ofJson customGuiPanelsToJson(const vector<CustomGuiPanelData>& panels){
    ofJson j={{"panels",ofJson::array()}};
    for(const auto& p:panels){ofJson widgets=ofJson::array();for(const auto& w:p.layout.widgets)widgets.push_back(customGuiWidgetToJson(w));j["panels"].push_back({{"id",p.id},{"widgets",widgets}});}return j;
}
vector<CustomGuiPanelData> customGuiPanelsFromJson(const ofJson& j){
    vector<CustomGuiPanelData> out;for(const auto& p:j.at("panels")){CustomGuiPanelData item;item.id=p.at("id");for(const auto& w:p.at("widgets"))item.layout.widgets.push_back(customGuiWidgetFromJson(w));out.push_back(item);}return out;
}
ofJson customGuiSnapshotBanksToJson(const vector<CustomGuiSnapshotBank>& banks){ofJson j={{"banks",ofJson::array()}};for(const auto& b:banks)j["banks"].push_back({{"customGuiId",b.customGuiId}});return j;}
vector<CustomGuiSnapshotBank> customGuiSnapshotBanksFromJson(const ofJson& j){vector<CustomGuiSnapshotBank> out;for(const auto& b:j.at("banks"))out.push_back({b.at("customGuiId")});return out;}
struct ofxOceanodeShared { static void nodeSelectedInCanvas(void*){} };
struct ofxOceanodeContainer {
    // HISTORY_TYPES
    ofxOceanodeContainer* timelineHost=nullptr;
    shared_ptr<Registry> registry=make_shared<Registry>();
    unordered_map<string,unique_ptr<ofxOceanodeNode>> nodes;
    unordered_map<string,ofxOceanodeNode*> parameterGroupNodesMap;
    vector<unique_ptr<ofxOceanodeAbstractConnection>> connections;
    vector<ofxOceanodeComment> comments;
    struct { void notify(void*){} } allNodesCreated;
    vector<CustomGuiPanelData> customGuiPanelsData;
    vector<CustomGuiSnapshotBank> customGuiSnapshotBanks;
    void rebuildCustomGuiPanels(){} void markCustomGuiSnapshotsDirty(){}
    CustomGuiPanelData* getCustomGuiPanelData(string id){for(auto& p:customGuiPanelsData)if(p.id==id)return &p;return nullptr;}
    void markCustomGuisDirty(){}
    bool rejectCable=false;
    ~ofxOceanodeContainer(){connections.clear();nodes.clear();}
    static bool isClearingContainers(){return false;}
    auto getAllModules(){vector<ofxOceanodeNode*> all;for(auto& n:nodes)all.push_back(n.second.get());return all;}
    ofxOceanodeNode& createNode(unique_ptr<ofxOceanodeNodeModel> model,int id,bool persistent,string setup){
        if(!pendingCanvasEdit && !replayingCanvasEdit)clearCanvasHistory();
        model->id=id;string key=model->type+"_"+to_string(id);model->group.key=key;
        auto node=make_unique<ofxOceanodeNode>(move(model),this);node->persistent=persistent;
        auto* raw=node.get();nodes[key]=move(node);parameterGroupNodesMap[key]=raw;
        canvasLiveNodes[key]={++nextCanvasNodeIdentity,setup};return *raw;
    }
    auto& add(int id=1,string type="Value",bool persistent=false){return createNode(registry->create(type),id,persistent,"setup-data");}
    void eraseNode(ofxOceanodeNode& n){
        const string key=n.getParameters().key;
        captureDeletedCanvasNode(n);canvasLiveNodes.erase(key);
        for(size_t i=0;i<connections.size();) {
            auto* c=connections[i].get();if(c->source==&n.parameter || c->sink==&n.parameter)c->deleteSelf();else ++i;
        }
        parameterGroupNodesMap.erase(key);nodes.erase(key);
    }
    ofxOceanodeAbstractConnection* createConnectionFromInfo(string a,string,string b,string,bool active=true,string* reason=nullptr){
        if(rejectCable){rejectCable=false;if(reason)*reason="injected failure";return nullptr;}
        if(!nodes.count(a)||!nodes.count(b))return nullptr;
        auto c=make_unique<ofxOceanodeAbstractConnection>(nodes.at(a)->parameter,nodes.at(b)->parameter,this,active);
        auto* raw=c.get();connections.push_back(move(c));return raw;
    }
    auto* connect(int a,int b){return createConnectionFromInfo("Value_"+to_string(a),"Value","Value_"+to_string(b),"Value");}
    void beginCanvasEdit();void endCanvasEdit();bool undoCanvasEdit();bool redoCanvasEdit();void clearCanvasHistory();
    bool canUndoCanvasEdit() const;bool canRedoCanvasEdit() const;
    void requestCanvasHistory(bool redo=false);uint64_t getCanvasHistoryRevision() const;
};
void ofxOceanodeContainer::synchronizeCanvasTransfers(CanvasEdit& edit,bool){assert(edit.nodeTransfers.empty());}
void ofxOceanodeNode::deleteSelf(){owner->eraseNode(*this);}
void ofxOceanodeAbstractConnection::deleteSelf(){
    auto& cables=owner->connections;
    cables.erase(find_if(cables.begin(),cables.end(),[this](const auto& c){return c.get()==this;}));
}
template<class T> string ofToString(T x){return to_string(x);}
uint64_t ofGetSystemTimeMicros(){return chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();}
auto& ofLogWarning(const string&){return cerr;}auto& ofLogError(const string&){return cerr;}
'''

CASES = r'''
int main(){
    ofxOceanodeContainer c;
    c.beginCanvasEdit();auto& a=c.add();a.parameter.value=10;c.endCanvasEdit();
    assert(c.canvasUndo.size()==1);
    assert(c.undoCanvasEdit() && c.nodes.empty());
    assert(c.redoCanvasEdit() && c.nodes.at("Value_1")->parameter.value==10);
    // A hundred intermediate drag frames form one entry; no parameter history.
    c.beginCanvasEdit();
    for(int i=0;i<100;++i){c.beginCanvasEdit();c.nodes.at("Value_1")->gui.position={float(i),float(i*2)};}
    c.endCanvasEdit();
    assert(c.canvasUndo.size()==2);
    c.nodes.at("Value_1")->parameter.value=99;
    assert(c.undoCanvasEdit());assert(c.nodes.at("Value_1")->gui.position.x==0);
    assert(c.nodes.at("Value_1")->parameter.value==99);
    assert(c.redoCanvasEdit());assert(c.nodes.at("Value_1")->gui.position.y==198);
    c.beginCanvasEdit();c.endCanvasEdit();assert(c.canvasUndo.size()==2);
    // Paste/duplicate is a single transaction including all internal cables.
    c.beginCanvasEdit();c.add(2);c.add(3);c.connect(1,2);c.connect(2,3);c.endCanvasEdit();
    assert(c.canvasUndo.size()==3 && c.connections.size()==2);
    assert(c.undoCanvasEdit());assert(c.nodes.size()==1 && c.connections.empty());
    assert(c.redoCanvasEdit());assert(c.nodes.size()==3 && c.connections.size()==2);
    // Deletion restores incoming AND outgoing edges and original input values.
    c.beginCanvasEdit();c.nodes.at("Value_2")->deleteSelf();c.endCanvasEdit();
    assert(c.nodes.size()==2 && c.connections.empty());assert(c.undoCanvasEdit());
    assert(c.connections.size()==2);assert(c.nodes.at("Value_2")->model->custom==17);
    assert(c.redoCanvasEdit());assert(c.undoCanvasEdit());
    // Replace an occupied inlet. Disconnect values survive undo/redo.
    c.beginCanvasEdit();c.connections.back()->deleteSelf();c.connect(1,3);c.endCanvasEdit();
    assert(c.undoCanvasEdit());assert(c.nodes.at("Value_3")->parameter.incoming->source->model->id==2);
    assert(c.redoCanvasEdit());assert(c.nodes.at("Value_3")->parameter.incoming->source->model->id==1);
    c.beginCanvasEdit();c.nodes.at("Value_3")->parameter.incoming->deleteSelf();c.endCanvasEdit();
    assert(c.nodes.at("Value_3")->parameter.value==3);
    assert(c.undoCanvasEdit());assert(c.redoCanvasEdit());assert(c.nodes.at("Value_3")->parameter.value==3);
    // Editing after undo drops the alternate future; no-op clicks preserve it.
    assert(c.undoCanvasEdit());c.beginCanvasEdit();c.endCanvasEdit();assert(c.canRedoCanvasEdit());
    c.beginCanvasEdit();c.nodes.at("Value_1")->gui.position={4,5};c.endCanvasEdit();assert(!c.canRedoCanvasEdit());
    // Same name/ID reused in one transaction must recreate the right instance.
    c.beginCanvasEdit();c.nodes.at("Value_3")->deleteSelf();auto& replacement=c.add(3);replacement.parameter.value=123;c.endCanvasEdit();
    assert(c.undoCanvasEdit());assert(c.nodes.at("Value_3")->parameter.incoming);
    assert(c.redoCanvasEdit());assert(c.nodes.at("Value_3")->parameter.value==123);
    // An external change is rejected rather than removing unrelated graph state.
    c.nodes.at("Value_1")->gui.position={50,50};assert(!c.undoCanvasEdit());assert(!c.canUndoCanvasEdit());
    // Histories belong to their own container; memory is bounded by entry count.
    ofxOceanodeContainer other;other.add();
    for(int i=0;i<110;++i){other.beginCanvasEdit();other.nodes.at("Value_1")->gui.position={float(i+1),0};other.endCanvasEdit();}
    assert(other.canvasUndo.size()==100);assert(!c.canUndoCanvasEdit());
    // Macro backing data survives history eviction while the restored node lives.
    string folder;
    {
        ofxOceanodeContainer macros;auto& m=macros.add(1,"Macro");
        static_cast<ofxOceanodeNodeMacro&>(m.getNodeModel()).innerValue=777;
        macros.beginCanvasEdit();m.deleteSelf();macros.endCanvasEdit();assert(macros.undoCanvasEdit());
        auto& restored=static_cast<ofxOceanodeNodeMacro&>(macros.nodes.at("Macro_1")->getNodeModel());
        assert(restored.innerValue==777);folder=restored.folder;
        macros.clearCanvasHistory();assert(filesystem::exists(folder+"/inner.json"));
    }
    assert(!filesystem::exists(folder));
    // Persistent nodes and copied comments restore without leaked pointers.
    ofxOceanodeContainer persistent;persistent.add(1,"Value",true);
    persistent.beginCanvasEdit();persistent.nodes.at("Value_1")->deleteSelf();persistent.comments.push_back({});persistent.endCanvasEdit();
    assert(persistent.undoCanvasEdit());assert(persistent.nodes.at("Value_1")->persistent && persistent.comments.empty());
    // Pruned custom-GUI publications come back with their deleted node.
    ofxOceanodeContainer gui;gui.add();gui.customGuiPanelsData.push_back({"panel",{{{{"Value_1/Value"},CustomGuiWidgetType::Slider}}}});
    gui.beginCanvasEdit();gui.nodes.at("Value_1")->deleteSelf();gui.endCanvasEdit();
    gui.customGuiPanelsData[0].layout.widgets.clear(); // native pruning on the next update
    assert(gui.undoCanvasEdit());assert(gui.customGuiPanelsData[0].layout.widgets.size()==1);
    assert(gui.redoCanvasEdit());assert(gui.undoCanvasEdit());assert(gui.customGuiPanelsData[0].layout.widgets.size()==1);
    // A failed cable restoration rolls back to the pre-undo graph safely.
    ofxOceanodeContainer failure;failure.add(1);failure.add(2);failure.connect(1,2);
    failure.beginCanvasEdit();failure.connections[0]->deleteSelf();failure.endCanvasEdit();failure.rejectCable=true;
    assert(!failure.undoCanvasEdit());assert(failure.connections.empty() && failure.nodes.size()==2);
    // Node factory failure is checked before destructive changes.
    ofxOceanodeContainer missing;missing.beginCanvasEdit();missing.add();missing.endCanvasEdit();assert(missing.undoCanvasEdit());
    missing.registry->registered.clear();assert(!missing.redoCanvasEdit());assert(missing.nodes.empty());
    // Shared chronological order and redo branching across nested canvases.
    ofxOceanodeContainer root;
    auto& outer=root.add(1,"Macro");
    auto& nestedMacro=static_cast<ofxOceanodeNodeMacro&>(outer.getNodeModel());
    nestedMacro.child=make_shared<ofxOceanodeContainer>();auto& nested=*nestedMacro.child;
    nested.timelineHost=&root;nested.add(1);
    root.beginCanvasEdit();outer.gui.position={20,0};root.endCanvasEdit();
    nested.beginCanvasEdit();nested.add(2);nested.endCanvasEdit();
    assert(root.undoCanvasEdit() && nested.nodes.size()==1);
    assert(nested.undoCanvasEdit() && outer.gui.position.x==0);
    assert(nested.redoCanvasEdit() && outer.gui.position.x==20);
    assert(root.redoCanvasEdit() && nested.nodes.size()==2);
    assert(root.undoCanvasEdit());assert(root.canRedoCanvasEdit());
    nested.beginCanvasEdit();nested.nodes.at("Value_1")->gui.position={8,0};nested.endCanvasEdit();
    assert(!root.canRedoCanvasEdit());
    const auto revision=root.getCanvasHistoryRevision();
    nested.requestCanvasHistory();assert(root.requestedCanvasHistory==-1);
    assert(nested.nodes.at("Value_1")->gui.position.x==8); // Deferred until update.
    root.processCanvasHistoryRequest();assert(nested.nodes.at("Value_1")->gui.position.x==0);
    assert(nested.getCanvasHistoryRevision()>revision);
    // Compound auxiliary changes preserve unrelated later panel layout edits.
    ofxOceanodeContainer compound;compound.add();
    compound.customGuiPanelsData={ {"moved",{}}, {"unrelated",{}} };
    {ofxOceanodeContainer::MacroCanvasEditScope edit(compound);compound.add(2);compound.customGuiPanelsData.erase(compound.customGuiPanelsData.begin());edit.success=true;}
    compound.customGuiPanelsData[0].layout.widgets.push_back({{"Value_1/Value"},CustomGuiWidgetType::Scope});
    assert(compound.undoCanvasEdit());assert(compound.customGuiPanelsData.size()==2);
    assert(compound.getCustomGuiPanelData("unrelated")->layout.widgets.size()==1);
    assert(compound.redoCanvasEdit());assert(!compound.getCustomGuiPanelData("moved"));
    cout << "Canvas undo regression passed\n";
}
'''


def main():
    header = (ROOT / "src/Managers/ofxOceanodeContainer.h").read_text()
    types = header[header.index("    struct CanvasNodeSnapshot {"):header.index("    //NodeModel;")]
    source = (ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
    functions = source[source.index("ofxOceanodeContainer::CanvasNodeSnapshot::~CanvasNodeSnapshot()") :source.index("void ofxOceanodeContainer::migrateCanvasPanelsTo(")]
    fixture = FIXTURE.replace("    // HISTORY_TYPES", types)
    canvas = (ROOT / "src/Managers/ofxOceanodeCanvas.cpp").read_text()
    shortcut = canvas[canvas.index("const bool historyShortcut"):canvas.index("auto prepareNewNodeModal")]
    assert "!editIO.WantTextInput" in shortcut and "!ImGui::IsAnyItemActive()" in shortcut
    assert "KeySuper" in canvas and "requestCanvasHistory(editIO.KeyShift)" in shortcut
    assert canvas.index("if(historyShortcut)") < canvas.index("for(auto nodePair : nodesVisibleInThisFrame)")
    with tempfile.TemporaryDirectory(prefix="oceanode-canvas-undo-") as tmp:
        cpp = Path(tmp) / "regression.cpp"
        cpp.write_text(fixture + functions + CASES)
        binary = Path(tmp) / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O1",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wno-unqualified-std-cast-call",
                        "-I" + str(JSON_INCLUDE), str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
