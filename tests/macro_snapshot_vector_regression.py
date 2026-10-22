#!/usr/bin/env python3
"""Test production Macro Snapshot capture, disk persistence, and vector recall.

Reuses isolated OF parameter/event stubs from the clipboard regression and real
nlohmann JSON. Runs MacroSnapshotSystem and MacroRouterValueDispatch production
implementations with filesystem adapters. No host UI or morph timing is run.
"""
from pathlib import Path
import os
import subprocess
import tempfile

from clipboard_parameter_regression import FIXTURE, NODE_FIXTURE, ROOT, JSON_INCLUDE, function

SUPPORT = r'''
#include <cmath>
#include <filesystem>
#include <fstream>
struct ofColor {int r=0,g=0,b=0,a=255;};
struct ofFloatColor {float r=0,g=0,b=0,a=1;};
ostream& operator<<(ostream& s,const ofColor& c){return s<<c.r;}
ostream& operator<<(ostream& s,const ofFloatColor& c){return s<<c.r;}
struct QuietLog {template<class T> QuietLog& operator<<(const T&){return *this;}};
QuietLog ofLogNotice(const string&){return {};}
QuietLog ofLogVerbose(const string&){return {};}
QuietLog ofLogError(const string&){return {};}
int ofToInt(const string& s){return stoi(s);}
// The production snapshot hash also calls ofToString(size_t).
template<class T> string ofToString(T n){return to_string(n);}
struct ofFilePath {
    static string removeTrailingSlash(string p){while(!p.empty() && p.back()=='/') p.pop_back();return p;}
    static string getFileName(const string& p){return filesystem::path(p).filename().string();}
};
struct ofFile {
    static bool doesFileExist(const string& p){return filesystem::exists(p);}
    static bool removeFile(const string& p){return filesystem::remove(p);}
};
struct ofDirectory {
    static void createDirectory(const string& p,bool,bool){filesystem::create_directories(p);}
};
int broadcasts=0;
struct ofxOceanodeShared {static void snapshotUpdated(const string&){++broadcasts;}};
ofJson ofLoadJson(const string& path){ifstream f(path);ofJson j;f>>j;return j;}
void ofSavePrettyJson(const string& path,const ofJson& j){
    written=j;ofstream f(path);assert(f);f<<j.dump(4);assert(f);
}
namespace oceanodePresetSave {
bool saveJson(const string& path,const ofJson& j){
    filesystem::create_directories(filesystem::path(path).parent_path());
    ofSavePrettyJson(path,j);return true;
}
}
struct abstractRouter : Model {
    bool excluded=false;
    bool isExcludeFromSnapshot(){return excluded;}
};
'''

CASES = r'''
template<class T> void checkVector(const string& base,const vector<T>& expected){
    ofxOceanodeParameter<vector<T>> param("Value",expected);
    ofxOceanodeNode node;abstractRouter router;
    node.nodeModel=&router;node.group.entries={&param};
    map<string,RouterInfo> routers;
    routers["Vector"]={true,"Vector",typeid(vector<T>).name(),&node};
    MacroSnapshotSystem snapshots;
    snapshots.storeSnapshot(3,routers,250.f,0.75f);
    const auto* stored=snapshots.getSnapshot(3);
    assert(stored && stored->routerValues.at("Vector").value.is_array());
    assert(stored->routerValues.at("Vector").value.size()==expected.size());
    snapshots.renameSnapshot(3,"Vector snapshot");
    // Local and global branches of the actual save() implementation.
    for(bool local:{true,false}){
        const string path=base+(local?"/local":"/global");
        filesystem::create_directories(path);
        snapshots.save(local,path,path,"Macro",1);
        const ofJson disk=ofLoadJson(path+"/snapshots.json");
        assert(disk["3"]["routerValues"]["Vector"]["value"].is_array());
        assert(disk["3"]["routerValues"]["Vector"]["value"].size()==expected.size());
        MacroSnapshotSystem reloaded;
        reloaded.loadFromPath(path);
        const auto* recalled=reloaded.getSnapshot(3);
        assert(recalled && recalled->name=="Vector snapshot");
        assert(recalled->morphTimeMs==250.f && recalled->morphBiPow==0.75f);
        param.p=vector<T>{};
        MacroRouterValueDispatch::applyValue(&param,recalled->routerValues.at("Vector"));
        assert(param.p.get()==expected);
        // Legacy snapshots embedded in a local Macro preset use this same JSON.
        MacroSnapshotSystem embedded;
        embedded.loadFromJson(disk);
        param.p=vector<T>{};
        MacroRouterValueDispatch::applyValue(&param,embedded.getSnapshot(3)->routerValues.at("Vector"));
        assert(param.p.get()==expected);
    }
}

int main(int argc,char** argv){
    assert(argc==2);string base=argv[1];
    checkVector<float>(base+"/float",{0.125f,-2.5f,4.25f,9.f});
    checkVector<int>(base+"/int",{-10,0,3,200});
    checkVector<bool>(base+"/bool",{true,false,true,false});
    checkVector<string>(base+"/string",{"one","two","","four"});
    checkVector<float>(base+"/single-float",{0.5f});
    checkVector<int>(base+"/single-int",{7});
    checkVector<float>(base+"/empty-float",{});
    checkVector<int>(base+"/empty-int",{});

    // Snapshot capture must keep the live connected vector, even immediately
    // after clipboard serialization deliberately saves the editable singleton.
    ofxOceanodeParameter<vector<float>> source("Source",{0.1f,0.4f,0.8f}),value("Value",{0});
    ofxOceanodeNode node;abstractRouter router;
    node.nodeModel=&router;node.group.entries={&value};
    map<string,RouterInfo> routers={{"Input",{true,"Input",typeid(vector<float>).name(),&node}}};
    ofxOceanodeConnection<vector<float>,vector<float>> connection(source,value,true);
    const auto live=source.p.get();
    assert(node.saveParametersToJson(false,true)["Value"]==0);
    MacroSnapshotSystem snapshots;
    snapshots.storeSnapshot(0,routers,0,0);
    assert(snapshots.getSnapshot(0)->routerValues.at("Input").value==live);
    assert(value.p.get()==live && value.in==&connection);
    assert(node.saveParametersToJson()["Value"]==live);
    const string path=base+"/connected";
    snapshots.save(true,"",path,"Macro",1);
    MacroSnapshotSystem reloaded;
    reloaded.loadFromPath(path+"/snapshots.json");
    ofxOceanodeParameter<vector<float>> recalled("Value",{0});
    MacroRouterValueDispatch::applyValue(&recalled,reloaded.getSnapshot(0)->routerValues.at("Input"));
    assert(recalled.p.get()==live);
    const auto morphStart=MacroRouterValueDispatch::captureForInterpolation(&value);
    assert(morphStart.value==live);
    router.excluded=true;
    snapshots.storeSnapshot(1,routers,0,0);
    assert(snapshots.getSnapshot(1)->routerValues.empty());
    router.excluded=false;routers.at("Input").isInput=false;
    snapshots.storeSnapshot(2,routers,0,0);
    assert(snapshots.getSnapshot(2)->routerValues.empty());
    assert(broadcasts==8);
    cout<<"Macro Snapshot vector regression passed (local/global disk round trips, embedded JSON, clipboard isolation)\n";
}
'''


