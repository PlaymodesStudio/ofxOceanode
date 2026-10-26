#!/usr/bin/env python3
"""Exercise production cleanup analysis/deletion with isolated host services.

ASan/UBSan cover disconnected chains/cycles, host bindings, unknown effects,
canvas ownership, stale previews, native deletion callbacks and comment pruning.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std;
struct ofxOceanodeContainer;
struct ofxOceanodeNodeModel;
struct ofxOceanodeAbstractConnection;
struct Abstract { virtual ~Abstract()=default; };
struct ofxOceanodeAbstractParameter : Abstract {
    ofxOceanodeNodeModel* model=nullptr; string nodeName;
    bool scoped=false,timelined=false;
    ofxOceanodeAbstractConnection* incoming=nullptr;
    vector<ofxOceanodeAbstractConnection*> outgoing;
    auto getNodeModel(){return model;}
    bool isScoped(){return scoped;} bool isTimelined(){return timelined;}
    string getName(){return "Value";} string getEscapedName(){return "Value";}
    vector<string> getGroupHierarchyNames(){return {nodeName};}
    auto getOutConnections(){return outgoing;} auto getInConnection(){return incoming;}
    vector<ofxOceanodeAbstractConnection*> getInConnections(){return incoming ? vector<ofxOceanodeAbstractConnection*>{incoming} : vector<ofxOceanodeAbstractConnection*>{};}
};
struct Parameters : vector<shared_ptr<Abstract>> {string name; string getEscapedName(){return name;}};
struct ofxOceanodeNodeModel {
    virtual ~ofxOceanodeNodeModel()=default; bool safe=true;
    bool canRemoveWhenUnused(){return safe;}
};
struct ofxOceanodeNodeMacro : ofxOceanodeNodeModel {
    shared_ptr<ofxOceanodeContainer> child;
    auto getContainer(){return child;}
};
struct ofxOceanodeNode {
    ofxOceanodeContainer* owner=nullptr; bool persistent=false;
    unique_ptr<ofxOceanodeNodeModel> model=make_unique<ofxOceanodeNodeModel>(); Parameters parameters;
    bool getIsPersistent(){return persistent;} auto& getNodeModel(){return *model;}
    auto& getParameters(){return parameters;} void deleteSelf();
    ofxOceanodeAbstractParameter& value(){return *static_cast<ofxOceanodeAbstractParameter*>(parameters[0].get());}
};
struct ofxOceanodeAbstractConnection {
    ofxOceanodeAbstractParameter *source,*sink;
    auto& getSourceParameter(){return *source;} auto& getSinkParameter(){return *sink;}
    ~ofxOceanodeAbstractConnection(){
        auto& v=source->outgoing; v.erase(remove(v.begin(),v.end(),this),v.end());
        if(sink->incoming==this)sink->incoming=nullptr;
    }
};
struct Widget {struct {string parameterPath;} parameterRef;};
struct Panel {struct {vector<Widget> widgets;} layout;};
struct Snapshot {map<string,int> parameterValues;};
struct Bank {vector<Snapshot> snapshots;};
struct Binding {string parameterPath;};
struct Track {vector<Binding> bindings;};
struct Timeline {
    vector<Track> tracks;
    unordered_map<string,ofxOceanodeAbstractParameter*>* paths=nullptr;
    void collectBoundParameters(unordered_set<ofxOceanodeAbstractParameter*>& out) {
        for(const auto& track:tracks)for(const auto& binding:track.bindings){
            auto it=paths->find(binding.parameterPath);if(it!=paths->end())out.insert(it->second);
        }
    }
};
struct Comment {vector<ofxOceanodeNode*> nodes;};
struct ofxOceanodeContainer {
    // PLAN
    vector<unique_ptr<ofxOceanodeNode>> nodes;
    vector<unique_ptr<ofxOceanodeAbstractConnection>> connections;
    unordered_map<string,ofxOceanodeNode*> parameterGroupNodesMap;
    unordered_map<string,ofxOceanodeAbstractParameter*> paths;
    vector<Panel> customGuiPanelsData; vector<Bank> customGuiSnapshotBanks;
    map<string,vector<int>> midiBindings,persistentMidiBindings;
    vector<Comment> comments;
    ofxOceanodeContainer* timelineHost=nullptr; Timeline timeline;
    Timeline* getAutomationExtension(){timeline.paths=&paths;return &timeline;}
    auto* getCustomGuiExtension(){return this;}
    void collectBoundParameters(unordered_set<ofxOceanodeAbstractParameter*>& out){
        for(const auto& p:customGuiPanelsData)for(const auto& w:p.layout.widgets)
            if(auto* target=findCustomGuiParameter(w.parameterRef.parameterPath))out.insert(target);
        for(const auto& b:customGuiSnapshotBanks)for(const auto& s:b.snapshots)for(const auto& v:s.parameterValues)
            if(auto* target=findCustomGuiParameter(v.first))out.insert(target);
    }
    uint64_t nodesRevision=0; int deletions=0;
    ~ofxOceanodeContainer(){connections.clear();}
    auto getAllModules(){vector<ofxOceanodeNode*> out;for(auto& n:nodes)out.push_back(n.get());return out;}
    ofxOceanodeAbstractParameter* findCustomGuiParameter(const string& p){auto i=paths.find(p);return i==paths.end()?nullptr:i->second;}
    auto findTimelineParameter(const string& p){return findCustomGuiParameter(p);}
    auto& getTimelineManager(){return timeline;}
    UnusedNodePlan findUnusedNodes(); bool removeUnusedNodes(const UnusedNodePlan& plan);
    ofxOceanodeNode& add(string name,bool safe=true){
        auto n=make_unique<ofxOceanodeNode>();n->owner=this;n->model->safe=safe;n->parameters.name=name;
        auto p=make_shared<ofxOceanodeAbstractParameter>();p->model=n->model.get();p->nodeName=name;
        n->parameters.push_back(p);paths[name+"/Value"]=p.get();auto* raw=n.get();nodes.push_back(std::move(n));
        parameterGroupNodesMap[name]=raw;++nodesRevision;return *raw;
    }
    void connect(ofxOceanodeNode& a,ofxOceanodeNode& b){
        assert(!b.value().incoming);
        auto link=make_unique<ofxOceanodeAbstractConnection>();link->source=&a.value();link->sink=&b.value();
        a.value().outgoing.push_back(link.get());b.value().incoming=link.get();connections.push_back(std::move(link));
    }
};
void ofxOceanodeNode::deleteSelf(){
    auto& links=owner->connections;
    links.erase(remove_if(links.begin(),links.end(),[&](const auto& c){return c->source==&value()||c->sink==&value();}),links.end());
    owner->paths.erase(parameters.name+"/Value");owner->parameterGroupNodesMap.erase(parameters.name);
    ++owner->nodesRevision;++owner->deletions;
    auto& nodes=owner->nodes;nodes.erase(find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.get()==this;}));
}
'''
CASES = r'''
int main(){
    {
        ofxOceanodeContainer c;
        auto& a=c.add("A");auto& b=c.add("B");auto& d=c.add("D");c.add("Lone");
        c.connect(a,b);c.connect(b,d);c.connect(d,a);c.comments.push_back({{&a,&b,&d}});
        auto plan=c.findUnusedNodes();assert(plan.groups.size()==2&&plan.connectionCount==3&&plan.preservedNodes==0);
        assert(c.removeUnusedNodes(plan));assert(c.nodes.empty()&&c.connections.empty()&&c.deletions==4&&c.comments[0].nodes.empty());
    }
    for(int usage=0;usage<9;++usage){
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");c.connect(a,b);
        switch(usage){
            case 0:a.model->safe=false;break;
            case 1:a.persistent=true;break;
            case 2:a.value().scoped=true;break;
            case 3:a.value().timelined=true;break;
            case 4:c.customGuiPanelsData.push_back({{{{{"A/Value"}}}}});break;
            case 5:c.customGuiSnapshotBanks.push_back({{{{{"A/Value",1}}}}});break;
            case 6:c.timeline.tracks.push_back({{{"A/Value"}}});break;
            case 7:c.midiBindings["A-|-Value"]={1};break;
            case 8:c.persistentMidiBindings["A-|-Value"]={1};break;
        }
        auto plan=c.findUnusedNodes();assert(plan.groups.empty()&&plan.preservedNodes==2);
        assert(plan.protectedGroups.size()==1&&!plan.protectedGroups[0].reasons.empty());
        assert(plan.protectedGroups[0].possiblyUnused==(usage==0));
        assert(c.removeUnusedNodes(plan));assert(c.nodes.size()==2);
    }
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto plan=c.findUnusedNodes();
        a.value().scoped=true;assert(!c.removeUnusedNodes(plan)&&c.nodes.size()==1);
        a.value().scoped=false;c.add("B");assert(!c.removeUnusedNodes(plan));
        plan=c.findUnusedNodes();c.connect(a,*c.nodes.back());assert(!c.removeUnusedNodes(plan));
        plan=c.findUnusedNodes();c.nodes.back()->model->safe=false;assert(!c.removeUnusedNodes(plan));
    }
    {
        // Publishing in another canvas protects the precise target, even with duplicate names.
        ofxOceanodeContainer root,child;child.timelineHost=&root;
        root.add("Same");auto& target=child.add("Same");root.paths["Child/Same/Value"]=&target.value();
        root.customGuiPanelsData.push_back({{{{{"Child/Same/Value"}}}}});
        assert(child.findUnusedNodes().groups.empty());assert(root.findUnusedNodes().groups.size()==1);
    }
    {
        ofxOceanodeContainer root,child;auto& a=root.add("A");auto& b=child.add("B");
        // Real containers own these edges; destroy the edge before either parameter.
        root.connect(a,b);assert(root.findUnusedNodes().groups.empty());root.connections.clear();
    }
    {
        ofxOceanodeContainer c;for(int i=0;i<2000;++i)c.add(to_string(i));
        for(int i=1;i<2000;++i)c.connect(*c.nodes[i-1],*c.nodes[i]);
        c.connect(*c.nodes.back(),*c.nodes.front());auto plan=c.findUnusedNodes();
        assert(plan.groups.size()==1&&plan.groups[0].size()==2000&&plan.connectionCount==2000);
        assert(c.removeUnusedNodes(plan)&&c.nodes.empty());
    }
    puts("Unused-node cleanup passed: cycles, chains, all bindings, protected effects, stale previews, ownership and 2,000-node teardown.");
}
'''

def main():
    source = (ROOT / 'src/Managers/ofxOceanodeContainer.cpp').read_text()
    header = (ROOT / 'src/Managers/ofxOceanodeContainer.h').read_text()
    plan = block(header, 'struct UnusedNodePlan') + ';'
    production = '\n'.join(block(source, signature) for signature in (
        'ofxOceanodeContainer::UnusedNodePlan ofxOceanodeContainer::findUnusedNodes()',
        'bool ofxOceanodeContainer::removeUnusedNodes('))
    with tempfile.TemporaryDirectory(prefix='oceanode-unused-') as directory:
        cpp = Path(directory) / 'test.cpp'
        cpp.write_text(FIXTURE.replace('// PLAN', plan) + production + CASES)
        binary = Path(directory) / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O1', '-g',
                        '-DOFXOCEANODE_USE_MIDI', '-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer', str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
