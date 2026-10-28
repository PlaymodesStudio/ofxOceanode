#!/usr/bin/env python3
"""Trigger (void) routers and void cables: Switch, Merge, Mute and Density.

Reuses the ASan/UBSan production Macro loader fixture. Checks the policies,
router outputs in Mix mode, outer Input triggers, the node's publishing and
changed void cables in a live-graph morph. No window or hardware.
"""
import macro_mixer_regression as mixer

mixer.CASES = r'''
static void reject(const std::function<void()>& f) { bool threw=false; try{f();}catch(const std::exception&){threw=true;} assert(threw); }
// Fires Out `Burst` times on every update.
class Pulse : public ofxOceanodeNodeModel {
public:
    Pulse(const std::string& name="Pulse",int initial=1):ofxOceanodeNodeModel(name),initial(initial) {}
    void setup() override { addParameter(burst.set("Burst",initial,0,8)); addOutputParameter(out.set("Out")); }
    int initial;
    void update(ofEventArgs&) override { for(int i=0;i<burst;++i) out.trigger(); }
    ofParameter<int> burst; ofParameter<void> out;
};
// Distinct models, so the morph matcher must see a changed cable into Hits.
struct Single : Pulse { Single():Pulse("Single",1) {} };
struct Triple : Pulse { Triple():Pulse("Triple",3) {} };
class Hits : public ofxOceanodeNodeModel {
public:
    Hits():ofxOceanodeNodeModel("Hits") {}
    void setup() override { addParameter(in.set("In")); listener=in.newListener([this] { ++count; }); }
    ofParameter<void> in; ofEventListener listener; int count=0;
};
static Hits& hits(ofxOceanodeContainer& c) {
    for(auto* node:c.getAllModules()) if(auto* h=dynamic_cast<Hits*>(&node->getNodeModel())) return *h;
    std::abort();
}
static int count(const MacroMixerMixing::Value& v) { return MacroMixerMixing::triggerCount(v); }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    using namespace MacroMixerMixing;
    {
        Registry policies;
        auto mix=[&](const std::string& id,int a,int b,float t,ofJson o=ofJson::object()) { return count(policies.mix(triggerType,id,triggers(a),triggers(b),t,o)); };
        assert(policies.policy(triggerType).id=="switch" && routerModel(triggerType)=="Router v");
        assert(routerPort(triggerType)=="Val" && routerPort("Float")=="Value");
        assert(std::find(supportedTypes().begin(),supportedTypes().end(),triggerType)!=supportedTypes().end());
        // Switch hands over at the threshold, including at the first frame.
        assert(mix("switch",1,2,.49f)==1 && mix("switch",1,2,.5f)==2);
        assert(mix("switch",1,2,0,{{"threshold",0.}})==2 && mix("switch",1,2,.99f,{{"threshold",1.}})==1);
        assert(mix("switch",1,2,1,{{"threshold",1.}})==2);
        // Merge passes either side; the same beat on both passes once.
        assert(mix("merge",1,0,0)==1 && mix("merge",0,1,.3f)==1 && mix("merge",1,1,.5f)==1 && mix("merge",3,1,.5f)==3);
        // Mute blocks from the first transition frame until completion.
        assert(mix("mute",1,1,0)==0 && mix("mute",2,2,.5f)==0 && mix("mute",2,3,1)==3);
        // Density: A passes at t=0, B at t=1; in between it is deterministic
        // and thins A out while B fills in.
        assert(mix("density",3,3,0)==3 && mix("density",4,2,1)==2);
        assert(mix("density",5,0,.37f,{{"seed",9}})==mix("density",5,0,.37f,{{"seed",9}}));
        int a=0,b=0;
        for(int i=1;i<1000;++i) { const float t=i/1000.f; a+=mix("density",1,0,t); b+=mix("density",0,1,t); assert(mix("density",2,0,t)>=mix("density",1,0,t)); }
        assert(a>420 && a<580 && b>420 && b<580);
        reject([&]{mix("merge",1,1,.5f,{{"threshold",.5}});});
        reject([&]{mix("density",1,1,.5f,{{"seed",-1}});});
        reject([&]{triggers(maxTriggers+1);});
        reject([&]{policies.mix(triggerType,"switch",1.f,triggers(0),.5f);});
    }
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    types->registerType<void>("v"); singleton->registerType<void>("v");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<router<void>>("Router","v");
    registry->registerModel<ofxOceanodeNodeMacro>("Macro"); registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    registry->registerModel<Pulse>("Test"); registry->registerModel<Hits>("Test");
    registry->registerModel<Single>("Test"); registry->registerModel<Triple>("Test");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("TriggerTest");
    auto named=[](ofxOceanodeContainer& c,int id,const std::string& name) {
        c.createNode(std::make_unique<router<void>>("v"),id).getNodeModel().getParameter<std::string>("Name")=name;
    };
    // Bang carries the Pulse; outer Go triggers come back out on Echo.
    auto save=[&](const std::string& folder,int burst) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("TriggerSource");
        named(c,1,"Bang"); named(c,2,"Go"); named(c,3,"Echo");
        static_cast<Pulse&>(c.createNode(std::make_unique<Pulse>(),1).getNodeModel()).burst=burst;
        assert(c.createConnectionFromInfo("Pulse_1","Out","Router_v_1","Val"));
        assert(c.createConnectionFromInfo("Router_v_2","Val","Router_v_3","Val"));
        c.savePreset(folder);
    };
    const auto one=scratch+"/one",two=scratch+"/two",same=scratch+"/same";
    save(one,1); save(two,2); save(same,1);
    auto contract=[](const std::string& policy,ofJson options=ofJson::object()) {
        MacroMixerContract c; c.routers={{"bang","Bang",triggerType,policy,options,"Output"},
            {"go","Go",triggerType,"",ofJson::object(),"Input"},{"echo","Echo",triggerType,policy,options,"Output"}};
        return c;
    };
    // Frames of one Mix transition, one -> to over 1000 ms: start, 39%, 59%, end, rest.
    auto frames=[&](const std::string& policy,ofJson options,const std::string& to) {
        MacroMixerRuntime r(parent); assert(r.setContract(contract(policy,options)) && r.importMacro(one) && r.importMacro(to));
        r.request(one,0,0); r.update(0); r.takeOutput("bang");
        r.update(1); assert(count(r.takeOutput("bang"))==1 && count(r.takeOutput("bang"))==0);
        std::vector<int> result;
        r.request(to,1000,0);
        for(double now:{10.,400.,600.,1010.,1020.}) { r.update(now); r.draw(); result.push_back(count(r.takeOutput("bang"))); }
        assert(!r.transitioning());
        return result;
    };
    assert(frames("",ofJson::object(),two)==std::vector<int>({1,1,2,2,2}));
    assert(frames("switch",{{"threshold",0.}},two)==std::vector<int>({2,2,2,2,2}));
    assert(frames("switch",{{"threshold",1.}},two)==std::vector<int>({1,1,1,2,2}));
    assert(frames("merge",ofJson::object(),two)==std::vector<int>({2,2,2,2,2}));
    assert(frames("merge",ofJson::object(),same)==std::vector<int>({1,1,1,1,1}));
    assert(frames("mute",ofJson::object(),two)==std::vector<int>({0,0,0,2,2}));
    {
        // An outer trigger reaches both live graphs; Merge passes it once.
        MacroMixerRuntime r(parent); assert(r.setContract(contract("merge")) && r.importMacro(one) && r.importMacro(two));
        r.request(one,0,0); r.update(0); r.update(1); r.takeOutput("echo");
        r.triggerInput("go"); r.triggerInput("bang"); r.triggerInput("missing");
        r.update(2); assert(count(r.takeOutput("echo"))==1);
        r.update(3); assert(count(r.takeOutput("echo"))==0);
        r.request(two,1000,0); r.update(10); r.takeOutput("echo");
        r.triggerInput("go"); r.update(500); assert(count(r.takeOutput("echo"))==1);
        r.triggerInput("go"); r.triggerInput("go"); r.update(600); assert(count(r.takeOutput("echo"))==2);
        assert(r.saveState().at("macros").size()==2);
    }
    {
        // The node publishes each event once, through ordinary void cables.
        auto& m=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        assert(m.setRouterContract(contract("")) && m.importMacroFolder(two));
        auto& sink=static_cast<Hits&>(parent.createNode(std::make_unique<Hits>(),1).getNodeModel());
        auto& echoed=static_cast<Hits&>(parent.createNode(std::make_unique<Hits>(),2).getNodeModel());
        assert(parent.createConnectionFromInfo("MacroMixer_1","Bang","Hits_1","In"));
        assert(parent.createConnectionFromInfo("MacroMixer_1","Echo","Hits_2","In"));
        m.getParameter<float>("Duration_ms")=0; m.getParameter<void>("Start!").trigger(); parent.update();
        const int before=sink.count; parent.update(); assert(sink.count==before+2);
        parent.draw(); parent.draw(); assert(sink.count==before+2);
        m.getParameter<void>("Go").trigger(); assert(echoed.count==0);
        parent.update(); assert(echoed.count==1 && sink.count==before+4);
        parent.update(); assert(echoed.count==1);
        parent.clearContainer();
    }
    // A changed cable into a void input morphs in place: Single (1 per frame)
    // hands Hits over to Triple (3 per frame) without falling back.
    auto saveMorph=[&](const std::string& folder,const std::string& from) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("TriggerMorph");
        named(c,1,"Bang");
        c.createNode(std::make_unique<Single>(),1); c.createNode(std::make_unique<Triple>(),1); c.createNode(std::make_unique<Hits>(),1);
        assert(c.createConnectionFromInfo(from+"_1","Out","Hits_1","In"));
        c.savePreset(folder);
    };
    const auto ma=scratch+"/morph-a",mb=scratch+"/morph-b"; saveMorph(ma,"Single"); saveMorph(mb,"Triple");
    auto morph=[&](const std::string& policy) {
        MacroMixerRuntime r(parent); MacroMixerContract c; c.routers={{"bang","Bang",triggerType,"",ofJson::object(),"Output"}};
        assert(r.setContract(c) && r.importMacro(ma) && r.importMacro(mb) && r.setMorphPolicy(triggerType,policy));
        r.request(ma,0,0,true); r.update(0);
        auto& h=hits(*r.origin()->container);
        int last=h.count; r.update(1); assert(h.count-last==1);
        const auto details=r.sourceDetails(r.macros()[1]->id());
        assert(std::any_of(details.begin(),details.end(),[](const auto& d) { return d.kind=="mix" && d.type==triggerType; }));
        assert(std::none_of(details.begin(),details.end(),[](const auto& d) { return d.kind=="fallback"; }));
        std::vector<int> result;
        r.request(mb,1000,0,true);
        for(double now:{10.,400.,600.,1010.,1020.}) {
            last=h.count; r.update(now); result.push_back(h.count-last);
            if(now==10.) assert(r.morphingInPlace());
        }
        assert(!r.transitioning() && &hits(*r.origin()->container)==&h);
        return result;
    };
    assert(morph("switch")==std::vector<int>({1,1,3,3,3}));
    assert(morph("merge")==std::vector<int>({3,3,3,3,3}));
    assert(morph("mute")==std::vector<int>({0,0,0,3,3}));
    std::cout<<"Trigger mixing passed: Switch/Merge/Mute/Density policies, Mix-mode router outputs, outer Input triggers, node publishing and in-place void cable morphs.\n";
}
'''
if __name__ == '__main__': mixer.main()