def implementation(path):
    return "\n".join(line for line in path.read_text().splitlines()
                     if not line.startswith("#include"))


def main():
    node_source = (ROOT / "src/Nodes/ofxOceanodeNode.cpp").read_text()
    node_functions = "\n".join(function(node_source, marker) for marker in [
        "ofJson ofxOceanodeNode::saveParametersToJson(",
        "void ofxOceanodeNode::saveParameterToJson(",
    ])
    node_fixture = NODE_FIXTURE.replace("struct Model {", "struct Model { virtual ~Model()=default;")
    node_fixture = node_fixture.replace(
        "ParameterGroup& getParameters(){return group;}",
        "ParameterGroup& getParameters(){return group;} Model& getNodeModel(){return *nodeModel;}")
    node_fixture = node_fixture.replace(
        "void ofSavePrettyJson(const string&,const ofJson& j){written=j;}", "")
    # Snapshot persistence must use the filesystem adapter, not the clipboard's
    # in-memory-only saveJson stub.
    node_fixture = node_fixture.replace(
        "bool saveJson(const string&,const ofJson& j){written=j;return true;}",
        "bool saveJson(const string&,const ofJson&);")
    macro_header = (ROOT / "src/Nodes/ofxOceanodeNodeMacro.h").read_text()
    router_info = macro_header[macro_header.index("struct RouterInfo {"):]
    router_info = router_info[:router_info.index("};") + 2]
    with tempfile.TemporaryDirectory(prefix="oceanode-snapshot-") as tmp:
        folder = Path(tmp)
        (folder / "ofMain.h").write_text(FIXTURE)
        (folder / "ofJson.h").write_text('#pragma once\n#include "ofMain.h"\n')
        (folder / "ofxOceanodeParameter.h").write_text('#pragma once\n#include "ofMain.h"\n')
        cpp = folder / "regression.cpp"
        cpp.write_text('#include "ofxOceanodeConnection.h"\n'
                       '#include "MacroSnapshotSystem.h"\n'
                       '#include "MacroRouterValueDispatch.h"\n'
                       + node_fixture + SUPPORT + router_info + "\n" + node_functions
                       + implementation(ROOT / "src/Nodes/MacroRouterValueDispatch.cpp")
                       + implementation(ROOT / "src/Nodes/MacroSnapshotSystem.cpp") + CASES)
        binary = folder / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O1",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-Wno-inconsistent-missing-override", "-Wno-self-assign-field",
                        "-I" + tmp, "-I" + str(JSON_INCLUDE),
                        "-I" + str(ROOT / "src/Connections"),
                        "-I" + str(ROOT / "src/Nodes"), str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(folder / "data")], check=True)


if __name__ == "__main__":
    main()
