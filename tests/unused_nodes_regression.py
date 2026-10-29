#!/usr/bin/env python3
"""Exercise the production dead-node analysis and removal with isolated host services.

The liveness rules (sinks, bindings, cables, portals, selectors, Macros, routers),
the plan shape, stale plans, unticked groups and the undo bracket all run the real
findUnusedNodes/removeUnusedNodes bodies under ASan/UBSan. Only the host types
around them are mocked; hasOutputPin is spliced from the real node model.
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
#include <chrono>
#include <cstdint>
#include <deque>
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
struct abstractPortal;
struct Abstract { virtual ~Abstract()=default; };
struct ofxOceanodeAbstractParameter : Abstract {
    ofxOceanodeNodeModel* model=nullptr; string nodeName;
    bool scoped=false,timelined=false,nodeOutput=true;
    ofxOceanodeAbstractConnection* incoming=nullptr;
    vector<ofxOceanodeAbstractConnection*> outgoing;
    auto getNodeModel(){return model;}
    bool isScoped(){return scoped;} bool isTimelined(){return timelined;}
    bool isNodeOutput(){return nodeOutput;}
    string getName(){return "Value";} string getEscapedName(){return "Value";}
    vector<string> getGroupHierarchyNames(){return {nodeName};}
    auto getOutConnections(){return outgoing;} auto getInConnection(){return incoming;}
    vector<ofxOceanodeAbstractConnection*> getInConnections(){return incoming ? vector<ofxOceanodeAbstractConnection*>{incoming} : vector<ofxOceanodeAbstractConnection*>{};}
};
struct Parameters : vector<shared_ptr<Abstract>> {string name; string getEscapedName(){return name;}};
struct ofxOceanodeNodeModel {
    virtual ~ofxOceanodeNodeModel()=default;
    Parameters parameters; bool declaredSink=false;
    Parameters& getParameterGroup(){return parameters;}
    // HAS_OUTPUT_PIN
    virtual bool isSink() const { return declaredSink || !hasOutputPin(); }
};
struct abstractRouter : ofxOceanodeNodeModel { bool isSink() const override {return true;} };
struct abstractPortal : ofxOceanodeNodeModel {
    string name; bool local=false; ofxOceanodeContainer* where=nullptr;
    static vector<abstractPortal*>& registry(){static vector<abstractPortal*> r;return r;}
    explicit abstractPortal(string n):name(n){registry().push_back(this);}
    ~abstractPortal(){auto& r=registry();r.erase(remove(r.begin(),r.end(),this),r.end());}
    string getName(){return name;}
    bool isMatching(abstractPortal* p){return name==p->name&&((!local&&!p->local)||(local&&p->local&&where==p->where));}
    bool isSink() const override {return false;}
};
struct ofxOceanodeShared {
    static vector<abstractPortal*> getPortalsNamed(const string& n){
        vector<abstractPortal*> out;for(auto* p:abstractPortal::registry())if(p->name==n)out.push_back(p);return out;
    }
};
struct portalSelectorBase : ofxOceanodeNodeModel {
    abstractPortal* selected=nullptr;
    abstractPortal* getSelectedPortal() const {return selected;}
};
struct ofxOceanodeNodeMacro : ofxOceanodeNodeModel {
    shared_ptr<ofxOceanodeContainer> child;
    shared_ptr<ofxOceanodeContainer> getContainer(){return child;}
    bool isSink() const override {return false;}
};
struct ofxOceanodeNode {
    ofxOceanodeContainer* owner=nullptr; bool persistent=false;
    unique_ptr<ofxOceanodeNodeModel> model;
    bool getIsPersistent(){return persistent;} auto& getNodeModel(){return *model;}
    auto& getParameters(){return model->parameters;} void deleteSelf();
    ofxOceanodeAbstractParameter& value(){return *static_cast<ofxOceanodeAbstractParameter*>(model->parameters[0].get());}
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
    bool pendingCanvasEdit=false; int begins=0,ends=0;
    void beginCanvasEdit(){if(pendingCanvasEdit)return;++begins;pendingCanvasEdit=true;}
    void endCanvasEdit(){if(!pendingCanvasEdit)return;pendingCanvasEdit=false;++ends;}
    ~ofxOceanodeContainer(){connections.clear();}
    auto getAllModules(){vector<ofxOceanodeNode*> out;for(auto& n:nodes)out.push_back(n.get());return out;}
    ofxOceanodeAbstractParameter* findCustomGuiParameter(const string& p){auto i=paths.find(p);return i==paths.end()?nullptr:i->second;}
    UnusedNodePlan findUnusedNodes(); bool removeUnusedNodes(const UnusedNodePlan& plan);
    ofxOceanodeNode& add(string name,unique_ptr<ofxOceanodeNodeModel> model=nullptr,bool outputPin=true){
        auto n=make_unique<ofxOceanodeNode>();n->owner=this;
        n->model=model?std::move(model):make_unique<ofxOceanodeNodeModel>();
        if(auto* portal=dynamic_cast<abstractPortal*>(n->model.get()))portal->where=this;
        n->model->parameters.name=name;
        auto p=make_shared<ofxOceanodeAbstractParameter>();p->model=n->model.get();p->nodeName=name;p->nodeOutput=outputPin;
        n->model->parameters.push_back(p);paths[name+"/Value"]=p.get();auto* raw=n.get();nodes.push_back(std::move(n));
        parameterGroupNodesMap[name]=raw;++nodesRevision;return *raw;
    }
    ofxOceanodeNode& addShapeSink(string name){return add(name,nullptr,false);}
    ofxOceanodeNode& addDeclared(string name){auto& n=add(name);n.model->declaredSink=true;return n;}
    ofxOceanodeNode& addRouter(string name){return add(name,make_unique<abstractRouter>());}
    ofxOceanodeNode& addPortal(string name,string portal,bool local=false){
        auto model=make_unique<abstractPortal>(portal);model->local=local;return add(name,std::move(model));
    }
    ofxOceanodeNode& addSelector(string name,ofxOceanodeNode* selected){
        auto model=make_unique<portalSelectorBase>();
        if(selected)model->selected=dynamic_cast<abstractPortal*>(selected->model.get());
        return add(name,std::move(model));
    }
    ofxOceanodeContainer& addMacro(string name){
        auto model=make_unique<ofxOceanodeNodeMacro>();model->child=make_shared<ofxOceanodeContainer>();
        model->child->timelineHost=this;auto* child=model->child.get();add(name,std::move(model));return *child;
    }
    void connect(ofxOceanodeNode& a,ofxOceanodeNode& b){
        assert(!b.value().incoming);
        auto link=make_unique<ofxOceanodeAbstractConnection>();link->source=&a.value();link->sink=&b.value();
        a.value().outgoing.push_back(link.get());b.value().incoming=link.get();connections.push_back(std::move(link));
    }
};
using Plan=ofxOceanodeContainer::UnusedNodePlan;
void ofxOceanodeNode::deleteSelf(){
    auto& links=owner->connections;
    links.erase(remove_if(links.begin(),links.end(),[&](const auto& c){return c->source==&value()||c->sink==&value();}),links.end());
    owner->paths.erase(model->parameters.name+"/Value");owner->parameterGroupNodesMap.erase(model->parameters.name);
    ++owner->nodesRevision;++owner->deletions;
    auto& nodes=owner->nodes;nodes.erase(find_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.get()==this;}));
}
'''
CASES = r'''
#define CHECK(...) assert((__VA_ARGS__))
static vector<string> reasonsOf(const Plan& plan,const string& node){
    for(const auto& kept:plan.kept)if(kept.node==node)return kept.reasons;
    return {"<not kept>"};
}
static bool kept(const Plan& plan,const string& node){return reasonsOf(plan,node)!=vector<string>{"<not kept>"};}
static bool has(const Plan& plan,const string& node,const string& reason){
    auto found=reasonsOf(plan,node);return find(found.begin(),found.end(),reason)!=found.end();
}
static bool dead(const Plan& plan,const string& node){
    for(const auto& group:plan.groups)if(find(group.nodes.begin(),group.nodes.end(),node)!=group.nodes.end())return true;
    return false;
}
static vector<vector<string>> groupNames(const Plan& plan){
    vector<vector<string>> out;for(const auto& group:plan.groups)out.push_back(group.nodes);return out;
}
using Names=vector<vector<string>>;

int main(){
    // Dead cycle and a lone node are removed in one undoable edit; comments lose their members.
    {
        ofxOceanodeContainer c;
        auto& a=c.add("A");auto& b=c.add("B");auto& d=c.add("D");c.add("Lone");
        c.connect(a,b);c.connect(b,d);c.connect(d,a);c.comments.push_back({{&a,&b,&d}});
        auto plan=c.findUnusedNodes();
        CHECK(plan.groups.size()==2&&plan.kept.empty());
        CHECK(plan.groups[0].connections==3&&plan.groups[1].connections==0);
        CHECK(c.removeUnusedNodes(plan));
        CHECK(c.nodes.empty()&&c.connections.empty()&&c.deletions==4&&c.comments[0].nodes.empty());
        CHECK(c.begins==1&&c.ends==1&&!c.pendingCanvasEdit);
    }
    // A dead chain goes in one run: the rule is liveness, not peeling the last layer.
    {
        ofxOceanodeContainer c;vector<ofxOceanodeNode*> chain;
        for(int i=0;i<5;++i)chain.push_back(&c.add("N"+to_string(i)));
        for(int i=1;i<5;++i)c.connect(*chain[i-1],*chain[i]);
        auto plan=c.findUnusedNodes();
        CHECK(plan.groups.size()==1&&plan.groups[0].nodes.size()==5&&plan.groups[0].connections==4);
    }
    // A chain feeding a shape sink is live, with a reason per node.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");auto& s=c.addShapeSink("S");
        c.connect(a,b);c.connect(b,s);auto plan=c.findUnusedNodes();
        CHECK(plan.groups.empty()&&plan.kept.size()==3);
        CHECK(has(plan,"S","sink: no outputs")&&has(plan,"B","feeds: S")&&has(plan,"A","feeds: B"));
    }
    // A dead branch off a live chain is listed on its own.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");auto& d=c.add("D");auto& s=c.addShapeSink("S");
        c.connect(a,b);c.connect(a,d);c.connect(b,s);auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names{{"D"}}&&plan.groups[0].connections==1);
        CHECK(has(plan,"A","feeds: B")&&kept(plan,"B")&&kept(plan,"S"));
    }
    // A dead subgraph that feeds a dead sink-less tail stays dead, whatever it touches.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");auto& s=c.addShapeSink("S");auto& x=c.add("X");auto& y=c.add("Y");
        c.connect(a,b);c.connect(b,s);c.connect(x,y);auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names{{"X","Y"}});
    }
    // R1: a declared sink with outputs is kept, and so is what feeds it.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& d=c.addDeclared("D");c.connect(a,d);
        auto plan=c.findUnusedNodes();
        CHECK(plan.groups.empty()&&has(plan,"D","sink: declared")&&has(plan,"A","feeds: D"));
    }
    // R5: routers are interface pins and always kept; what only they feed is dead.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& r=c.addRouter("R");auto& x=c.add("X");c.addRouter("Lone");
        c.connect(a,r);c.connect(r,x);auto plan=c.findUnusedNodes();
        CHECK(has(plan,"R","sink: router")&&has(plan,"Lone","sink: router")&&has(plan,"A","feeds: R"));
        CHECK(groupNames(plan)==Names{{"X"}});
    }
    // R3: each host binding on a consumer keeps it and its feeder.
    {
        const char* expected[]={"sink: persistent","sink: scope Value","sink: timeline Value","sink: binding Value",
            "sink: binding Value","sink: binding Value","sink: MIDI Value","sink: MIDI Value"};
        for(int usage=0;usage<8;++usage){
            ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");c.connect(a,b);
            switch(usage){
                case 0:b.persistent=true;break;
                case 1:b.value().scoped=true;break;
                case 2:b.value().timelined=true;break;
                case 3:c.customGuiPanelsData.push_back({{{{{"B/Value"}}}}});break;
                case 4:c.customGuiSnapshotBanks.push_back({{{{{"B/Value",1}}}}});break;
                case 5:c.timeline.tracks.push_back({{{"B/Value"}}});break;
                case 6:c.midiBindings["B-|-Value"]={1};break;
                case 7:c.persistentMidiBindings["B-|-Value"]={1};break;
            }
            auto plan=c.findUnusedNodes();
            CHECK(plan.groups.empty()&&plan.kept.size()==2);
            CHECK(has(plan,"B",expected[usage])&&has(plan,"A","feeds: B"));
            CHECK(c.removeUnusedNodes(plan)&&c.nodes.size()==2);
        }
        // A binding on the producer keeps only the producer: its consumer feeds nothing.
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");c.connect(a,b);a.value().scoped=true;
        auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names{{"B"}}&&has(plan,"A","sink: scope Value"));
    }
    // Stale plans refuse: bindings, new nodes, new cables and new sinks all change the dead set.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto plan=c.findUnusedNodes();CHECK(dead(plan,"A"));
        a.value().scoped=true;CHECK(!c.removeUnusedNodes(plan)&&c.nodes.size()==1);
        a.value().scoped=false;c.add("B");CHECK(!c.removeUnusedNodes(plan));
        plan=c.findUnusedNodes();c.connect(a,*c.nodes.back());CHECK(!c.removeUnusedNodes(plan));
        plan=c.findUnusedNodes();c.nodes.back()->model->declaredSink=true;CHECK(!c.removeUnusedNodes(plan));
        CHECK(c.deletions==0&&c.begins==0);
    }
    // Publishing in another canvas keeps the precise target, even with duplicate names.
    {
        ofxOceanodeContainer root,child;child.timelineHost=&root;
        root.add("Same");auto& target=child.add("Same");root.paths["Child/Same/Value"]=&target.value();
        root.customGuiPanelsData.push_back({{{{{"Child/Same/Value"}}}}});
        CHECK(child.findUnusedNodes().groups.empty()&&has(child.findUnusedNodes(),"Same","sink: binding Value"));
        CHECK(root.findUnusedNodes().groups.size()==1);
    }
    // Cables that leave the canvas keep both ends, from either side.
    {
        ofxOceanodeContainer root,child;auto& a=root.add("A");auto& b=child.add("B");
        // Real containers own these edges; destroy the edge before either parameter.
        root.connect(a,b);
        auto rootPlan=root.findUnusedNodes(),childPlan=child.findUnusedNodes();
        CHECK(rootPlan.groups.empty()&&has(rootPlan,"A","feeds: outside canvas"));
        CHECK(childPlan.groups.empty()&&has(childPlan,"B","connection: outside canvas"));
        root.connections.clear();
    }
    // R6: a writer portal is live when a peer reader feeds a live node.
    {
        ofxOceanodeContainer c;auto& x=c.add("X");auto& w=c.addPortal("PW","G");auto& r=c.addPortal("PR","G");auto& s=c.addShapeSink("S");
        c.connect(x,w);c.connect(r,s);auto plan=c.findUnusedNodes();
        CHECK(plan.groups.empty());
        CHECK(has(plan,"PR","feeds: S")&&has(plan,"PW","portal: G used by PR")&&has(plan,"X","feeds: PW"));
    }
    // R6: a reader nobody consumes leaves the writer dead, and a local portal never matches a global one.
    {
        ofxOceanodeContainer c;auto& x=c.add("X");auto& w=c.addPortal("PW","G");c.addPortal("PR","G");
        c.connect(x,w);auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names({{"PR"},{"PW","X"}}));
    }
    {
        ofxOceanodeContainer c;auto& x=c.add("X");auto& w=c.addPortal("PW","G");auto& r=c.addPortal("PR","G",true);auto& s=c.addShapeSink("S");
        c.connect(x,w);c.connect(r,s);auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names{{"PW","X"}}&&kept(plan,"PR"));
    }
    // R6: a selector that feeds something live keeps the portal it reads, and that portal's writer.
    {
        ofxOceanodeContainer c;auto& x=c.add("X");auto& w=c.addPortal("PW","G");auto& sel=c.addSelector("SEL",&w);auto& s=c.addShapeSink("S");
        c.connect(x,w);c.connect(sel,s);auto plan=c.findUnusedNodes();
        CHECK(plan.groups.empty()&&has(plan,"PW","portal: G used by SEL")&&has(plan,"X","feeds: PW"));
    }
    // R6: a reader or selector in another canvas keeps the writer here; without one the writer is dead.
    for(int variant=0;variant<3;++variant){
        ofxOceanodeContainer root;auto& child=root.addMacro("M");
        auto& x=child.add("X");auto& w=child.addPortal("W","G");child.connect(x,w);
        auto& reader=root.addPortal("R","G");
        if(variant==0){auto& t=root.add("T");root.connect(reader,t);}
        if(variant==1)root.addSelector("SEL",&reader);
        auto plan=child.findUnusedNodes();
        if(variant<2)CHECK(plan.groups.empty()&&has(plan,"W","portal: G used by another canvas")&&has(plan,"X","feeds: W"));
        else CHECK(groupNames(plan)==Names{{"W","X"}});
        root.connections.clear();
    }
    // R7: a Macro is live through a hard sink inside it; routers and pure nodes do not count.
    {
        ofxOceanodeContainer root;auto& x=root.add("X");auto& inner=root.addMacro("M");
        auto& m=*root.nodes.back();root.connect(x,m);
        inner.addRouter("R");inner.add("P");
        auto plan=root.findUnusedNodes();
        CHECK(groupNames(plan)==Names{{"M","X"}});
        inner.addShapeSink("Sink");plan=root.findUnusedNodes();
        CHECK(plan.groups.empty()&&has(plan,"M","macro: contains Sink")&&has(plan,"X","feeds: M"));
    }
    {   // Nested Macros, a binding inside, and a portal that broadcasts out of the Macro.
        ofxOceanodeContainer root;auto& outer=root.addMacro("M");auto& deeper=outer.addMacro("M2");deeper.addDeclared("Deep");
        CHECK(has(root.findUnusedNodes(),"M","macro: contains M2"));
    }
    {
        ofxOceanodeContainer root;auto& inner=root.addMacro("M");auto& n=inner.add("N");
        root.paths["M/N/Value"]=&n.value();root.customGuiPanelsData.push_back({{{{{"M/N/Value"}}}}});
        CHECK(has(root.findUnusedNodes(),"M","macro: contains N"));
    }
    {
        ofxOceanodeContainer root;auto& inner=root.addMacro("M");auto& x=inner.add("X");auto& w=inner.addPortal("W","G");inner.connect(x,w);
        root.addPortal("R","G");
        CHECK(has(root.findUnusedNodes(),"M","macro: contains W"));
    }
    // Inside a Macro the same command keeps its router pins and lists what only they feed.
    {
        ofxOceanodeContainer root;auto& inner=root.addMacro("M");
        auto& r=inner.addRouter("R");auto& d=inner.add("D");inner.connect(r,d);
        auto plan=inner.findUnusedNodes();
        CHECK(has(plan,"R","sink: router")&&groupNames(plan)==Names{{"D"}});
    }
    // Unticked groups survive removal, and the plan still lists them afterwards.
    {
        ofxOceanodeContainer c;auto& a=c.add("A");auto& b=c.add("B");auto& cc=c.add("C");auto& d=c.add("D");
        c.connect(a,b);c.connect(cc,d);auto plan=c.findUnusedNodes();
        CHECK(groupNames(plan)==Names({{"A","B"},{"C","D"}}));
        plan.groups[1].selected=false;
        CHECK(c.removeUnusedNodes(plan)&&c.deletions==2&&c.nodes.size()==2);
        CHECK(groupNames(c.findUnusedNodes())==Names{{"C","D"}});
    }
    // Removal joins an edit already in progress instead of closing it.
    {
        ofxOceanodeContainer c;c.add("A");c.pendingCanvasEdit=true;
        auto plan=c.findUnusedNodes();CHECK(c.removeUnusedNodes(plan));
        CHECK(c.begins==0&&c.ends==0&&c.pendingCanvasEdit&&c.nodes.empty());
    }
    // The result never depends on the order nodes were created in.
    {
        auto build=[](bool reversed){
            auto c=make_unique<ofxOceanodeContainer>();vector<string> order={"A","B","D","S","X"};
            if(reversed)reverse(order.begin(),order.end());
            for(const auto& name:order){if(name=="S")c->addShapeSink(name);else c->add(name);}
            auto at=[&](const string& n)->ofxOceanodeNode&{return *c->parameterGroupNodesMap.at(n);};
            c->connect(at("A"),at("B"));c->connect(at("A"),at("D"));c->connect(at("B"),at("S"));
            return c;
        };
        auto first=build(false),second=build(true);auto p1=first->findUnusedNodes(),p2=second->findUnusedNodes();
        CHECK(groupNames(p1)==groupNames(p2)&&p1.kept.size()==p2.kept.size());
        for(size_t i=0;i<p1.kept.size();++i)CHECK(p1.kept[i].node==p2.kept[i].node&&p1.kept[i].reasons==p2.kept[i].reasons);
    }
    // Scale: a 2,000-node dead cycle, and a live graph with portal fan-out, stay fast and exact.
    {
        auto start=chrono::steady_clock::now();
        ofxOceanodeContainer c;for(int i=0;i<2000;++i)c.add(to_string(i));
        for(int i=1;i<2000;++i)c.connect(*c.nodes[i-1],*c.nodes[i]);
        c.connect(*c.nodes.back(),*c.nodes.front());auto plan=c.findUnusedNodes();
        CHECK(plan.groups.size()==1&&plan.groups[0].nodes.size()==2000&&plan.groups[0].connections==2000);
        CHECK(c.removeUnusedNodes(plan)&&c.nodes.empty());
        ofxOceanodeContainer live;auto& s=live.addShapeSink("S");
        for(int i=0;i<300;++i){
            auto& x=live.add("X"+to_string(i));auto& w=live.addPortal("W"+to_string(i),"G");live.connect(x,w);
            auto& r=live.addPortal("R"+to_string(i),"G");(void)r;
        }
        for(int i=0;i<300;++i){
            auto& hop=live.add("H"+to_string(i));live.connect(*live.parameterGroupNodesMap.at("R"+to_string(i)),hop);
            if(i==299)live.connect(hop,s);
        }
        auto livePlan=live.findUnusedNodes();
        // Every portal shares one name, so the last hop's reader keeps all writers live.
        CHECK(kept(livePlan,"R299")&&kept(livePlan,"W0")&&kept(livePlan,"X0")&&kept(livePlan,"W299"));
        CHECK(dead(livePlan,"H0")&&dead(livePlan,"R0"));
        CHECK(chrono::steady_clock::now()-start<chrono::seconds(10));
    }
    puts("Unused-node cleanup passed: liveness chains and cycles, sinks, bindings, routers, portals, selectors, Macros, stale plans, unticked groups, undo bracket and scale.");
}
'''

def main():
    source = (ROOT / 'src/Managers/ofxOceanodeContainer.cpp').read_text()
    header = (ROOT / 'src/Managers/ofxOceanodeContainer.h').read_text()
    model = (ROOT / 'src/Nodes/ofxOceanodeNodeModel.h').read_text()
    # The mock's default must stay the real one: same line, and the real helper spliced in.
    assert 'virtual bool isSink() const { return declaredSink || !hasOutputPin(); }' in model
    helper = block(model, 'bool hasOutputPin() const')
    plan = block(header, 'struct UnusedNodePlan') + ';'
    production = '\n'.join(block(source, signature) for signature in (
        'ofxOceanodeContainer::UnusedNodePlan ofxOceanodeContainer::findUnusedNodes()',
        'bool ofxOceanodeContainer::removeUnusedNodes('))
    with tempfile.TemporaryDirectory(prefix='oceanode-unused-') as directory:
        cpp = Path(directory) / 'test.cpp'
        cpp.write_text(FIXTURE.replace('// PLAN', plan).replace('// HAS_OUTPUT_PIN', helper) + production + CASES)
        binary = Path(directory) / 'test'
        subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-O1', '-g',
                        '-DOFXOCEANODE_USE_MIDI', '-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer', str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
