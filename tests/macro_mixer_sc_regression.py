#!/usr/bin/env python3
"""SCBus graph lifetime/routing with production traversal/manager/Output and captured server commands; no audio device."""
import macro_mixer_regression as mixer
from of_environment import addon_source, addon_include_dirs
mixer.PRELUDE += r'''
#include "SCFake.h"
#include "scNode.h"
#include "scOutput.h"
#include "serverManager.h"
#include "scGlobalPitch.h"
#include "MacroMixerSCBus.h"
#include "MacroMixerServerAllocator.h"
#include "ofxOceanodeSuperColliderConfig.h"
class TestTone : public scNode {
    ofParameter<float> frequency;
    std::map<ofxSCServer*,std::unique_ptr<ofxSCSynth>> synths;
    std::map<ofxSCServer*,int> buses;
public:
    static inline int live=0;
    TestTone(std::string name="Mixer Test Tone"):scNode(name) {++live;}
    ~TestTone() override { for(auto& s:synths) s.second->free(); --live; }
    void setup() override {addParameter(frequency.set("Freq",220,0,20000));addOutput("Out");}
    void buildSynth(ofxSCServer* s) override {synths[s]=std::make_unique<ofxSCSynth>("tone",s);}
    void createSynth(ofxSCServer* s) override {synths[s]->set("freq",frequency.get());synths[s]->createAndRun();}
    void setOutputBus(ofxSCServer* s,int,int bus) override {buses[s]=bus;if(synths.count(s))synths[s]->set("out",bus);}
    int getOutputBusIndex(ofxSCServer* s,int) override {return buses.at(s);}
    int getLastSynthID(ofxSCServer* s) override {return synths.count(s)?synths[s]->nodeID:-1;}
    void moveSynthBefore(ofxSCServer*,int) override {}
    void free(ofxSCServer* s) override {if(synths.count(s)){synths[s]->free();synths.erase(s);}}
};
class TestEffect : public TestTone {
public:
    std::map<ofxSCServer*,int> inputBus;
    TestEffect():TestTone("Mixer Test Effect") {}
    void setup() override {TestTone::setup();addInput("In");}
    void setInputBus(ofxSCServer* s,scNode*,int bus) override {inputBus[s]=bus;}
};
// A node saved with a Server, like SC FFT HD: it pins its source to that server.
class TestPinned : public TestTone {
    ofParameter<int> server;
public:
    TestPinned():TestTone("Mixer Test Pinned") {}
    void setup() override {TestTone::setup();addParameter(server.set("Server",0,0,2));}
};
static ofxSCServer* setupServer=nullptr;
static bool synthdefsLoading=false;
'''
BASE=mixer.native_production

def production():
    result=BASE()+'\nstd::map<ofxSCServer*,int> serverManager::serverSampleRates;\n'
    source=addon_source(mixer.ROOT.parent/'ofxOceanodeSuperCollider','serverManager.cpp').read_text()
    for signature in ['serverManager::serverManager()', 'serverManager::~serverManager()',
        'void serverManager::addOutput(', 'void serverManager::removeOutput(',
        'void serverManager::setVolume(', 'void serverManager::setDelay(',
        'void serverManager::setStereoMix(', 'void serverManager::setStereoMixSize(',
        'void serverManager::watchMixerGraphSync()', 'void serverManager::releaseGraphBusses()', 'void serverManager::teardownGraphForPresetLoad()',
        'bool serverManager::hasSoundNodes()', 'void serverManager::recomputeGraph()']:
        result+=(source[source.index(signature):source.index('void serverManager::loadSynthdefsFromPreset')] if signature=='void serverManager::recomputeGraph()' else mixer.block(source,signature))+'\n'
    # Newer serverManager versions release folded waveform captures from the graph.
    if 'void serverManager::clearFoldedWaveforms(' in source:
        result+=mixer.block(source,'void serverManager::clearFoldedWaveforms(')+'\n'
    result+='void serverManager::setup(){server=setupServer;watchMixerGraphSync();initialized=true;busFromSilent=std::make_unique<ofxSCBus>(RATE_AUDIO,128,server);}\n'
    result+='void serverManager::requireSynthdefFolder(const std::string&){}\nbool serverManager::areRequiredSynthdefsLoading() const {return synthdefsLoading;}\n'
    return result
