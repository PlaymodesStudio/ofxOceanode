#!/usr/bin/env python3
"""SCBus graph lifetime/routing with production traversal/manager/Output and captured server commands; no audio device."""
import macro_mixer_regression as mixer
mixer.PRELUDE += r'''
#include "SCFake.h"
#include "scNode.h"
#include "scOutput.h"
#include "serverManager.h"
#include "MacroMixerSCBus.h"
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
static ofxSCServer* setupServer=nullptr;
static bool synthdefsLoading=false;
'''
BASE=mixer.native_production

def production():
    result=BASE()+'\nstd::map<ofxSCServer*,int> serverManager::serverSampleRates;\n'
    source=(mixer.ROOT.parent/'ofxOceanodeSuperCollider/src/serverManager.cpp').read_text()
    for signature in ['serverManager::serverManager()', 'serverManager::~serverManager()',
        'void serverManager::addOutput(', 'void serverManager::removeOutput(',
        'void serverManager::setVolume(', 'void serverManager::setDelay(',
        'void serverManager::setStereoMix(', 'void serverManager::setStereoMixSize(',
        'void serverManager::watchMixerGraphSync()', 'void serverManager::releaseGraphBusses()', 'void serverManager::teardownGraphForPresetLoad()',
        'void serverManager::recomputeGraph()']:
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
    registry->registerModel<TestEffect>("SuperCollider/Test");
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
    fs::current_path(cwd);std::cout<<"ScBus mixer passed: production traversal/Output, private policy gains/options, input routing, staged silence, SynthDef readiness, repeated crossfades, stable unrelated buses/synths, retirement, source failures, explicit server routing, allocation rollback, saved destination/cables, independent clipboard and real Macro embedding/copies.\n";
}
'''
if __name__=='__main__':
    mixer.main(extra_includes=[mixer.ROOT/'tests/support/macro_mixer_sc',mixer.ROOT.parent/'ofxOceanodeSuperCollider/src',mixer.ROOT.parent/'ofxOsc/src',mixer.ROOT.parent/'ofxOsc/libs/oscpack/src'],
        extra_sources=[str(mixer.ROOT.parent/'ofxOsc/src/ofxOscMessage.cpp')]+[str(mixer.ROOT.parent/'ofxOceanodeSuperCollider/src'/name) for name in ('scNode.cpp','scOutput.cpp','MacroMixerSCBus.cpp')])