mixer.native_production=production
mixer.CASES=r'''
static void recompute(serverManager& m,ofxSCServer& s) {m.recomputeGraph();s.acknowledge();}
static void near(double a,double b) {if(!std::isfinite(a)||std::abs(a-b)>=.0001) {std::cerr<<a<<" != "<<b<<"\n";std::abort();}}
static nodePort bus(const MacroMixerRuntime& r) {return MacroMixerMixing::object<nodePort>(r.outputValue("audio"));}
static int bridgeID(ofxSCServer& s) {for(auto& n:s.synths) if(n.second.name=="oceanodeMacroMixer2") return n.first;return -1;}
static ofxOceanodeMacroMixer& innerMixer(ofxOceanodeNodeMacro& macro) {
    for(auto* n:macro.getContainer()->getAllModules())
        if(auto* m=dynamic_cast<ofxOceanodeMacroMixer*>(&n->getNodeModel())) return *m;
    std::abort();
}
int main(int argc,char** argv) {
    assert(argc==3);namespace fs=std::filesystem;ofDisableDataPath();ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>();ofGetMainLoop()->setCurrentWindow(window);
    ofxSCServer s1,s2; serverManager m1,m2; setupServer=&s1;m1.setup();setupServer=&s2;m2.setup();
    std::vector<serverManager*> managers{&m1,&m2}; registerMacroMixerSCBus(managers);registerMacroMixerSCBus(managers);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>();auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    types->registerType<nodePort>("ScBus");ofxOceanodeTypesRegistry::getInstance()->registerType<nodePort>("ScBus");
    registry->registerModel<router<nodePort>>("Router","ScBus",nodePort()); registry->registerModel<TestTone>("SuperCollider/Test");
    registry->registerModel<TestEffect>("SuperCollider/Test");registry->registerModel<TestPinned>("SuperCollider/Test");
    registry->registerModel<scOutput>("SuperCollider",managers);registry->registerModel<ofxOceanodeMacroMixer>("Container");registry->registerModel<ofxOceanodeNodeMacro>("Container");
    auto transport=std::make_shared<ofxOceanodeTransport>();transport->setDriverMode(TransportDriverMode::FrameStep);const auto generation=transport->getState().generation;
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("TestParent");
    const std::string scratch=argv[2];const auto cwd=fs::current_path();fs::current_path(scratch);
    auto write=[](const std::string& dir,const std::string& file,const ofJson& j){fs::create_directories(dir);std::ofstream(dir+"/"+file)<<j.dump(2);};
    const auto a=scratch+"/a",b=scratch+"/b";
    for(auto dir:{a,b}) {
        write(dir,"modules.json",{{"SC Mixer Test Tone",{{"01",{0,0}}}},{"SC Mixer Test Effect",{{"01",{0,0}}}},{"Router ScBus",{{"01",{0,0}}}}});
        write(dir,"SC_Output_1.json",{{"Server",0},{"Active",true}});
        write(dir,"SC_Mixer_Test_Tone_1.json",{{"Freq",dir==a?220:660}});write(dir,"Router_ScBus_1.json",{{"Name","Audio"}});write(dir,"SC_Mixer_Test_Effect_1.json",{{"Freq",440}});
        write(dir,"connections.json",{{"SC_Mixer_Test_Tone_1",{{"Out",{{"SC_Mixer_Test_Effect_1",{{"In",nullptr}}}}}}},{"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
    }
    MacroMixerContract c;c.routers={{"audio","Audio","ScBus"}};
    {
        // Morph internal SC controls; the bridge is an identity adapter with
        // destination-only sources, preserving graph leases and server sync.
        auto& output=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        MacroMixerRuntime r(parent); auto contract=c; contract.routers[0].policy="fade_out";
        assert(r.setContract(contract)&&r.importMacro(a)&&r.importMacro(b));
        r.request(a,0,0,true); r.update(0); output.getParameter<nodePort>("In")=bus(r); recompute(m1,s1); r.update(1);
        auto* bridge=bus(r).getNodeRef(); assert(bridge&&bridgeID(s1)>0);
        const auto* liveGraph=r.origin(); const int liveNodes=TestTone::live;
        r.request(b,1000,0,true); r.update(10); output.getParameter<nodePort>("In")=bus(r);
        assert(r.morphingInPlace()&&r.origin()==liveGraph&&r.destination()==liveGraph&&TestTone::live==liveNodes);
        auto& frequency=r.destination()->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel().getParameter<float>("Freq");
        near(frequency,220); r.update(510); near(frequency,440);
        int id=bridgeID(s1); assert(id>0); near(s1.synths.at(id).controls.at("gainA"),0); near(s1.synths.at(id).controls.at("gainB"),1);
        assert(bus(r).getNodeRef()==bridge); r.draw(); assert(bus(r).getNodeRef()==bridge);
        auto port=MacroMixerMixing::object<nodePort>(r.destination()->read("audio"));
        auto effect=static_cast<TestEffect*>(port.getNodeRef());
        auto& tone=static_cast<TestTone&>(r.destination()->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        assert(effect->inputBus.at(&s1)==tone.getOutputBusIndex(&s1,0));
        r.finish(); r.update(520); assert(!r.transitioning()&&r.origin()==liveGraph); near(frequency,660);
        r.request(a,1000,0,true); r.update(530); r.finish(); r.update(540); assert(!r.transitioning()); near(frequency,220);
        // Changed ScBus wiring blends on the live graph and waits for backend
        // acknowledgement, including a Finish requested during preparation.
        const auto extra=scratch+"/extra"; fs::copy(b,extra,fs::copy_options::recursive);
        auto modules=ofLoadJson(extra+"/modules.json"); modules["SC Mixer Test Tone"]["02"]={0,0}; ofSaveJson(extra+"/modules.json",modules);
        write(extra,"SC_Mixer_Test_Tone_2.json",{{"Freq",330}});
        auto cables=ofLoadJson(extra+"/connections.json"); cables.erase("SC_Mixer_Test_Effect_1");
        // Change the model driving the selected resource port. Renaming a
        // same-type tone in the same role is now an in-place match instead.
        cables["SC_Mixer_Test_Tone_2"]["Out"]["Router_ScBus_1"]["Value"]=nullptr;
        ofSaveJson(extra+"/connections.json",cables);
        assert(r.importMacro(extra)); r.request(extra,1000,0,true); r.update(550); r.finish(); r.update(560);
        assert(r.morphingInPlace()&&r.origin()==liveGraph&&r.progress()==0);
        output.getParameter<nodePort>("In")=bus(r); recompute(m1,s1); r.update(561); r.update(562);
        // Final native rewire is another server handoff; temporary bridges and
        // the native source graph stay alive until it has been acknowledged.
        recompute(m1,s1); r.update(563); r.update(564); assert(!r.transitioning());
        r.request(a,100,0,false); r.update(570); output.getParameter<nodePort>("In")=bus(r); recompute(m1,s1); r.update(580); r.update(620);
        id=bridgeID(s1); near(s1.synths.at(id).controls.at("gainA"),0); near(s1.synths.at(id).controls.at("gainB"),0);
        r.clear(); output.getParameter<nodePort>("In")=nodePort(); recompute(m1,s1);
    }
    parent.clearContainer(); recompute(m1,s1); assert(TestTone::live==0&&ofxSCSynth::live==0);
    const auto routed=scratch+"/routed",silent=scratch+"/silent";
    fs::copy(b,routed,fs::copy_options::recursive); fs::copy(b,silent,fs::copy_options::recursive);
    auto routedModules=ofLoadJson(routed+"/modules.json"); routedModules["SC Mixer Test Effect"]["02"]={0,0}; ofSaveJson(routed+"/modules.json",routedModules);
    write(routed,"SC_Mixer_Test_Effect_2.json",{{"Freq",330}});
    write(routed,"connections.json",{{"SC_Mixer_Test_Tone_1",{{"Out",{{"SC_Mixer_Test_Effect_2",{{"In",nullptr}}}}}}},
        {"SC_Mixer_Test_Effect_2",{{"Out",{{"SC_Mixer_Test_Effect_1",{{"In",nullptr}}}}}}},
        {"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
    write(silent,"connections.json",{{"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
    for(const auto& spec:std::vector<std::pair<std::string,std::pair<float,float>>>{{"linear",{.5,.5}},{"equal_power",{std::sqrt(.5f),std::sqrt(.5f)}},{"switch",{0,1}},{"fade_out",{0,0}}}) {
        auto& output=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        MacroMixerRuntime r(parent); assert(r.setContract(c)&&r.setMorphPolicy("ScBus",spec.first,{{"smoothing_ms",5}}));
        assert(r.importMacro(a)&&r.importMacro(routed)&&r.importMacro(silent));
        r.request(a,0,0,true);r.update(0);output.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);r.update(1);
        const auto* graph=r.origin();
        auto& tone=static_cast<TestTone&>(graph->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        auto& effect=static_cast<TestEffect&>(graph->container->getDynamicNodes().at("SC Mixer Test Effect").at(1)->getNodeModel());
        const int toneID=tone.getLastSynthID(&s1),effectID=effect.getLastSynthID(&s1),toneBus=tone.getOutputBusIndex(&s1,0),effectBus=effect.getOutputBusIndex(&s1,0);
        const auto& details=r.sourceDetails(r.importedMacro(routed)->id());
        assert(std::none_of(details.begin(),details.end(),[](const auto& d){return d.kind=="fallback";}));
        assert(std::any_of(details.begin(),details.end(),[&](const auto& d){return d.kind=="mix"&&d.type=="ScBus"&&d.policy==spec.first&&d.options["smoothing_ms"]==5;}));
        r.request(routed,1000,0,true);r.update(10); assert(r.morphingInPlace()&&r.origin()==graph);
        auto* internal=effect.getParameter<nodePort>("In").get().getNodeRef(); assert(internal&&internal!=&tone);
        synthdefsLoading=true;recompute(m1,s1);r.update(100);assert(r.progress()==0);near(tone.getParameter<float>("Freq"),220);
        synthdefsLoading=false;m1.recomputeGraph();r.update(200);assert(r.progress()==0);
        assert(tone.getLastSynthID(&s1)==toneID&&effect.getLastSynthID(&s1)==effectID);
        s1.acknowledge();r.update(210);r.update(710);near(r.progress(),.5);near(tone.getParameter<float>("Freq"),440);
        const int internalID=internal->getLastSynthID(&s1);assert(internalID>0);
        near(s1.synths.at(internalID).controls.at("gainA"),spec.second.first);near(s1.synths.at(internalID).controls.at("gainB"),spec.second.second);
        near(s1.synths.at(internalID).controls.at("smoothing"),.005);
        assert(tone.getOutputBusIndex(&s1,0)==toneBus&&effect.getOutputBusIndex(&s1,0)==effectBus);
        assert(effect.inputBus.at(&s1)==internal->getOutputBusIndex(&s1,0));
        r.finish();r.update(720);recompute(m1,s1);r.update(721);r.update(722);assert(!r.transitioning()&&r.origin()==graph);
        assert(tone.getLastSynthID(&s1)==toneID&&effect.getLastSynthID(&s1)==effectID);
        // Removing a source erases it from the native graph at completion but
        // retains its C++ owner and private bridge through the final /synced.
        r.request(a,1000,0,true);r.update(800);recompute(m1,s1);r.update(801);r.finish();r.update(802);
        assert(!graph->container->getDynamicNodes().count("SC Mixer Test Effect")||graph->container->getDynamicNodes().at("SC Mixer Test Effect").size()==1);
        assert(TestTone::live==3);m1.recomputeGraph();assert(TestTone::live==3);s1.acknowledge();r.update(803);r.update(804);assert(TestTone::live==2);
        assert(tone.getLastSynthID(&s1)==toneID&&effect.getLastSynthID(&s1)==effectID);
        // An uncabled ScBus input is silence; it uses the same configured policy.
        const auto& silentDetails=r.sourceDetails(r.importedMacro(silent)->id());
        assert(std::any_of(silentDetails.begin(),silentDetails.end(),[](const auto& d){return d.kind=="mix"&&d.type=="ScBus"&&d.message.find("Silence")!=std::string::npos;}));
        r.request(silent,100,0,true);r.update(900);assert(r.morphingInPlace());recompute(m1,s1);r.update(901);r.finish();r.update(902);
        recompute(m1,s1);r.update(903);r.update(904);assert(!effect.getParameter<nodePort>("In").get().getNodeRef());
        const int capacity=s1.busLimit;s1.busLimit=int(s1.buses.size());
        r.request(routed,1000,0,true);r.update(950);assert(r.morphingInPlace());m1.recomputeGraph();r.update(951);
        assert(!r.transitioning()&&!r.diagnostics().empty()&&r.origin()==graph);
        assert(!effect.getParameter<nodePort>("In").get().getNodeRef());
        assert(effect.getLastSynthID(&s1)==effectID);s1.busLimit=capacity;recompute(m1,s1);r.update(952);
        // Cancel/Clear before the new route is acknowledged. No freed native
        // references may remain in the server manager or the private bridge.
        r.request(routed,1000,0,true);r.update(1000);assert(r.morphingInPlace());m1.recomputeGraph();
        r.clear();output.getParameter<nodePort>("In")=nodePort();s1.acknowledge();assert(TestTone::live>0);recompute(m1,s1);assert(TestTone::live==0);
        parent.clearContainer();recompute(m1,s1);assert(ofxSCSynth::live==0);
    }
    {
        auto& output1=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        auto& output2=parent.createNode(registry->create("SC Output"),2).getNodeModel();output2.getParameter<int>("Server")=1;
        MacroMixerRuntime r(parent);assert(r.setContract(c)&&r.importMacro(a)&&r.importMacro(routed));
        r.request(a,0,0,true);r.update(0);output1.getParameter<nodePort>("In")=bus(r);output2.getParameter<nodePort>("In")=bus(r);
        recompute(m1,s1);recompute(m2,s2);r.update(1);
        r.request(routed,1000,0,true);r.update(10);assert(r.morphingInPlace());
        recompute(m1,s1);r.update(20);assert(r.progress()==0);
        m2.recomputeGraph();r.update(30);assert(r.progress()==0);s2.acknowledge();r.update(40);r.finish();r.update(41);
        recompute(m1,s1);recompute(m2,s2);r.update(42);r.update(43);assert(!r.transitioning());
        r.request(a,1000,0,true);r.update(100);recompute(m1,s1);recompute(m2,s2);r.update(101);r.finish();r.update(102);
        assert(TestTone::live==3);recompute(m1,s1);assert(TestTone::live==3);recompute(m2,s2);assert(TestTone::live==2);
        r.clear();output1.getParameter<nodePort>("In")=nodePort();output2.getParameter<nodePort>("In")=nodePort();
        recompute(m1,s1);recompute(m2,s2);parent.clearContainer();recompute(m1,s1);recompute(m2,s2);assert(TestTone::live==0&&ofxSCSynth::live==0);
    }
    {
        // Server allocator: a free server before a busy one, loaded SynthDefs
        // before loading ones, then the lowest average CPU. One server, a
        // single (pinned or Single) source or no other reachable server: none.
        using namespace MacroMixerServerAllocator;
        ofxSCServer s3; Request q; q.origin=&s1;
        q.candidates={{&s1,true,true,false,1},{&s2,true,true,false,5},{&s3,true,false,false,40}};
        assert(choose(q)==&s3);
        q.candidates={{&s1,true,true,false,1},{&s2,true,true,false,30},{&s3,true,true,false,10}};
        assert(choose(q)==&s3);
        q.candidates={{&s1,true,false,false,0},{&s2,true,false,true,1},{&s3,true,false,false,50}};
        assert(choose(q)==&s3);
        q.candidates={{&s1,true,false,false,0},{&s2,false,false,false,0}};assert(choose(q)==nullptr);
        q.candidates={{&s1,true,false,false,0}};assert(choose(q)==nullptr);
        q.candidates={{&s1,true,false,false,0},{&s2,true,false,false,0}};assert(choose(q)==&s2);
        q.single=true;assert(choose(q)==nullptr);
        q.single=false;q.origin=nullptr;assert(choose(q)==nullptr);
        // Placement hooks read saved definitions: a node saved with a Server
        // pins its source there; an ScBus Portal (nested too) keeps it single.
        const auto* hooks=MacroMixerMixing::registry().placement("ScBus");assert(hooks&&hooks->preload);
        auto pin=hooks->pin(ofJson::parse(R"({"nodes":{"SC_FFT_HD_1":{"model":"SC FFT HD","settings":{"Server":1}}}})"));
        assert(pin.index==1&&!pin.single&&pin.reason.find("server 1")!=std::string::npos);
        pin=hooks->pin(ofJson::parse(R"({"nodes":{"Macro_1":{"model":"Macro","settings":{},"graph":{"nodes":{"Portal_ScBus_1":{"model":"Portal ScBus","settings":{}}}}}}})"));
        assert(pin.single&&pin.index<0&&pin.reason.find("Portal")!=std::string::npos);
        pin=hooks->pin(ofJson::parse(R"({"nodes":{"A":{"model":"SC FFT HD","settings":{"Server":0}},"B":{"model":"SC A2k","settings":{"Server":1}}}})"));
        assert(pin.single);
        // Parameters save as strings; an SC Output (kept out of sources) never pins.
        pin=hooks->pin(ofJson::parse(R"({"nodes":{"A":{"model":"SC FFT HD","settings":{"Server":"1"}},"O":{"model":"SC Output","settings":{"Server":"0"}}}})"));
        assert(pin.index==1&&!pin.single);
        // SC Mixer Pro holds its source on one server unless Follow Graph
        // Server is on with every strip on one server.
        const auto mixerPro=[](const std::string& follow,int a,int b) {
            return ofJson::parse(R"({"nodes":{"M":{"model":"Macro","settings":{},"graph":{"nodes":{"SC_Mixer_Pro_1":{"model":"SC Mixer Pro","settings":{)"
                +follow+R"("mixerPro":{"tracks":[{"server":)"+std::to_string(a)+R"(},{"server":)"+std::to_string(b)+R"(}],"buses":[],"returns":[]}}}}}}}})");
        };
        // Not following, its strips play only on the server they name: the
        // source is pinned there (a following one goes anywhere; strips on
        // several servers keep it single).
        pin=hooks->pin(mixerPro("",1,1));assert(!pin.single&&pin.index==1&&pin.reason.find("Follow Graph Server")!=std::string::npos);
        pin=hooks->pin(mixerPro(R"("Follow_Graph_Server":"0",)",0,0));assert(!pin.single&&pin.index==0);
        pin=hooks->pin(mixerPro(R"("Follow_Graph_Server":"1",)",0,0));assert(!pin.single&&pin.index<0);
        pin=hooks->pin(mixerPro(R"("Follow_Graph_Server":"1",)",1,1));assert(!pin.single&&pin.index<0);
        pin=hooks->pin(mixerPro(R"("Follow_Graph_Server":"1",)",0,1));assert(pin.single);
        pin=hooks->pin(mixerPro("",0,1));assert(pin.single);
        assert(hooks->carries("SC Mixer Test Tone")&&hooks->carries("Router ScBus")&&!hooks->carries("Router Float"));
        assert(hooks->name(&s1)=="0"&&hooks->name(&s2)=="1"&&hooks->name(&s3).empty());
    }
    {
        // Audio Servers: Alternate, an SC Output on each server. At rest the
        // graph runs on the first server whose walk reaches the bridge; the
        // other runs only the bridge, with silent inputs. A morph in place that
        // swaps its audio island wholesale (Tone removed, Effect added: BLOOBS'
        // analogBox replaced by a Macro) builds the island on the free server
        // and leaves the origin's synths alone; at completion the island stays
        // there and the next morph alternates back. A node kept on both sides
        // keeps the morph on one server, and Details says why.
        const auto isleA=scratch+"/isle-a",isleB=scratch+"/isle-b",shared=scratch+"/isle-shared";
        write(isleA,"modules.json",{{"SC Mixer Test Tone",{{"01",{0,0}}}},{"Router ScBus",{{"01",{0,0}}}}});
        write(isleA,"SC_Mixer_Test_Tone_1.json",{{"Freq",220}});write(isleA,"Router_ScBus_1.json",{{"Name","Audio"}});
        write(isleA,"connections.json",{{"SC_Mixer_Test_Tone_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
        write(isleB,"modules.json",{{"SC Mixer Test Effect",{{"01",{0,0}}}},{"Router ScBus",{{"01",{0,0}}}}});
        write(isleB,"SC_Mixer_Test_Effect_1.json",{{"Freq",440}});write(isleB,"Router_ScBus_1.json",{{"Name","Audio"}});
        write(isleB,"connections.json",{{"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
        write(shared,"modules.json",{{"SC Mixer Test Tone",{{"01",{0,0}}}},{"SC Mixer Test Effect",{{"01",{0,0}}}},{"Router ScBus",{{"01",{0,0}}}}});
        write(shared,"SC_Mixer_Test_Tone_1.json",{{"Freq",220}});write(shared,"SC_Mixer_Test_Effect_1.json",{{"Freq",440}});write(shared,"Router_ScBus_1.json",{{"Name","Audio"}});
        write(shared,"connections.json",{{"SC_Mixer_Test_Tone_1",{{"Out",{{"SC_Mixer_Test_Effect_1",{{"In",nullptr}}}}}}},{"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
        auto& output1=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        auto& output2=parent.createNode(registry->create("SC Output"),2).getNodeModel();output2.getParameter<int>("Server")=1;
        MacroMixerRuntime r(parent);r.setAlternateServers(true);
        assert(r.setContract(c)&&r.importMacro(isleA)&&r.importMacro(isleB)&&r.importMacro(shared));
        r.request(isleA,0,0,true);r.update(0);output1.getParameter<nodePort>("In")=bus(r);output2.getParameter<nodePort>("In")=bus(r);
        recompute(m1,s1);recompute(m2,s2);r.update(1);
        auto* graph=r.origin();
        auto& tone=static_cast<TestTone&>(graph->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        assert(graph->placement->instance==&s1&&tone.getLastSynthID(&s1)>0&&tone.getLastSynthID(&s2)==-1);
        assert(bridgeID(s1)>0&&bridgeID(s2)>0&&m1.hasSoundNodes()&&!m2.hasSoundNodes());
        const int toneID=tone.getLastSynthID(&s1),toneBus=tone.getOutputBusIndex(&s1,0);
        const auto preview=r.sourceDetails(r.importedMacro(isleB)->id());
        assert(std::any_of(preview.begin(),preview.end(),[](const auto& d){return d.kind=="audio"&&d.message.find("replaced wholesale")!=std::string::npos;}));
        // Tone kept on both sides keeps the island on the playing server (its sound morphs there).
        const auto sharedPreview=r.sourceDetails(r.importedMacro(shared)->id());
        assert(std::any_of(sharedPreview.begin(),sharedPreview.end(),[](const auto& d){return d.kind=="audio"&&d.message.find("SC_Mixer_Test_Tone_1 is shared by both sides")!=std::string::npos;}));
        r.request(isleB,1000,0,true);r.update(10);
        assert(r.morphingInPlace()&&r.origin()==graph&&graph->placement->instance==&s1&&graph->placement->incoming==&s2);
        assert(r.audioPlacement()=="audio: server 0 -> 1"&&r.status().find("audio: server 0 -> 1")!=std::string::npos);
        const auto& moved=r.sourceDetails(r.importedMacro(isleB)->id());
        assert(std::any_of(moved.begin(),moved.end(),[](const auto& d){return d.kind=="audio"&&d.message=="audio: server 0 -> 1";}));
        auto& effect=static_cast<TestEffect&>(graph->container->getDynamicNodes().at("SC Mixer Test Effect").at(1)->getNodeModel());
        recompute(m1,s1);recompute(m2,s2);r.update(20);
        // The island is built on server 1 only. Server 0 keeps the origin's
        // synth and bus (it only inserts the internal bridge), and the origin
        // never runs on server 1.
        assert(effect.getLastSynthID(&s2)>0&&effect.getLastSynthID(&s1)==-1&&tone.getLastSynthID(&s2)==-1);
        assert(tone.getLastSynthID(&s1)==toneID&&tone.getOutputBusIndex(&s1,0)==toneBus);
        r.update(520);near(r.progress(),.5);
        auto* internal=MacroMixerMixing::object<nodePort>(graph->read("audio")).getNodeRef();
        const int insideA=internal->getLastSynthID(&s1),insideB=internal->getLastSynthID(&s2),effectID=effect.getLastSynthID(&s2);
        assert(insideA>0&&insideB>0);
        // Each server's copy of the blend hears only its own side (256: the
        // server's silent bus, its first allocation) and applies its gain.
        near(s1.synths.at(insideA).controls.at("gainA"),.5);near(s2.synths.at(insideB).controls.at("gainB"),.5);
        near(s1.synths.at(insideA).controls.at("inA"),toneBus);near(s1.synths.at(insideA).controls.at("inB"),256);
        near(s2.synths.at(insideB).controls.at("inB"),effect.getOutputBusIndex(&s2,0));near(s2.synths.at(insideB).controls.at("inA"),256);
        r.finish();r.update(530);recompute(m1,s1);recompute(m2,s2);r.update(531);r.update(532);
        assert(!r.transitioning()&&r.origin()==graph&&graph->placement->instance==&s2&&!graph->placement->incoming);
        // The island stays on server 1 (the same synth); server 0 runs only the silent bridge.
        assert(effect.getLastSynthID(&s2)==effectID&&effect.getLastSynthID(&s1)==-1&&!m1.hasSoundNodes()&&m2.hasSoundNodes());
        assert(bridgeID(s1)>0&&bridgeID(s2)>0);
        r.request(isleA,1000,0,true);r.update(600);
        assert(r.morphingInPlace()&&graph->placement->incoming==&s1&&r.audioPlacement()=="audio: server 1 -> 0");
        recompute(m1,s1);recompute(m2,s2);r.update(610);r.finish();r.update(620);recompute(m1,s1);recompute(m2,s2);r.update(621);r.update(622);
        assert(!r.transitioning()&&graph->placement->instance==&s1&&m1.hasSoundNodes()&&!m2.hasSoundNodes());
        // Tone is kept on both sides. With Single (and plain Alternate) the
        // morph keeps it and runs on the one server; with "Alternate, swap
        // shared" the kept Tone is replaced so the island swaps wholesale and
        // the incoming audio starts on the free server, the old Tone playing
        // on until completion.
        // The Tone re-added by the last morph (the first one retired with isleB).
        auto& liveTone=static_cast<TestTone&>(graph->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        const int liveToneID=liveTone.getLastSynthID(&s1);assert(liveToneID>0);
        r.setAlternateServers(false);
        r.request(shared,1000,0,true);r.update(700);
        assert(r.morphingInPlace()&&!graph->placement->incoming);
        assert(r.audioPlacement()=="audio: server 0, stays: Audio Servers is Single");
        recompute(m1,s1);recompute(m2,s2);r.update(710);
        auto& sharedEffect=static_cast<TestEffect&>(graph->container->getDynamicNodes().at("SC Mixer Test Effect").at(1)->getNodeModel());
        assert(sharedEffect.getLastSynthID(&s1)>0&&sharedEffect.getLastSynthID(&s2)==-1&&liveTone.getLastSynthID(&s1)==liveToneID);
        r.finish();r.update(720);recompute(m1,s1);recompute(m2,s2);r.update(721);r.update(722);
        assert(!r.transitioning()&&graph->placement->instance==&s1&&!m2.hasSoundNodes());
        r.setAlternateServers(true);r.setSwapSharedAudio(true);
        r.request(isleA,1000,0,true);r.update(750);
        assert(r.morphingInPlace()&&graph->placement->incoming==&s2&&r.audioPlacement()=="audio: server 0 -> 1");
        const auto& swapped=r.sourceDetails(r.importedMacro(isleA)->id());
        assert(std::any_of(swapped.begin(),swapped.end(),[](const auto& d){return d.kind=="replaced"&&d.location=="SC_Mixer_Test_Tone_1"&&d.message.find("swaps shared audio")!=std::string::npos;}));
        recompute(m1,s1);recompute(m2,s2);r.update(760);
        TestTone* newTone=nullptr;
        for(auto& entry:graph->container->getDynamicNodes().at("SC Mixer Test Tone")) if(&entry.second->getNodeModel()!=&liveTone) newTone=static_cast<TestTone*>(&entry.second->getNodeModel());
        assert(newTone&&newTone->getLastSynthID(&s2)>0&&newTone->getLastSynthID(&s1)==-1&&liveTone.getLastSynthID(&s1)==liveToneID);
        r.finish();r.update(770);recompute(m1,s1);recompute(m2,s2);r.update(771);r.update(772);
        assert(!r.transitioning()&&graph->placement->instance==&s2&&!m1.hasSoundNodes()&&m2.hasSoundNodes());
        // The target's audio is pinned to server 0 (a node saved with Server
        // 0) while the running graph plays on server 1 and the morph cannot
        // move its island (its Router feeds another node): in place it would
        // be silent, so the graph falls back and the second graph starts on
        // server 0.
        const auto pinned=scratch+"/isle-pinned";fs::copy(isleB,pinned,fs::copy_options::recursive);
        auto pinnedModules=ofLoadJson(pinned+"/modules.json");pinnedModules["SC Mixer Test Pinned"]["01"]={0,0};pinnedModules["SC Mixer Test Effect"]["02"]={0,0};ofSaveJson(pinned+"/modules.json",pinnedModules);
        write(pinned,"SC_Mixer_Test_Pinned_1.json",{{"Freq",110},{"Server",0}});write(pinned,"SC_Mixer_Test_Effect_2.json",{{"Freq",330}});
        auto pinnedCables=ofLoadJson(pinned+"/connections.json");pinnedCables["Router_ScBus_1"]["Value"]["SC_Mixer_Test_Effect_2"]["In"]=nullptr;ofSaveJson(pinned+"/connections.json",pinnedCables);
        assert(r.importMacro(pinned));
        r.request(pinned,1000,0,true);r.update(900);
        assert(r.morphFallback()&&r.destination()!=graph&&r.destination()->placement->instance==&s1);
        assert(r.status().find("plays only on server 0")!=std::string::npos&&r.audioPlacement().rfind("audio: server 1 -> 0",0)==0);
        recompute(m1,s1);recompute(m2,s2);r.update(910);r.finish();r.update(920);
        recompute(m1,s1);recompute(m2,s2);r.update(921);r.update(922);
        assert(!r.transitioning()&&r.origin()->placement->instance==&s1);
        recompute(m1,s1);recompute(m2,s2);r.update(923);assert(m1.hasSoundNodes()&&!m2.hasSoundNodes());
        r.clear();output1.getParameter<nodePort>("In")=nodePort();output2.getParameter<nodePort>("In")=nodePort();
        recompute(m1,s1);recompute(m2,s2);parent.clearContainer();recompute(m1,s1);recompute(m2,s2);assert(TestTone::live==0&&ofxSCSynth::live==0);
    }
    {
        // Two graphs (a morph that falls back, then Mix mode) crossfade across
        // servers: the destination starts on the free server, each server's
        // bridge copy applies its side's gain with the other side silent, at
        // completion the origin's server holds no graph and the next
        // transition goes back.
        const auto fallback=scratch+"/servers-fallback";fs::copy(b,fallback,fs::copy_options::recursive);
        write(fallback,"Router_ScBus_1.json",{{"Name","Audio"},{"Exclude_From_Snapshots",true}});
        auto& output1=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        auto& output2=parent.createNode(registry->create("SC Output"),2).getNodeModel();output2.getParameter<int>("Server")=1;
        MacroMixerRuntime r(parent);r.setAlternateServers(true);assert(r.setContract(c)&&r.importMacro(a)&&r.importMacro(fallback));
        r.request(a,0,0,true);r.update(0);output1.getParameter<nodePort>("In")=bus(r);output2.getParameter<nodePort>("In")=bus(r);
        recompute(m1,s1);recompute(m2,s2);r.update(1);
        const auto* origin=r.origin();assert(origin->placement->instance==&s1&&m1.hasSoundNodes()&&!m2.hasSoundNodes());
        auto& originTone=static_cast<TestTone&>(origin->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        const int originToneID=originTone.getLastSynthID(&s1);
        r.request(fallback,1000,0,true);r.update(10);
        assert(r.morphFallback()&&r.destination()!=origin&&r.destination()->placement->instance==&s2&&r.audioPlacement()=="audio: server 0 -> 1");
        const auto& details=r.sourceDetails(r.importedMacro(fallback)->id());
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="audio"&&d.message=="audio: server 0 -> 1";}));
        recompute(m1,s1);recompute(m2,s2);r.update(20);assert(r.progress()==0);
        auto& destinationTone=static_cast<TestTone&>(r.destination()->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel());
        assert(destinationTone.getLastSynthID(&s2)>0&&destinationTone.getLastSynthID(&s1)==-1);
        assert(originTone.getLastSynthID(&s2)==-1&&originTone.getLastSynthID(&s1)==originToneID);
        r.update(520);const int id1=bridgeID(s1),id2=bridgeID(s2);assert(id1>0&&id2>0);
        near(s1.synths.at(id1).controls.at("gainA"),.5);near(s2.synths.at(id2).controls.at("gainB"),.5);
        near(s1.synths.at(id1).controls.at("inB"),256);near(s2.synths.at(id2).controls.at("inA"),256);
        r.update(1100);assert(!r.transitioning()&&r.origin()->placement->instance==&s2);
        r.update(1101);recompute(m1,s1);recompute(m2,s2);r.update(1102);
        assert(!m1.hasSoundNodes()&&m2.hasSoundNodes());
        r.request(a,1000,0,false);r.update(1200);
        assert(r.transitioning()&&r.destination()->placement->instance==&s1&&r.audioPlacement()=="audio: server 1 -> 0");
        recompute(m1,s1);recompute(m2,s2);r.update(1210);r.update(2300);assert(!r.transitioning());
        r.update(2301);recompute(m1,s1);recompute(m2,s2);r.update(2302);
        assert(m1.hasSoundNodes()&&!m2.hasSoundNodes());
        // Single keeps a placed graph where it is: no second server.
        r.setAlternateServers(false);r.request(fallback,1000,0,false);r.update(2400);
        assert(r.destination()->placement->instance==&s1&&r.audioPlacement()=="audio: server 0, stays: Audio Servers is Single");
        recompute(m1,s1);recompute(m2,s2);r.update(2410);r.update(3500);r.update(3501);recompute(m1,s1);recompute(m2,s2);r.update(3502);
        assert(!r.transitioning()&&m1.hasSoundNodes()&&!m2.hasSoundNodes());
        r.clear();output1.getParameter<nodePort>("In")=nodePort();output2.getParameter<nodePort>("In")=nodePort();
        recompute(m1,s1);recompute(m2,s2);parent.clearContainer();recompute(m1,s1);recompute(m2,s2);assert(TestTone::live==0&&ofxSCSynth::live==0);
    }
    {
        // A stale /synced must not release a newer retirement, including when
        // all external Outputs have been removed. Offline capture has no reply.
        auto first=std::make_shared<int>(1),second=std::make_shared<int>(2);
        std::weak_ptr<int> firstWeak=first,secondWeak=second;
        const auto firstTicket=m1.requestMixerRouting(first);first.reset();m1.recomputeGraph();
        const auto secondTicket=m1.requestMixerRouting(second);second.reset();s1.acknowledge();
        assert(firstWeak.expired()&&!secondWeak.expired()&&m1.mixerRoutingReady(firstTicket)&&!m1.mixerRoutingReady(secondTicket));
        recompute(m1,s1);assert(secondWeak.expired()&&m1.mixerRoutingReady(secondTicket));
        s1.nrtCapturing=s1.nrtCaptureOnly=true;
        auto offline=std::make_shared<int>(3);std::weak_ptr<int> offlineWeak=offline;
        const auto offlineTicket=m1.requestMixerRouting(offline);offline.reset();m1.recomputeGraph();
        assert(offlineWeak.expired()&&m1.mixerRoutingReady(offlineTicket)&&s1.syncs.empty());
        s1.nrtCapturing=s1.nrtCaptureOnly=false;
    }
    // Policy coefficients/options use the actual private processor, separate from the NRT DSP test.
    for(const auto& spec:std::vector<std::pair<std::string,std::pair<float,float>>>{{"linear",{.5,.5}},{"equal_power",{std::sqrt(.5f),std::sqrt(.5f)}},{"switch",{0,1}},{"fade_out",{0,0}}}) {
        const auto& policy=MacroMixerMixing::registry().policy("ScBus",spec.first);
        const auto options=policy.options(ofJson::object());
        assert(options["channels"]==2 && options["smoothing_ms"]==20);
        for(const auto& bad:std::vector<ofJson>{{{"channels",0}},{{"channels",129}},{{"channels",1.5}},{{"smoothing_ms",-1}},{{"unknown",1}}}) {
            bool rejected=false;try{policy.options(bad);}catch(...){rejected=true;}assert(rejected);
        }
        auto fn=policy.createProcessor();TestTone aNode,bNode;aNode.setup();bNode.setup();
        auto av=MacroMixerMixing::object("ScBus",nodePort(0,&aNode)),bv=MacroMixerMixing::object("ScBus",nodePort(0,&bNode));
        auto value=fn(av,bv,.5f,options);auto* bridge=MacroMixerMixing::object<nodePort>(value).getNodeRef();
        bridge->buildSynth(&s1);bridge->createSynth(&s1);bridge->graphDidRebuild(&s1,true);int id=bridge->getLastSynthID(&s1);
        near(s1.synths.at(id).controls.at("gainA"),spec.second.first);near(s1.synths.at(id).controls.at("gainB"),spec.second.second);
        near(s1.synths.at(id).controls.at("smoothing"),.02);
        fn(av,bv,0,options);near(s1.synths.at(id).controls.at("gainA"),1);near(s1.synths.at(id).controls.at("gainB"),0);
        fn(av,bv,1,options);near(s1.synths.at(id).controls.at("gainA"),0);near(s1.synths.at(id).controls.at("gainB"),1);
        bridge->free(&s1);
    }
    auto& out=parent.createNode(registry->create("SC Output"),1).getNodeModel();
    auto& unrelated=parent.createNode(registry->create("SC Mixer Test Tone"),2).getNodeModel();
    auto& otherOut=parent.createNode(registry->create("SC Output"),2).getNodeModel();
    assert(parent.createConnectionFromInfo("SC_Mixer_Test_Tone_2","Out","SC_Output_2","In"));recompute(m1,s1);
    auto& tone=static_cast<TestTone&>(unrelated);const int outsideBus=tone.getOutputBusIndex(&s1,0),outsideID=tone.getLastSynthID(&s1);
    {
        MacroMixerRuntime r(parent);assert(r.setContract(c));assert(r.importMacro(a)&&r.importMacro(b));assert(s1.synths.size()==2); // Import staging cannot start audio, including source SC Outputs.
        std::vector<MacroMixerDiagnostic> errors;
        auto graph=MacroMixerSource::instantiate(parent,r.importedMacro(a),errors);
        assert(graph && !graph->container->getDynamicNodes().count("SC Output")); graph.reset();
        const auto entries=r.macros();r.requestMacro(entries[0]->id(),0,0);r.update(0);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);
        int id=bridgeID(s1);assert(id>0);const int originLive=TestTone::live;auto* bridge=bus(r).getNodeRef();
        // Loading a definition without a cable edit must also release readiness.
        synthdefsLoading=true;recompute(m1,s1);synthdefsLoading=false;recompute(m1,s1);
        assert(MacroMixerMixing::registry().policy("ScBus","linear").ready(r.outputValue("audio")));
        r.requestMacro(entries[1]->id(),100,0);r.update(0);out.getParameter<nodePort>("In")=bus(r);assert(bus(r).getNodeRef()==bridge);
        r.update(100);assert(r.transitioning()&&r.progress()==0); // No fade/promotion before SC graph acknowledgement.
        synthdefsLoading=true;recompute(m1,s1);r.update(120);assert(r.transitioning()&&r.progress()==0);
        assert(s1.synths.count(id)&&tone.getLastSynthID(&s1)==outsideID);
        synthdefsLoading=false;m1.recomputeGraph();r.update(130);assert(r.transitioning()&&r.progress()==0);s1.acknowledge();near(s1.synths.at(id).controls.at("gainA"),1);near(s1.synths.at(id).controls.at("gainB"),0);assert(tone.getLastSynthID(&s1)==outsideID&&tone.getOutputBusIndex(&s1,0)==outsideBus);
        r.update(180);near(s1.synths.at(id).controls.at("gainA"),.5);near(s1.synths.at(id).controls.at("gainB"),.5);
        const auto state=r.saveState();MacroMixerRuntime copy(parent);assert(copy.restoreState(state));copy.update(0);assert(bus(copy).getNodeRef()!=bridge);
        assert(copy.origin()->container->getDynamicNodes().at("SC Mixer Test Tone").at(1)->getNodeModel().getParameter<float>("Freq").get()==660);
        r.update(230);assert(!r.transitioning());r.update(231);assert(TestTone::live>=originLive+1); // Old graph stays alive until manager rewiring.
        const int beforeRetirement=TestTone::live;m1.recomputeGraph();assert(TestTone::live==beforeRetirement);
        s1.acknowledge();assert(TestTone::live==beforeRetirement-2);
        assert(tone.getLastSynthID(&s1)==outsideID&&s1.synths.count(outsideID));
        auto held=r.macro(entries[1]->id());fs::rename(b,b+"-missing");assert(!r.importMacro(b,true)&&r.macro(entries[1]->id())==held);
        MacroMixerRuntime missing(parent);assert(missing.restoreState(state));missing.update(0);assert(!missing.origin()&&!missing.diagnostics().empty());
        r.update(232);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);assert(s1.synths.count(id));fs::rename(b+"-missing",b);
        // Explicit Output server changes preserve the unrelated graph's existing route.
        out.getParameter<int>("Server")=1;recompute(m2,s2);assert(bridgeID(s2)>0&&tone.getLastSynthID(&s1)==outsideID);
        out.getParameter<int>("Server")=0;recompute(m1,s1);
        // Failed bus reservation is transactional and leaves the old synths/buses running.
        const auto before=s1.synths;const int capacity=s1.busLimit;s1.busLimit=int(s1.buses.size());
        r.requestMacro(entries[0]->id(),100,0);r.update(300);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);
        assert(s1.synths.size()==before.size()&&tone.getOutputBusIndex(&s1,0)==outsideBus);
        r.update(350);assert(!r.transitioning()&&!r.diagnostics().empty());s1.busLimit=capacity;
        r.update(351);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);
        for(int i=0;i<8;++i) {
            const auto time=400+i*10;r.requestMacro(entries[i%2]->id(),0,0);r.update(time);recompute(m1,s1);
            r.update(time+1);r.update(time+2);recompute(m1,s1);r.update(time+3);
            assert(TestTone::live<=originLive+4);assert(tone.getLastSynthID(&s1)==outsideID);
        }
        r.clear();out.getParameter<nodePort>("In")=nodePort();recompute(m1,s1);assert(tone.getLastSynthID(&s1)==outsideID);
    }
    {
        // An unknown saved setting on one SC node replaces only that node: the
        // rest of the graph morphs in place and the old and new tones blend at
        // the effect input through a private ScBus bridge.
        const auto configured=scratch+"/configured"; fs::copy(b,configured,fs::copy_options::recursive);
        write(configured,"SC_Mixer_Test_Tone_1.json",{{"Freq",660},{"Custom",1}});
        MacroMixerRuntime r(parent);assert(r.setContract(c)&&r.importMacro(a)&&r.importMacro(configured));
        r.request(a,0,0,true);r.update(0);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);
        const auto* origin=r.origin();const int originLive=TestTone::live;
        const auto details=r.sourceDetails(r.importedMacro(configured)->id());
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="replaced"&&d.location=="SC_Mixer_Test_Tone_1"&&d.message.find("(Custom)")!=std::string::npos;}));
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="mix"&&d.location=="SC_Mixer_Test_Effect_1 / In"&&d.type=="ScBus";}));
        r.request(configured,100,0,true);r.update(10);out.getParameter<nodePort>("In")=bus(r);
        assert(r.morphingInPlace()&&r.origin()==origin&&TestTone::live==originLive+1);
        recompute(m1,s1);r.update(20);r.update(21);r.finish();r.update(22);
        recompute(m1,s1);r.update(23);r.update(24);assert(!r.transitioning()&&r.origin()==origin);
        const auto& tones=origin->container->getDynamicNodes().at("SC Mixer Test Tone");assert(tones.size()==1&&!tones.count(1));
        r.clear();out.getParameter<nodePort>("In")=nodePort();recompute(m1,s1);
    }
    {
        // A Router setting cannot be split off: both graphs keep running and
        // the ScBus output crossfades with the router policy instead.
        const auto configured=scratch+"/configured-router"; fs::copy(b,configured,fs::copy_options::recursive);
        write(configured,"Router_ScBus_1.json",{{"Name","Audio"},{"Exclude_From_Snapshots",true}});
        MacroMixerRuntime r(parent);assert(r.setContract(c)&&r.importMacro(a)&&r.importMacro(configured));
        r.request(a,0,0,true);r.update(0);out.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);
        const auto* origin=r.origin();const int originLive=TestTone::live;
        r.request(configured,100,0,true);r.update(10);out.getParameter<nodePort>("In")=bus(r);
        assert(r.morphFallback()&&r.origin()==origin&&r.destination()!=origin&&TestTone::live>originLive);
        assert(r.status().find("saved configuration differs (Exclude_From_Snapshots)")!=std::string::npos);
        recompute(m1,s1);r.update(20);assert(r.progress()==0); // Timing starts once the server acknowledged the routing.
        r.update(70);int id=bridgeID(s1);assert(id>0);
        near(s1.synths.at(id).controls.at("gainA"),.5);near(s1.synths.at(id).controls.at("gainB"),.5);
        r.update(120);assert(!r.transitioning()&&r.origin()!=origin&&r.currentID()==r.importedMacro(configured)->id());
        r.clear();out.getParameter<nodePort>("In")=nodePort();recompute(m1,s1);
    }
    parent.clearContainer();recompute(m1,s1);recompute(m2,s2);assert(TestTone::live==0&&ofxSCSynth::live==0);
    {
        // A single external input fans into independently instantiated effects in A and B.
        const auto feed=scratch+"/feed";
        write(feed,"modules.json",{{"SC Mixer Test Effect",{{"01",{0,0}}}},{"Router ScBus",{{"01",{0,0}},{"02",{0,0}}}}});
        write(feed,"SC_Mixer_Test_Effect_1.json",{{"Freq",440}});write(feed,"Router_ScBus_1.json",{{"Name","Audio"}});write(feed,"Router_ScBus_2.json",{{"Name","Feed"}});
        write(feed,"connections.json",{{"Router_ScBus_2",{{"Value",{{"SC_Mixer_Test_Effect_1",{{"In",nullptr}}}}}}},{"SC_Mixer_Test_Effect_1",{{"Out",{{"Router_ScBus_1",{{"Value",nullptr}}}}}}}});
        auto inputContract=c;inputContract.routers.push_back({"feed","Feed","ScBus","",ofJson::object(),"Input"});
        auto& source=static_cast<TestTone&>(parent.createNode(registry->create("SC Mixer Test Tone"),1).getNodeModel());
        auto& output=parent.createNode(registry->create("SC Output"),1).getNodeModel();
        MacroMixerRuntime r(parent);assert(r.setContract(inputContract));assert(r.setInputs({{"feed",MacroMixerMixing::object("ScBus",nodePort(0,&source))}}));
        assert(r.importMacro(feed));r.requestMacro(r.macros()[0]->id(),0,0);r.update(0);output.getParameter<nodePort>("In")=bus(r);recompute(m1,s1);r.update(1);
        auto& effect=static_cast<TestEffect&>(r.origin()->container->getDynamicNodes().at("SC Mixer Test Effect").at(1)->getNodeModel());
        assert(effect.inputBus.at(&s1)==source.getOutputBusIndex(&s1,0));assert(r.saveState()["inputs"]["feed"].is_null());
        r.clear();output.getParameter<nodePort>("In")=nodePort();recompute(m1,s1);parent.clearContainer();recompute(m1,s1);
        const auto invalid=scratch+"/invalid";write(invalid,"modules.json",{{"SC Output",{{"01",{0,0}}}}});write(invalid,"SC_Output_1.json",ofJson::object());write(invalid,"connections.json",ofJson::object());
        MacroMixerRuntime bad(parent);assert(bad.setContract(c)&&!bad.importMacro(invalid));assert(TestTone::live==0&&ofxSCSynth::live==0);
    }
    {
        // Real node persistence, external ScBus cables and independent clipboard copies.
        auto& n=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1);auto& mix=static_cast<ofxOceanodeMacroMixer&>(n.getNodeModel());
        c.routers[0].policy="equal_power";assert(mix.setRouterContract(c));assert(mix.importMacroFolder(a));mix.getParameter<float>("Duration_ms")=0;
        auto& output=parent.createNode(registry->create("SC Output"),1);assert(parent.createConnectionFromInfo("MacroMixer_1","Audio","SC_Output_1","In"));
        mix.getParameter<void>("Start!").trigger();parent.update();recompute(m1,s1);parent.update();
        const auto folder=scratch+"/saved";n.savePreset(folder);const auto original=mix.getParameter<nodePort>("Audio").get().getNodeRef();assert(original);
        parent.saveClipboardModulesAndConnections({&n,&output},{0,0});assert(parent.loadClipboardModulesAndConnections({40,40},false));parent.update();recompute(m1,s1);parent.update();
        auto& clone=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(2)->getNodeModel());
        assert(clone.getParameter<nodePort>("Audio").get().getNodeRef()!=original);
        assert(clone.mixerRuntime()->origin()->container!=mix.mixerRuntime()->origin()->container);
        clone.deactivate();recompute(m1,s1);assert(mix.getParameter<nodePort>("Audio").get().getNodeRef()==original);
        parent.clearContainer();recompute(m1,s1);
        auto& reload=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1);reload.loadPresetBeforeConnections(folder);
        parent.createNode(registry->create("SC Output"),1);assert(parent.createConnectionFromInfo("MacroMixer_1","Audio","SC_Output_1","In"));
        assert(reload.loadPreset(folder));parent.update();recompute(m1,s1);parent.update();parent.clearContainer();recompute(m1,s1);
    }
    
    {
        // The mixed port retains its private bridge through a real Macro router and paste.
        auto& n=parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1);
        auto& macro=static_cast<ofxOceanodeNodeMacro&>(n.getNodeModel());auto child=macro.getContainer();
        auto& mix=static_cast<ofxOceanodeMacroMixer&>(child->createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        assert(mix.setRouterContract(c)&&mix.importMacroFolder(a));mix.getParameter<float>("Duration_ms")=0;
        auto& router=child->createNode(registry->create("Router ScBus"),1).getNodeModel();router.getParameter<std::string>("Name")="Mixed";
        assert(child->createConnectionFromInfo("MacroMixer_1","Audio","Router_ScBus_1","Value"));
        parent.createNode(registry->create("SC Output"),1);assert(parent.createConnectionFromInfo("Macro_1","Mixed","SC_Output_1","In"));
        mix.getParameter<void>("Start!").trigger();parent.update();recompute(m1,s1);parent.update();
        auto* original=macro.getParameter<nodePort>("Mixed").get().getNodeRef();assert(original&&original->getLastSynthID(&s1)>0);
        parent.saveClipboardModulesAndConnections({&n,parent.getDynamicNodes().at("SC Output").at(1).get()},{0,0});
        assert(parent.loadClipboardModulesAndConnections({30,30},false));parent.update();recompute(m1,s1);parent.update();
        auto& copy=static_cast<ofxOceanodeNodeMacro&>(parent.getDynamicNodes().at("Macro").at(2)->getNodeModel());
        assert(copy.getParameter<nodePort>("Mixed").get().getNodeRef()!=original);
        assert(innerMixer(copy).mixerRuntime()->origin()->container!=mix.mixerRuntime()->origin()->container);
        copy.getParameter<bool>("Active")=false;recompute(m1,s1);assert(original->getLastSynthID(&s1)>0);
        parent.clearContainer();recompute(m1,s1);recompute(m2,s2);
    }
    assert(liveGui==0&&TestTone::live==0&&ofxSCSynth::live==0);
    assert(std::find(s1.messages.begin(),s1.messages.end(),"/g_freeAll")==s1.messages.end());
    fs::current_path(cwd);std::cout<<"ScBus mixer passed: production traversal/Output, private policy gains/options, input routing, staged silence, SynthDef readiness, repeated crossfades, stable unrelated buses/synths, retirement, source failures, explicit server routing, allocation rollback, saved destination/cables, independent clipboard and real Macro embedding/copies; server allocator and placement hooks, per-input bridge affinity, an audio island morphed in place onto the free server and kept there, a shared node keeping one server, and crossfades alternating across servers.\n";
}
'''
if __name__=='__main__':
    mixer.main(extra_includes=[mixer.ROOT/'tests/support/macro_mixer_sc',*addon_include_dirs(mixer.ROOT.parent/'ofxOceanodeSuperCollider'),mixer.ROOT.parent/'ofxOsc/src',mixer.ROOT.parent/'ofxOsc/libs/oscpack/src'],
        extra_sources=[str(mixer.ROOT.parent/'ofxOsc/src/ofxOscMessage.cpp')]+[str(addon_source(mixer.ROOT.parent/'ofxOceanodeSuperCollider',name)) for name in ('scNode.cpp','scOutput.cpp','MacroMixerSCBus.cpp','MacroMixerServerAllocator.cpp')])
