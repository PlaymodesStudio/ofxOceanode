#!/usr/bin/env python3
"""Native graph-parameter morphs, differing graphs, controls, clocks and persistence.

Reuses the ASan/UBSan production Macro loader fixture. No window or hardware.
"""
import macro_mixer_regression as mixer

mixer.PRELUDE += '\n#include "bufferNode.h"\n#include "bufferHeaderPro.h"\n#include "dotMaker.h"\n#include "oscillator.h"\n#include "ranger.h"\n'
# Use production Timestamp code without constructing the application clock singleton.
time_source = (mixer.ROOT / 'src/ofxOceanodeTime.cpp').read_text()
for signature in ('Timestamp::Timestamp()', 'Timestamp::Timestamp(int64_t',
                  'uint64_t Timestamp::epochMicroseconds()', 'int64_t Timestamp::epochMicrosecondsSigned()',
                  'uint64_t Timestamp::epochMilliseconds()', 'void Timestamp::update()',
                  'void Timestamp::substractMs(', 'bool Timestamp::operator==(', 'bool Timestamp::operator!=(',
                  'bool Timestamp::operator<(', 'bool Timestamp::operator<=(', 'bool Timestamp::operator>(', 'bool Timestamp::operator>=('):
    mixer.PRELUDE += mixer.block(time_source, signature) + '\n'

mixer.CASES = r'''
static void near(float a,float b) { if(!std::isfinite(a)||std::abs(a-b)>.0001f) { std::cerr<<a<<" != "<<b<<"\n"; std::abort(); } }
static int triggers=0, morphSetups=0;
class MorphProbe : public ofxOceanodeNodeModel {
public:
    MorphProbe():ofxOceanodeNodeModel("Morph Probe") {}
    ofParameter<float> x,driven,output,transientValue,readOnly;
    ofParameter<int> count,mode,updates,draws,configuration;
    ofParameter<bool> gate;
    ofParameter<std::string> label;
    ofParameter<std::vector<float>> floats,unequal;
    ofParameter<std::vector<int>> ints;
    ofParameter<ofColor> color;
    ofParameter<ofFloatColor> tint;
    ofParameter<void> trigger;
    ofEventListener listener;
    void setup() override {
        ++morphSetups; addInspectorParameter(configuration.set("Configuration",0));
        addParameter(x.set("X",2,-100,100)); addParameter(driven.set("Driven",0,-100,100));
        addParameter(count.set("Count",0,INT_MIN,INT_MAX));
        addParameterDropdown(mode,"Mode",0,{"A","B","C","D"});
        addParameter(gate.set("Gate",false)); addParameter(label.set("Label","a"));
        addParameter(floats.set("Floats",{0,2})); addParameter(ints.set("Ints",{0,2}));
        addParameter(unequal.set("Unequal",{1}));
        addParameter(color.set("Color",ofColor(0,20,40,60)));
        addParameter(tint.set("Tint",ofFloatColor(0,.2,.4,.6)));
        addParameter(trigger.set("Trigger")); listener=trigger.newListener([] { ++triggers; });
        addParameter(transientValue.set("Transient",17),ofxOceanodeParameterFlags_DisableSavePreset);
        addParameter(readOnly.set("Read Only",19),ofxOceanodeParameterFlags_ReadOnly);
        addOutputParameter(output.set("Output",0)); addOutputParameter(updates.set("Updates",0)); addOutputParameter(draws.set("Draws",0));
    }
    void update(ofEventArgs&) override { output=x.get()*x.get(); updates=updates.get()+1; }
    void draw(ofEventArgs&) override { draws=draws.get()+1; }
};
class MorphPhaseSink : public ofxOceanodeNodeModel {
public:
    MorphPhaseSink():ofxOceanodeNodeModel("Morph Phase Sink") {}
    ofParameter<float> phase;
    void setup() override { addParameter(phase.set("Phase",0,0,1),ofxOceanodeParameterFlags_DisableSavePreset); }
};
class SharedPortalProbe : public ofxOceanodeNodeModel {
public:
    ofParameter<float> value;
    SharedPortalProbe():ofxOceanodeNodeModel("Portal f") {}
    void setup() override { addParameter(value.set("Value",0)); }
};
static MorphProbe& probe(const MacroMixerSource::Graph* graph,int id=1) {
    return static_cast<MorphProbe&>(graph->container->getDynamicNodes().at("Morph Probe").at(id)->getNodeModel());
}
static auto& floating(ofxOceanodeContainer& c,int id=1) {
    return c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),id).getNodeModel();
}
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    types->registerType<std::vector<float>>("v_f"); singleton->registerType<std::vector<float>>("v_f");
    types->registerType<int>("i"); singleton->registerType<int>("i");
    types->registerType<std::vector<int>>("v_i"); singleton->registerType<std::vector<int>>("v_i");
    types->registerType<bool>("b"); singleton->registerType<bool>("b");
    types->registerType<ofColor>("c"); singleton->registerType<ofColor>("c");
    types->registerType<ofFloatColor>("c_f"); singleton->registerType<ofFloatColor>("c_f");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    types->registerType<buffer<float>*>("Buffer f"); singleton->registerType<buffer<float>*>("Buffer f");
    registry->registerModel<bufferNode<float>>("Buffer","f",0.f);
    registry->registerModel<bufferHeaderPro<float>>("Buffer","f",0.f);
    registry->registerModel<oscillator>("Generators"); registry->registerModel<ranger>("Modifiers");
    registry->registerModel<phasor>("Clock"); registry->registerModel<SharedPortalProbe>("Test");
    registry->registerModel<MorphPhaseSink>("Test"); registry->registerModel<MorphProbe>("Test"); registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("MorphTest");
    auto save=[&](const std::string& folder,bool target,bool nested=false) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("MorphSource");
        auto& out=floating(c); out.getParameter<std::string>("Name")="Value";
        auto& input=floating(c,2); input.getParameter<std::string>("Name")="Input"; input.getParameter<float>("Value")=target?42:11;
        auto& p=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),1).getNodeModel());
        p.configuration=target?1:0; // Deliberately incompatible Inspector config exercises fresh-graph fallback.
        p.x=target?6:2; p.count=target?10:0; p.mode=target?3:0; p.gate=target; p.label=target?"b":"a";
        p.floats=target?std::vector<float>{4,6}:std::vector<float>{0,2}; p.ints=target?std::vector<int>{4,6}:std::vector<int>{0,2};
        p.unequal=target?std::vector<float>{3,4}:std::vector<float>{1};
        p.color=target?ofColor(100,120,140,160):ofColor(0,20,40,60);
        p.tint=target?ofFloatColor(1,.8,.6,.4):ofFloatColor(0,.2,.4,.6);
        assert(c.createConnectionFromInfo("Morph_Probe_1","Output","Router_f_1","Value"));
        assert(c.createConnectionFromInfo("Router_f_2","Value","Morph_Probe_1","Driven"));
        // Source-only and destination-only nodes must never be paired by type alone.
        auto& extra=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),target?3:2).getNodeModel()); extra.x=target?9:7;
        if(nested) {
            auto& macro=static_cast<ofxOceanodeNodeMacro&>(c.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1).getNodeModel());
            auto& r=floating(*macro.getContainer()); r.getParameter<std::string>("Name")="NestedInput"; r.getParameter<float>("Value")=target?80:20;
            auto& q=static_cast<MorphProbe&>(macro.getContainer()->createNode(std::make_unique<MorphProbe>(),1).getNodeModel()); q.x=target?10:2;
            assert(macro.getContainer()->createConnectionFromInfo("Router_f_1","Value","Morph_Probe_1","Driven"));
        }
        c.savePreset(folder);
    };
    const auto a=scratch+"/a",b=scratch+"/b"; save(a,false); save(b,true);
    MacroMixerContract contract; contract.routers[0].policy="switch"; contract.routers[0].options={{"threshold",.9}};
    {
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(contract)); assert(runtime.importMacro(a)&&runtime.importMacro(b));
        runtime.request(a,0,0); runtime.update(0); near(runtime.value(),4);
        probe(runtime.origin()).x=4; probe(runtime.origin()).transientValue=99; probe(runtime.origin()).readOnly=88;
        runtime.update(1); near(runtime.value(),16);
        const auto* origin=runtime.origin(); const int originUpdates=probe(origin).updates;
        runtime.request(b,1000,0,true); runtime.update(10);
        // The Configuration change forces a second graph. Its controls still
        // morph, the origin keeps running and outputs follow the router policy
        // (Switch at .9 here), instead of jumping to the destination.
        const auto destinationValue=[&] { return std::get<float>(runtime.destination()->read("value")); };
        assert(runtime.morphing()&&runtime.morphMode()&&runtime.morphFallback()&&!runtime.outputsBypassed());
        near(runtime.value(),16); near(probe(runtime.destination()).x,4);
        assert(probe(origin).updates>originUpdates); assert(!runtime.destination()->container->getDynamicNodes().at("Morph Probe").count(2));
        near(probe(runtime.destination(),3).x,9); near(probe(runtime.destination()).driven,11);
        runtime.update(260); near(runtime.value(),16); near(destinationValue(),20.25); assert(probe(runtime.destination()).mode==0);
        runtime.update(510); near(runtime.value(),16); near(destinationValue(),25); // Graph evaluates 5*5, not a blend of 16 and 36.
        auto& p=probe(runtime.destination()); near(p.x,5); assert(p.count==5 && p.mode==3 && p.gate && p.label.get()=="b");
        assert(p.floats.get()==std::vector<float>({2,4}) && p.ints.get()==std::vector<int>({2,4}));
        assert(p.unequal.get()==std::vector<float>({3,4})); assert(p.color.get()==ofColor(50,70,90,110)); near(p.tint.get().r,.5);
        near(p.transientValue,17); near(p.readOnly,19); near(p.driven,26.5f);
        runtime.draw(); near(runtime.value(),16); assert(probe(origin).draws==1 && p.draws==1);
        assert(runtime.destination()->parameter("value").getName()=="Value");
        const auto saved=runtime.saveState(); assert(saved.at("morph")==true && saved.at("playback")==runtime.destinationID());
        MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0); near(copy.value(),36); assert(copy.morphMode());
        auto legacy=saved; legacy.erase("morph"); assert(copy.restoreState(legacy)); copy.update(0); assert(!copy.morphMode());
        runtime.update(960); near(runtime.value(),34.81f); assert(runtime.morphFallback()); // Past the Switch threshold.
        runtime.finish(); runtime.update(520); near(runtime.value(),36); assert(!runtime.morphing()&&!runtime.destination()&&runtime.morphMode());
        // Mode belongs to the request; a normal request queued during a morph keeps ordinary policies.
        runtime.request(a,1000,0,true); runtime.update(530); runtime.request(b,1000,0,false);
        runtime.update(1530); near(runtime.value(),4); runtime.update(1540); assert(!runtime.morphMode());
        runtime.update(2040); near(runtime.value(),4); runtime.finish(); runtime.update(2050); near(runtime.value(),36);
        // Missing destination preserves the current graph and outputs.
        const auto* playing=runtime.origin(); fs::rename(a,a+"-offline"); runtime.request(a,100,0,true); runtime.update(2060);
        assert(runtime.origin()==playing&&!runtime.destination()); near(runtime.value(),36); fs::rename(a+"-offline",a);
        runtime.clear(); assert(!runtime.morphMode());
        runtime.request(a,0,0,true); const auto pending=runtime.saveState(); assert(pending.at("morph")==true);
        assert(copy.restoreState(pending)); copy.update(0); assert(copy.morphMode());
    }
    // Native nested Macro paths: same model/ID at the root and nested levels remain independent.
    const auto na=scratch+"/nested-a",nb=scratch+"/nested-b"; save(na,false,true); save(nb,true,true);
    {
        MacroMixerRuntime runtime(parent); assert(runtime.importMacro(na)&&runtime.importMacro(nb));
        runtime.request(na,0,0,true); runtime.update(0); runtime.request(nb,1000,0,true); runtime.update(10); runtime.update(510);
        near(probe(runtime.destination()).x,4);
        auto& nested=static_cast<ofxOceanodeNodeMacro&>(runtime.destination()->container->getDynamicNodes().at("Macro").at(1)->getNodeModel());
        auto& q=static_cast<MorphProbe&>(nested.getContainer()->getDynamicNodes().at("Morph Probe").at(1)->getNodeModel());
        near(q.x,6); near(q.driven,50); // Unconnected nested router control morphs; driven input follows its cable.
        assert(runtime.destination()->container->createConnectionFromInfo("Router_f_2","Value","Macro_1","NestedInput"));
        runtime.update(610); near(q.driven,29.6f); // A nested Macro input must obey its parent cable.
        runtime.finish(); runtime.update(620); near(q.x,10); near(q.driven,42);
    }
    // Selected outer Input router controls remain live during morphs, including draw.
    {
        MacroMixerRuntime runtime(parent); auto c=contract; c.routers.push_back({"input","Input","Float","",ofJson::object(),"Input"});
        assert(runtime.setContract(c)&&runtime.importMacro(a)&&runtime.importMacro(b)); assert(runtime.setInputs({{"input",13.f}}));
        runtime.request(a,0,0,true); runtime.update(0); runtime.request(b,1000,0,true); runtime.update(10); runtime.update(510);
        near(probe(runtime.destination()).driven,13); assert(runtime.setInputs({{"input",31.f}})); runtime.update(610); runtime.draw(); near(probe(runtime.destination()).driven,31);
    }
    // Bi Pow uses the existing MacroMixer curve; zero duration applies exact target values.
    {
        MacroMixerRuntime runtime(parent); assert(runtime.importMacro(a)&&runtime.importMacro(b));
        runtime.request(a,0,0,true); runtime.update(0); runtime.request(b,1000,.7f,true); runtime.update(10); runtime.update(260);
        const float eased=MacroMixerRuntime::shape(.25f,.7f), expected=2+4*eased; near(probe(runtime.destination()).x,expected);
        near(runtime.value(),4+eased*(expected*expected-4)); // Linear router crossfade from the running origin (2*2).
        runtime.finish(); runtime.update(270); runtime.request(a,0,0,true); runtime.update(280); near(runtime.value(),4); assert(!runtime.transitioning());
    }
    // A different saved ID on the output path identifies the same model role;
    // preserve that live instance and interpolate its controls.
    {
        const auto different=scratch+"/different"; fs::copy(b,different,fs::copy_options::recursive);
        auto cables=ofLoadJson(different+"/connections.json"); cables.erase("Morph_Probe_1");
        cables["Morph_Probe_3"]["Output"]["Router_f_1"]["Value"]=true; ofSaveJson(different+"/connections.json",cables);
        MacroMixerRuntime runtime(parent); assert(runtime.importMacro(a)&&runtime.importMacro(different));
        runtime.request(a,0,0,true); runtime.update(0); runtime.request(different,1000,0,true); runtime.update(10); near(runtime.value(),4); assert(runtime.morphingInPlace());
        runtime.update(510); near(runtime.value(),30.25); near(probe(runtime.destination()).x,5.5f);
    }
    // Native listeners may remove/retype ports while a morph is running.
    {
        MacroMixerRuntime runtime(parent); assert(runtime.importMacro(a)&&runtime.importMacro(b));
        runtime.request(a,0,0,true); runtime.update(0); runtime.request(b,1000,0,true); runtime.update(10); runtime.update(510);
        auto& p=probe(runtime.destination()); p.removeParameter("Gate"); p.removeParameter("Floats");
        ofParameter<int> replacement; p.addParameter(replacement.set("Floats",123));
        runtime.update(610); near(replacement,123); near(std::get<float>(runtime.destination()->read("value")),4.4f*4.4f);
        near(runtime.value(),4+.6f*(4.4f*4.4f-4)); assert(runtime.diagnostics().empty());
        runtime.finish(); runtime.update(620); near(runtime.value(),36); near(replacement,123);
    }
    // Same-graph morphs keep native Buffer histories, HeaderPro readers,
    // phasors, Portal values and running instances. Target values are read from
    // files without ever constructing a second graph.
    const auto liveA=scratch+"/live-a",liveB=scratch+"/live-b";
    auto saveLive=[&](const std::string& folder,bool target,bool modulated=false,int textureClock=0,bool freedom=false) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("LiveSource");
        auto& out=floating(c); out.getParameter<std::string>("Name")="Value";
        auto& p=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),1).getNodeModel());
        p.x=target?6:2; p.gate=target; p.label=target?"target":"source"; p.count=target?10:0;
        p.floats=target?std::vector<float>{4,6}:std::vector<float>{0,2};
        p.ints=target?std::vector<int>{4,6}:std::vector<int>{0,2};
        p.color=target?ofColor(100,120,140,160):ofColor(0,20,40,60);
        p.tint=target?ofFloatColor(1,.8,.6,.4):ofFloatColor(0,.2,.4,.6);
        assert(c.createConnectionFromInfo("Morph_Probe_1","Output","Router_f_1","Value"));
        c.createNode(std::make_unique<phasor>(),1); c.createNode(std::make_unique<SharedPortalProbe>(),1);
        auto& nested=static_cast<ofxOceanodeNodeMacro&>(c.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1).getNodeModel());
        auto child=nested.getContainer(); auto& level=floating(*child); level.getParameter<std::string>("Name")="Level"; level.getParameter<float>("Value")=target?6:2;
        auto& buffer=child->createNode(std::make_unique<bufferNode<float>>("f",0.f),1).getNodeModel(); buffer.getParameter<int>("Max Size")=64;
        auto& header=child->createNode(std::make_unique<bufferHeaderPro<float>>("f",0.f),1).getNodeModel(); header.getParameter<int>("Copies")=3; header.getParameter<std::vector<float>>("Offset")={1};
        assert(child->createConnectionFromInfo("Router_f_1","Value","Buffer_f_1","Input"));
        assert(child->createConnectionFromInfo("Buffer_f_1","Output","HeaderPro_f_1","Buffer Input"));
        if(modulated) {
            c.createNode(std::make_unique<phasor>(),2);
            c.createNode(std::make_unique<oscillator>(),1);
            auto& range=c.createNode(std::make_unique<ranger>(),1).getNodeModel();
            range.getParameter<std::vector<float>>("Min Out")={2}; range.getParameter<std::vector<float>>("Max Out")={2};
            assert(c.createConnectionFromInfo("Phasor_2","Phase","Oscillator_1","Phase"));
            assert(c.createConnectionFromInfo("Oscillator_1","Output","Ranger_1","Input"));
            assert(c.createConnectionFromInfo("Ranger_1","Output","Macro_1","Level"));
        }
        if(textureClock) {
            c.createNode(std::make_unique<MorphPhaseSink>(),1);
            c.createNode(std::make_unique<phasor>(),textureClock);
            assert(c.createConnectionFromInfo("Phasor_"+std::to_string(textureClock),"Phase","Morph_Phase_Sink_1","Phase"));
        }
        if(freedom) {
            c.createNode(std::make_unique<phasor>(),3);
            c.createNode(std::make_unique<oscillator>(),2);
            c.createNode(std::make_unique<ranger>(),2);
            assert(c.createConnectionFromInfo("Phasor_3","Phase","Oscillator_2","Phase"));
            assert(c.createConnectionFromInfo("Oscillator_2","Output","Ranger_2","Input"));
            assert(c.createConnectionFromInfo("Ranger_2","Output","Morph_Probe_1","Driven"));
        }
        c.savePreset(folder);
        // The outer saved Macro pin must override its child definition, exactly
        // as native preset recall does. Neither preset should use this 999.
        auto json=ofLoadJson(folder+"/Macro_1/Router_f_1.json"); json["Value"]=999.f; ofSaveJson(folder+"/Macro_1/Router_f_1.json",json);
        // Old presets can still contain this now-transient Inspector readout.
        json=ofLoadJson(folder+"/Macro_1/Buffer_f_1.json"); json["Last_Frame_Ms"]=target?"1234":"9876"; ofSaveJson(folder+"/Macro_1/Buffer_f_1.json",json);
    };
    saveLive(liveA,false); saveLive(liveB,true);
    {
        MacroMixerRuntime r(parent); assert(r.importMacro(liveA)&&r.importMacro(liveB)); r.request(liveA,0,0,true); r.update(0);
        auto* original=r.origin(); auto& p=probe(original); auto phase=static_cast<phasor&>(original->container->getDynamicNodes().at("Phasor").at(1)->getNodeModel()).getBasePhasor();
        phase->setPhasor({.37f});
        auto& shared=original->container->getDynamicNodes().at("Portal f").at(1)->getNodeModel().getParameter<float>("Value"); shared=.77f;
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto& bufferModel=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel(); auto* history=bufferModel.getParameter<buffer<float>*>("Output").get();
        history->clear(); history->addFrame(10,Timestamp(1000)); history->addFrame(20,Timestamp(2000)); history->addFrame(30,Timestamp(3000));
        auto& header=child->getDynamicNodes().at("HeaderPro f").at(1)->getNodeModel();
        assert(static_cast<ofxOceanodeAbstractParameter&>(header.getParameterGroup().get("Output")).getFlags()&ofxOceanodeParameterFlags_DisableInConnection);
        assert(!original->captureMorphValues().count({"Macro","1","HeaderPro f","1","Output"}));
        auto& level=child->getDynamicNodes().at("Router f").at(1)->getNodeModel().getParameter<float>("Value"); near(level,2);
        const int setups=morphSetups, updates=p.updates, draws=p.draws;
        const auto aID=r.currentID(),bID=r.importedMacro(liveB)->id();
        r.request(liveB,1000,0,true); r.update(10);
        assert(r.morphingInPlace()&&r.origin()==original&&r.destination()==original&&morphSetups==setups);
        assert(r.currentID()==aID&&r.destinationID()==bID); near(r.value(),4); assert(history->getSize()==3); near(phase->getPhasor(),.37f);
        r.update(510); near(r.value(),16); near(level,4); near(shared,.77f);
        assert(p.gate&&p.label.get()=="target"&&p.count==5);
        assert(p.floats.get()==std::vector<float>({2,4})&&p.ints.get()==std::vector<int>({2,4})); assert(p.color.get()==ofColor(50,70,90,110)); near(p.tint.get().r,.5);
        assert(history->getSize()==4); near(history->getFrame(0).getObjectRef(),10); assert(history->getFrame(0).getTimestamp()==Timestamp(1000));
        assert(header.getParameter<std::vector<float>>("Output").get()==std::vector<float>({4,30,20}));
        assert(p.updates==updates+2); r.draw(); assert(p.draws==draws+1); near(phase->getPhasor(),.37f);
        assert(r.saveState().at("playback")==bID); MacroMixerRuntime copy(parent); assert(copy.restoreState(r.saveState())); copy.update(0); near(copy.value(),36);
        r.finish(); r.update(520); assert(r.origin()==original&&!r.destination()&&r.currentID()==bID&&!r.morphingInPlace()); near(level,6); near(r.value(),36); assert(history->getSize()==5);
        r.request(liveA,1000,0,true); r.update(530); assert(r.morphingInPlace()&&r.origin()==original&&morphSetups==setups+1); // Only the explicit copy constructed a probe.
        r.finish(); r.update(540); near(level,2); near(r.value(),4); assert(history->getSize()==6);
        // Inspector/unknown native configuration changes require normal native loading.
        const auto changed=scratch+"/changed-config"; fs::copy(liveB,changed,fs::copy_options::recursive);
        auto json=ofLoadJson(changed+"/Macro_1/HeaderPro_f_1.json"); json["Calculate_On_Update"]="0"; ofSaveJson(changed+"/Macro_1/HeaderPro_f_1.json",json);
        assert(r.importMacro(changed)); r.request(changed,1000,0,true); r.update(550); assert(r.morphing()&&!r.morphingInPlace()&&r.destination()!=original);
    }
    // Added/removed live modulation: Phasor -> Oscillator -> Ranger -> Macro
    // input. Both directions retain the common nested Buffer and clock. The
    // blend follows changes emitted after morph preparation, not a frozen value.
    const auto modulated=scratch+"/live-modulated"; saveLive(modulated,false,true);
    {
        MacroMixerRuntime r(parent); assert(r.importMacro(modulated)&&r.importMacro(liveB));
        r.request(modulated,0,0,true); r.update(0);
        const auto* original=r.origin();
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto* history=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get();
        history->clear(); history->addFrame(123,Timestamp(1234));
        auto& level=child->getDynamicNodes().at("Router f").at(1)->getNodeModel().getParameter<float>("Value");
        auto signal=[&](float value) {
            auto& range=original->container->getDynamicNodes().at("Ranger").at(1)->getNodeModel();
            range.getParameter<std::vector<float>>("Min Out")={value}; range.getParameter<std::vector<float>>("Max Out")={value};
        };
        signal(2); near(level,2);
        r.request(liveB,1000,.4f,true); r.update(10);
        assert(r.morphingInPlace()&&r.origin()==original&&r.destination()==original); near(level,2);
        assert(original->container->getDynamicNodes().at("Phasor").count(2));
        r.update(260); signal(10);
        const float t=MacroMixerRuntime::shape(.25f,.4f); near(level,(1-t)*10+t*6);
        r.update(510); signal(8); near(level,7); // The signal still publishes between graph updates.
        assert(history->getFrame(0).getObjectRef()==123&&history->getFrame(0).getTimestamp()==Timestamp(1234));
        r.finish(); r.update(520); near(level,6);
        assert(r.origin()==original&&!original->container->getDynamicNodes().at("Phasor").count(2));
        assert(!original->container->getDynamicNodes().at("Ranger").count(1));
        auto* retained=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get(); assert(retained==history);
        // Destination-only modulation is created at the start and introduced
        // smoothly; common buffers are neither cleared nor reconstructed.
        r.request(modulated,1000,0,true); r.update(530); assert(r.morphingInPlace()); near(level,6);
        signal(10); near(level,6); r.update(1030); near(level,8);
        auto state=r.saveState(); MacroMixerRuntime copy(parent); assert(copy.restoreState(state)); copy.update(0);
        assert(copy.origin()->container->getDynamicNodes().at("Ranger").count(1));
        r.finish(); r.update(1040); near(level,10); assert(r.origin()==original);
        auto& pin=static_cast<ofxOceanodeAbstractParameter&>(original->macro->getContainer()->getDynamicNodes().at("Macro").at(1)->getParameters().get("Level"));
        assert(pin.hasInConnection()&&pin.getInConnection()->getSourceParameter().getNodeModel()->nodeName()=="Ranger");
        signal(4); near(level,4); // Final native cable, with no temporary proxy.
        // Another transition proves the committed wiring matches its revision.
        r.request(liveB,1000,0,true); r.update(1050); assert(r.morphingInPlace()); r.clear();
    }
    // Roll back a prepared topology without destroying the common graph.
    {
        std::vector<MacroMixerDiagnostic> errors;
        auto a=MacroMixerSource::importFolder(parent,liveB,{},errors);
        auto b=MacroMixerSource::importFolder(parent,modulated,{},errors);
        auto graph=MacroMixerSource::instantiate(parent,a,errors); assert(graph&&b);
        assert(graph->prepareMorphInPlace(b)); graph->applyMorph(.5f); graph->cancelMorph();
        assert(!graph->container->getDynamicNodes().at("Ranger").count(1));
        assert(graph->prepareMorphInPlace(b)); graph->applyMorph(1); graph->revision=b;
        assert(graph->prepareMorphInPlace(a)); graph->applyMorph(.5f); graph->cancelMorph();
        assert(graph->container->getDynamicNodes().at("Ranger").count(1));
        auto& pin=static_cast<ofxOceanodeAbstractParameter&>(graph->container->getDynamicNodes().at("Macro").at(1)->getParameters().get("Level"));
        assert(pin.hasInConnection()&&pin.getInConnection()->getSourceParameter().getNodeModel()->nodeName()=="Ranger");
        assert(graph->prepareMorphInPlace(a)); graph->applyMorph(1);
    }
    // Native listeners may remove a bridged sink. No hidden inactive cable
    // may retain that destroyed wrapper, including during cancellation/teardown.
    {
        const auto edited=scratch+"/live-editable-sink"; fs::copy(modulated,edited,fs::copy_options::recursive);
        auto cables=ofLoadJson(edited+"/connections.json"); cables.erase("Ranger_1");
        cables["Ranger_1"]["Output"]["Morph_Probe_1"]["Driven"]=nullptr;
        ofSaveJson(edited+"/connections.json",cables);
        std::vector<MacroMixerDiagnostic> errors;
        auto a=MacroMixerSource::importFolder(parent,edited,{},errors);
        auto b=MacroMixerSource::importFolder(parent,liveB,{},errors);
        auto graph=MacroMixerSource::instantiate(parent,a,errors); assert(graph&&b);
        assert(graph->prepareMorphInPlace(b)); graph->applyMorph(.5f);
        probe(graph.get()).removeParameter("Driven"); graph->cancelMorph(); assert(graph->update());
    }
    // Replacing a source cable blends two live signals at the common sink.
    {
        const auto changed=scratch+"/live-rewired"; fs::copy(modulated,changed,fs::copy_options::recursive);
        auto cables=ofLoadJson(changed+"/connections.json"); cables.erase("Ranger_1");
        cables["Morph_Probe_1"]["Output"]["Macro_1"]["Level"]=nullptr;
        ofSaveJson(changed+"/connections.json",cables);
        MacroMixerRuntime r(parent); assert(r.importMacro(modulated)&&r.importMacro(changed));
        r.request(modulated,0,0,true); r.update(0); auto* original=r.origin();
        r.request(changed,1000,0,true); r.update(10); assert(r.morphingInPlace()); r.update(510);
        auto& p=probe(original); p.x=3; r.update(511);
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        const auto& level=child->getDynamicNodes().at("Router f").at(1)->getNodeModel().getParameter<float>("Value");
        near(level,2*.499f+9*.501f);
        auto& range=original->container->getDynamicNodes().at("Ranger").at(1)->getNodeModel();
        range.getParameter<std::vector<float>>("Min Out")={4}; range.getParameter<std::vector<float>>("Max Out")={4};
        near(level,4*.499f+9*.501f);
        r.finish(); r.update(520); near(level,9); // X matched initially, so a live edit to this unplanned control remains live.
        assert(r.origin()==original);
    }
    // Unsupported changed trigger wiring rolls back staged additions before
    // fresh native loading; the original graph stays valid for failure recovery.
    {
        const auto changed=scratch+"/live-trigger"; fs::copy(liveB,changed,fs::copy_options::recursive);
        auto modules=ofLoadJson(changed+"/modules.json"); modules["Morph Probe"]["08"]={0,0}; ofSaveJson(changed+"/modules.json",modules);
        fs::copy_file(changed+"/Morph_Probe_1.json",changed+"/Morph_Probe_8.json");
        auto cables=ofLoadJson(changed+"/connections.json"); cables["Morph_Probe_8"]["Trigger"]["Phasor_1"]["Reset"]=nullptr;
        ofSaveJson(changed+"/connections.json",cables);
        MacroMixerRuntime r(parent); assert(r.importMacro(modulated)&&r.importMacro(changed)); r.request(modulated,0,0,true); r.update(0);
        const auto* original=r.origin(); r.request(changed,1000,0,true); r.update(10);
        assert(r.morphing()&&!r.morphingInPlace()&&r.destination()!=original);
        assert(!original->container->getDynamicNodes().at("Morph Probe").count(8));
        auto& pin=static_cast<ofxOceanodeAbstractParameter&>(original->container->getDynamicNodes().at("Macro").at(1)->getParameters().get("Level"));
        assert(pin.hasInConnection()&&pin.getInConnection()->getSourceParameter().getNodeModel()->nodeName()=="Ranger");
        assert(r.destination()->container->getDynamicNodes().at("Morph Probe").count(8));
    }
    // Graph role wins over IDs for every model type, including native Macro,
    // Router, Buffer and HeaderPro instances at renamed nested paths.
    std::function<void(const std::string&,const std::string&,int)> renumber;
    renumber=[&](const std::string& source,const std::string& destination,int offset) {
        fs::create_directories(destination);
        auto modules=ofLoadJson(source+"/modules.json"),renamed=ofJson::object();
        std::map<std::string,std::string> keys;
        for(auto model=modules.begin();model!=modules.end();++model) for(auto node=model.value().begin();node!=model.value().end();++node) {
            auto type=model.key(); ofStringReplace(type," ","_");
            const int oldID=std::stoi(node.key()),newID=oldID+offset;
            const auto oldKey=type+"_"+std::to_string(oldID),newKey=type+"_"+std::to_string(newID);
            keys[oldKey]=newKey; renamed[model.key()][std::to_string(newID)]=node.value();
            auto settings=ofLoadJson(source+"/"+oldKey+".json"); ofSaveJson(destination+"/"+newKey+".json",settings);
            if(model.key()=="Macro" && settings.value("LocalPreset",true)) renumber(source+"/"+oldKey,destination+"/"+newKey,offset+10);
        }
        auto cables=ofLoadJson(source+"/connections.json"),newCables=ofJson::object();
        for(auto a=cables.begin();a!=cables.end();++a) for(auto b=a.value().begin();b!=a.value().end();++b)
            for(auto c=b.value().begin();c!=b.value().end();++c) for(auto d=c.value().begin();d!=c.value().end();++d)
                newCables[keys.at(a.key())][b.key()][keys.at(c.key())][d.key()]=d.value();
        ofSaveJson(destination+"/modules.json",renamed); ofSaveJson(destination+"/connections.json",newCables);
    };
    const auto renamed=scratch+"/renamed"; renumber(liveB,renamed,10);
    {
        MacroMixerRuntime r(parent); assert(r.importMacro(liveA)&&r.importMacro(renamed)); r.request(liveA,0,0,true); r.update(0);
        const auto* original=r.origin(); auto& p=probe(original); const int setups=morphSetups;
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto* history=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get();
        history->clear(); history->addFrame(17,Timestamp(1234));
        r.request(renamed,1000,0,true); r.update(10); assert(r.morphingInPlace()&&r.destination()==original&&morphSetups==setups);
        r.update(510); near(p.x,4); r.finish(); r.update(520); near(p.x,6);
        assert(history==child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get());
        assert(history->getFrame(0).getObjectRef()==17);
        MacroMixerRuntime copy(parent); assert(copy.restoreState(r.saveState())); copy.update(0);
        assert(copy.origin()->container->getDynamicNodes().at("Macro").count(11));
        auto copiedChild=static_cast<ofxOceanodeNodeMacro&>(copy.origin()->container->getDynamicNodes().at("Macro").at(11)->getNodeModel()).getContainer();
        assert(copiedChild->getDynamicNodes().at("Buffer f").count(21));
        r.request(liveA,1000,0,true); r.update(530); assert(r.morphingInPlace()&&r.destination()==original); r.finish(); r.update(540); near(p.x,2);
        r.request(renamed,1000,0,true); r.update(550); assert(r.morphingInPlace()); r.finish(); r.update(560);
        // Configuration changes still need native loading. Role-based capture
        // also maps current values into the renamed destination's native IDs.
        const auto incompatible=scratch+"/renamed-config"; fs::copy(renamed,incompatible,fs::copy_options::recursive);
        auto settings=ofLoadJson(incompatible+"/Morph_Probe_11.json"); settings["Configuration"]="1"; ofSaveJson(incompatible+"/Morph_Probe_11.json",settings);
        assert(r.importMacro(incompatible)); r.request(liveA,0,0,true); r.update(570); near(p.x,2);
        r.request(incompatible,1000,0,true); r.update(580); assert(!r.morphingInPlace()&&r.morphing());
        near(probe(r.destination(),11).x,2); assert(r.status().find("Configuration")!=std::string::npos);
        r.update(1080); near(probe(r.destination(),11).x,4);
    }
    // A graph can reuse existing IDs for different roles. Match the two probe
    // instances by their named downstream routers, preserving a bijection.
    {
        auto savePair=[&](const std::string& folder,bool target) {
            ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("RolePair");
            auto& a=floating(c,1); a.getParameter<std::string>("Name")="Value";
            auto& b=floating(c,2); b.getParameter<std::string>("Name")="Other";
            auto& one=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),1).getNodeModel()); one.x=target?8:2;
            auto& two=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),2).getNodeModel()); two.x=target?6:4;
            assert(c.createConnectionFromInfo(target?"Morph_Probe_2":"Morph_Probe_1","Output","Router_f_1","Value"));
            assert(c.createConnectionFromInfo(target?"Morph_Probe_1":"Morph_Probe_2","Output","Router_f_2","Value")); c.savePreset(folder);
        };
        const auto a=scratch+"/pair-a",b=scratch+"/pair-b"; savePair(a,false); savePair(b,true);
        MacroMixerRuntime r(parent); assert(r.importMacro(a)&&r.importMacro(b)); r.request(a,0,0,true); r.update(0); const auto* original=r.origin();
        r.request(b,1000,0,true); r.update(10); assert(r.morphingInPlace()); r.update(510);
        near(probe(original,1).x,4); near(probe(original,2).x,6); r.finish(); r.update(520);
        near(probe(original,1).x,6); near(probe(original,2).x,8);
        r.request(a,1000,0,true); r.update(530); assert(r.morphingInPlace()); r.finish(); r.update(540);
        near(probe(original,1).x,2); near(probe(original,2).x,4);
    }
    // The user's 2/3/4 layout: the texture clock is 3, 2 or 4, while the same
    // IDs can instead drive newly added modulation chains. Its live connection
    // and phase must survive every direction; an unsaved Phase is never blended.
    const std::array<std::string,3> roleFolders{scratch+"/role-2",scratch+"/role-3",scratch+"/role-4"};
    saveLive(roleFolders[0],false,true,3,false);
    saveLive(roleFolders[1],true,false,2,false);
    saveLive(roleFolders[2],true,true,4,true);
    for(int initial=0;initial<3;++initial) {
        MacroMixerRuntime r(parent); for(const auto& folder:roleFolders) assert(r.importMacro(folder));
        r.request(roleFolders[initial],0,0,true); r.update(0); const auto* original=r.origin();
        auto& sink=original->container->getDynamicNodes().at("Morph Phase Sink").at(1)->getNodeModel();
        auto& phasePort=static_cast<ofxOceanodeAbstractParameter&>(sink.getParameterGroup().get("Phase"));
        auto* clock=phasePort.getInConnection()->getSourceParameter().getNodeModel();
        auto* cable=phasePort.getInConnection(); const int clockID=clock->getNumIdentifier();
        auto base=static_cast<phasor*>(clock)->getBasePhasor(); base->setPhasor({.63f}); r.update(1);
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto* history=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get();
        history->clear(); history->addFrame(111,Timestamp(4321));
        for(int step=0;step<6;++step) {
            const int target=(initial+1+step)%3; const int now=10+step*2000;
            r.request(roleFolders[target],1000,0,true); r.update(now);
            assert(r.morphingInPlace()&&r.destination()==original&&phasePort.getInConnection()==cable);
            near(sink.getParameter<float>("Phase"),.63f); r.update(now+500);
            assert(clock==phasePort.getInConnection()->getSourceParameter().getNodeModel());
            assert(clock->getNumIdentifier()==clockID);
            base->setPhasor({.99f}); r.update(now+600); near(sink.getParameter<float>("Phase"),.99f);
            base->setPhasor({.01f}); r.update(now+700); near(sink.getParameter<float>("Phase"),.01f);
            r.finish(); r.update(now+800); assert(r.origin()==original&&phasePort.getInConnection()==cable);
            assert(history->getFrame(0).getObjectRef()==111&&history->getFrame(0).getTimestamp()==Timestamp(4321));
            base->setPhasor({.63f}); r.update(now+801);
        }
    }
    // An unsaved numeric input can still morph when both endpoints are live
    // cables. The two sources have different models, so role matching cannot
    // turn this into unchanged wiring.
    {
        const auto from=scratch+"/unsaved-cabled-a",to=scratch+"/unsaved-cabled-b";
        for(bool target:{false,true}) {
            ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("UnsavedCabled");
            auto& output=floating(c); output.getParameter<std::string>("Name")="Value";
            auto& p=static_cast<MorphProbe&>(c.createNode(std::make_unique<MorphProbe>(),1).getNodeModel()); p.x=.5f;
            c.createNode(std::make_unique<phasor>(),1); c.createNode(std::make_unique<MorphPhaseSink>(),1);
            assert(c.createConnectionFromInfo("Morph_Probe_1","Output","Router_f_1","Value"));
            assert(c.createConnectionFromInfo(target?"Morph_Probe_1":"Phasor_1",target?"Output":"Phase","Morph_Phase_Sink_1","Phase"));
            assert(c.savePreset(target?to:from));
        }
        MacroMixerRuntime r(parent); assert(r.importMacro(from)&&r.importMacro(to)); r.request(from,0,0,true); r.update(0);
        const auto* graph=r.origin(); auto& clock=static_cast<phasor&>(graph->container->getDynamicNodes().at("Phasor").at(1)->getNodeModel());
        auto& sink=graph->container->getDynamicNodes().at("Morph Phase Sink").at(1)->getNodeModel().getParameter<float>("Phase");
        clock.getBasePhasor()->setPhasor({.75f}); r.update(1); near(sink,.75f);
        r.request(to,1000,0,true); r.update(10); assert(r.morphingInPlace()); r.update(510); near(sink,.5f);
        r.finish(); r.update(520); near(sink,.25f); assert(r.origin()==graph);
        r.request(from,1000,0,true); r.update(530); assert(r.morphingInPlace()); r.update(1030); near(sink,.5f);
        r.finish(); r.update(1040); near(sink,.75f);
        std::vector<MacroMixerDiagnostic> errors;
        auto target=MacroMixerSource::importFolder(parent,to,{},errors); assert(target);
        auto* live=const_cast<MacroMixerSource::Graph*>(graph); assert(live->prepareMorphInPlace(target)); live->applyMorph(.5f); near(sink,.5f);
        live->cancelMorph(); near(sink,.75f);
    }
    // New/removed whole nested Macros are native graph additions. Their live
    // numeric outputs blend at common sinks; incoming controls remain live.
    auto saveNestedAddition=[&](const std::string& folder,bool target,int kind) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("NestedAddition");
        assert(c.loadPreset(target?liveB:liveA));
        assert(c.createConnectionFromInfo("Macro_1","Level","Morph_Probe_1","Driven"));
        if(kind) {
            auto& extra=static_cast<ofxOceanodeNodeMacro&>(c.createNode(std::make_unique<ofxOceanodeNodeMacro>(),2).getNodeModel());
            auto child=extra.getContainer();
            if(kind!=4) {
                auto& gain=floating(*child,1); gain.getParameter<std::string>("Name")="Gain"; gain.getParameter<float>("Value")=2;
                auto& signal=floating(*child,2); signal.getParameter<std::string>("Name")="Signal";
                auto& clock=static_cast<phasor&>(child->createNode(std::make_unique<phasor>(),1).getNodeModel());
                child->createNode(std::make_unique<ranger>(),1);
                assert(child->createConnectionFromInfo("Phasor_1","Phase","Ranger_1","Input"));
                assert(child->createConnectionFromInfo("Router_f_1","Value","Ranger_1","Max Out"));
                assert(child->createConnectionFromInfo("Ranger_1","Output","Router_f_2","Value"));
                assert(c.createConnectionFromInfo("Macro_1","Level","Macro_2","Gain"));
                if(kind!=3) assert(c.createConnectionFromInfo("Macro_2","Signal","Morph_Probe_1","X"));
                clock.getBasePhasor()->setPhasor({.5f}); c.update();
            }
        }
        assert(c.savePreset(folder));
        if(kind==2) {
            const auto library=scratch+"/Macros/nested/SignalMod"; fs::create_directories(scratch+"/Macros/nested");
            fs::copy(folder+"/Macro_2",library,fs::copy_options::recursive);
            auto json=ofLoadJson(folder+"/Macro_2.json"); json["LocalPreset"]=false; json["CategoryStruct"]={"nested"}; json["Macro"]="SignalMod"; json["MacroPath"]=library;
            ofSaveJson(folder+"/Macro_2.json",json); ofxOceanodeShared::readMacros();
        }
    };
    const auto plainNested=scratch+"/nested-plain"; saveNestedAddition(plainNested,false,0);
    const std::array<std::string,4> nestedTargets{scratch+"/nested-local",scratch+"/nested-global",scratch+"/nested-unused",scratch+"/nested-empty"};
    for(int kind=1;kind<=4;++kind) saveNestedAddition(nestedTargets[kind-1],true,kind);
    for(int kind=1;kind<=4;++kind) {
        MacroMixerRuntime r(parent); assert(r.importMacro(plainNested)&&r.importMacro(nestedTargets[kind-1]));
        r.request(plainNested,0,0,true); r.update(0); const auto* original=r.origin();
        auto child=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto& level=child->getDynamicNodes().at("Router f").at(1)->getNodeModel().getParameter<float>("Value");
        auto* history=child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get();
        history->clear(); history->addFrame(321,Timestamp(9876));
        r.request(nestedTargets[kind-1],1000,0,true); r.update(10); assert(r.morphingInPlace()&&r.destination()==original);
        auto extraNode=original->container->getDynamicNodes().at("Macro").at(2); std::weak_ptr<ofxOceanodeNode> extraWeak=extraNode;
        auto& extra=static_cast<ofxOceanodeNodeMacro&>(extraNode->getNodeModel());
        assert(extra.getContainer()->getAllModules().size()==(kind==4?0:4));
        if(kind==2) assert(extra.getNodeTitleSuffix()=="SignalMod");
        auto setClock=[&](float value) {
            auto& clock=static_cast<phasor&>(extra.getContainer()->getDynamicNodes().at("Phasor").at(1)->getNodeModel()); clock.getBasePhasor()->setPhasor({value});
        };
        if(kind!=4) setClock(.75f);
        r.update(510); near(level,4); near(probe(original).driven,4);
        if(kind<=2) { near(extra.getParameter<float>("Gain"),4); near(extra.getParameter<float>("Signal"),3); near(probe(original).x,2.5f); r.update(510); near(r.value(),6.25f); }
        else near(probe(original).x,4);
        MacroMixerRuntime copy(parent); assert(copy.restoreState(r.saveState())); copy.update(0);
        assert(copy.origin()->container->getDynamicNodes().at("Macro").count(2));
        r.finish(); r.update(520); assert(r.origin()==original); near(level,6);
        if(kind<=2) { near(probe(original).x,4.5f); setClock(.5f); r.update(521); near(probe(original).x,3); }
        r.request(plainNested,1000,0,true); r.update(530); assert(r.morphingInPlace());
        if(kind!=4) setClock(.75f);
        r.update(1030); near(level,4); if(kind<=2) near(probe(original).x,2.5f);
        assert(history==child->getDynamicNodes().at("Buffer f").at(1)->getNodeModel().getParameter<buffer<float>*>("Output").get());
        assert(history->getFrame(0).getObjectRef()==321&&history->getFrame(0).getTimestamp()==Timestamp(9876));
        extraNode.reset(); assert(!extraWeak.expired()); r.finish(); r.update(1040);
        assert(extraWeak.expired()&&!original->container->getDynamicNodes().at("Macro").count(2)); near(probe(original).x,2);
        // Repeated loading/retirement keeps the common graph and aliases valid.
        r.request(nestedTargets[kind-1],1000,0,true); r.update(1050); assert(r.morphingInPlace()); r.finish(); r.update(1060);
    }
    // Rollback cleans a fully loaded nested graph and all boundary cables.
    {
        std::vector<MacroMixerDiagnostic> errors;
        auto a=MacroMixerSource::importFolder(parent,plainNested,{},errors),b=MacroMixerSource::importFolder(parent,nestedTargets[0],{},errors);
        auto graph=MacroMixerSource::instantiate(parent,a,errors); assert(graph&&b&&graph->prepareMorphInPlace(b));
        graph->applyMorph(.5f); graph->cancelMorph(); assert(!graph->container->getDynamicNodes().at("Macro").count(2));
        assert(!static_cast<ofxOceanodeAbstractParameter&>(probe(graph.get()).getParameterGroup().get("X")).hasInConnection());
        assert(graph->prepareMorphInPlace(b)); graph->applyMorph(1); graph->revision=b;
        auto* retained=graph->container->getDynamicNodes().at("Macro").at(2).get();
        assert(graph->prepareMorphInPlace(a)); graph->applyMorph(.5f); graph->cancelMorph();
        assert(graph->container->getDynamicNodes().at("Macro").at(2).get()==retained);
        assert(static_cast<ofxOceanodeAbstractParameter&>(probe(graph.get()).getParameterGroup().get("X")).hasInConnection());
        // A later unsupported trigger cable rolls back the already loaded Macro.
        const auto failing=scratch+"/nested-unsupported"; fs::copy(nestedTargets[0],failing,fs::copy_options::recursive);
        auto cables=ofLoadJson(failing+"/connections.json"); cables["Morph_Probe_1"]["Trigger"]["Phasor_1"]["Reset"]=nullptr; ofSaveJson(failing+"/connections.json",cables);
        auto target=MacroMixerSource::importFolder(parent,failing,{},errors); assert(target);
        assert(graph->prepareMorphInPlace(a)); graph->applyMorph(1); graph->revision=a;
        assert(!graph->prepareMorphInPlace(target)); assert(!graph->container->getDynamicNodes().at("Macro").count(2));
        assert(graph->update());
    }
    // Local added source folder Macro_1 can be staged under runtime ID 2 when
    // a different common Macro already occupies ID 1. No copying or renaming of
    // the source files is required, including its nested definition paths.
    {
        const auto alias=scratch+"/nested-alias"; fs::create_directories(alias);
        for(auto f:fs::directory_iterator(nestedTargets[0])) {
            if(f.is_regular_file() && f.path().filename()!="Macro_1.json" && f.path().filename()!="Macro_2.json")
                fs::copy_file(f.path(),fs::path(alias)/f.path().filename());
        }
        fs::copy(nestedTargets[0]+"/Macro_1",alias+"/Macro_2",fs::copy_options::recursive);
        fs::copy(nestedTargets[0]+"/Macro_2",alias+"/Macro_1",fs::copy_options::recursive);
        fs::copy_file(nestedTargets[0]+"/Macro_1.json",alias+"/Macro_2.json"); fs::copy_file(nestedTargets[0]+"/Macro_2.json",alias+"/Macro_1.json");
        auto cables=ofLoadJson(nestedTargets[0]+"/connections.json"),changed=ofJson::object();
        auto remap=[](const std::string& name) { return name=="Macro_1" ? std::string("Macro_2") : name=="Macro_2" ? std::string("Macro_1") : name; };
        for(auto a=cables.begin();a!=cables.end();++a) for(auto b=a.value().begin();b!=a.value().end();++b)
            for(auto c=b.value().begin();c!=b.value().end();++c) for(auto d=c.value().begin();d!=c.value().end();++d)
                changed[remap(a.key())][b.key()][remap(c.key())][d.key()]=d.value();
        ofSaveJson(alias+"/connections.json",changed);
        MacroMixerRuntime r(parent); assert(r.importMacro(plainNested)&&r.importMacro(alias)); r.request(plainNested,0,0,true); r.update(0);
        const auto* original=r.origin(); r.request(alias,1000,0,true); r.update(10); assert(r.morphingInPlace());
        auto& added=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(2)->getNodeModel());
        assert(added.getParameterGroup().contains("Signal")&&added.getContainer()->getDynamicNodes().at("Phasor").count(1));
        r.finish(); r.update(20); r.request(plainNested,1000,0,true); r.update(30); assert(r.morphingInPlace()); r.finish(); r.update(40);
    }
    {
        dotMaker dots; dots.setup();
        assert(static_cast<ofxOceanodeAbstractParameter&>(dots.getParameterGroup().get("X_out")).getFlags()&ofxOceanodeParameterFlags_DisableInConnection);
        assert(static_cast<ofxOceanodeAbstractParameter&>(dots.getParameterGroup().get("Y_out")).getFlags()&ofxOceanodeParameterFlags_DisableInConnection);
    }
    // Source details are read-only compatibility previews. Changed connections
    // share router policies, independent of router-output mixing preferences.
    {
        MacroMixerRuntime r(parent); assert(r.importMacro(plainNested)&&r.importMacro(nestedTargets[0])&&r.importMacro(b));
        r.request(plainNested,0,0,true); r.update(0); const auto* original=r.origin();
        const auto destination=r.importedMacro(nestedTargets[0])->id();
        const int setups=morphSetups; const auto modules=original->container->getAllModules().size();
        auto details=r.sourceDetails(destination);
        assert(morphSetups==setups&&original->container->getAllModules().size()==modules);
        assert(std::none_of(details.begin(),details.end(),[](const auto& d){return d.kind=="fallback";}));
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="mix"&&d.location=="Morph_Probe_1 / X"&&d.type=="Float"&&d.policy=="linear";}));
        assert(r.setMorphPolicy("Float","switch",{{"threshold",.8}}));
        details=r.sourceDetails(destination);
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="mix"&&d.type=="Float"&&d.policy=="switch"&&d.options.at("threshold")==.8;}));
        assert(!r.setMorphPolicy("Float","missing")); assert(r.morphPolicies().at("Float").policy=="switch");
        const auto saved=r.saveState(); MacroMixerRuntime copy(parent); assert(copy.restoreState(saved)); copy.update(0);
        assert(copy.morphPolicies().at("Float").options.at("threshold")==.8);
        auto invalid=saved; invalid["morphPolicies"]["Float"]["policy"]="unavailable";
        const auto* retained=copy.origin(); assert(!copy.restoreState(invalid)&&copy.origin()==retained);
        auto legacy=saved; legacy.erase("morphPolicies"); assert(copy.restoreState(legacy)); assert(copy.morphPolicies().empty());
        // A source's own policy overrides the mixer default only for transitions into it.
        assert(r.setMorphPolicy("Float","linear")&&r.setSourceMorphPolicy(destination,"Float","switch",{{"threshold",.8}}));
        const auto other=r.importedMacro(b)->id();
        assert(r.sourceMorphPolicies(other).empty()&&r.morphPoliciesInto(other).at("Float").policy=="linear");
        assert(r.morphPoliciesInto(destination).at("Float").options.at("threshold")==.8);
        details=r.sourceDetails(destination);
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="mix"&&d.type=="Float"&&d.policy=="switch"&&d.options.at("threshold")==.8;}));
        assert(!r.setSourceMorphPolicy("missing","Float","switch")&&!r.setSourceMorphPolicy(destination,"Float","missing"));
        assert(r.sourceMorphPolicies(destination).at("Float").policy=="switch");
        const auto withSource=r.saveState(); assert(copy.restoreState(withSource));
        assert(copy.sourceMorphPolicies(destination).at("Float").options.at("threshold")==.8&&copy.sourceMorphPolicies(other).empty());
        auto invalidSource=withSource;
        for(auto& entry:invalidSource.at("macros")) if(entry.at("id")==destination) entry["morphPolicies"]["Float"]["policy"]="unavailable";
        assert(!copy.restoreState(invalidSource)&&copy.sourceMorphPolicies(destination).at("Float").policy=="switch");
        assert(copy.restoreState(legacy)&&copy.sourceMorphPolicies(destination).empty());
        assert(copy.restoreState(withSource)&&copy.clearSourceMorphPolicy(destination,"Float")&&copy.sourceMorphPolicies(destination).empty());
        assert(copy.setSourceMorphPolicy(destination,"Float","switch")&&copy.removeMacro(destination)&&copy.sourceMorphPolicies(destination).empty());
        r.request(nestedTargets[0],1000,0,true); r.update(10); assert(r.morphingInPlace());
        assert(!r.setMorphPolicy("Float","linear")&&!r.setSourceMorphPolicy(destination,"Float","linear")&&!r.clearSourceMorphPolicy(destination,"Float"));
        auto& extra=static_cast<ofxOceanodeNodeMacro&>(original->container->getDynamicNodes().at("Macro").at(2)->getNodeModel());
        auto& clock=static_cast<phasor&>(extra.getContainer()->getDynamicNodes().at("Phasor").at(1)->getNodeModel()); clock.getBasePhasor()->setPhasor({.75f});
        r.update(510); near(probe(original).x,2); // Switch leaves the origin fixed value until its configured threshold.
        r.update(910); near(probe(original).x,4.2f); r.finish(); r.update(1010);
        assert(r.hasTransitionDetails(destination));
        assert(std::any_of(r.sourceDetails(destination).begin(),r.sourceDetails(destination).end(),[](const auto& d){return d.kind=="mix"&&d.policy=="switch";}));
        assert(r.setMorphPolicy("Float","linear")); assert(r.hasTransitionDetails(destination));
        assert(std::any_of(r.sourceDetails(destination).begin(),r.sourceDetails(destination).end(),[](const auto& d){return d.kind=="mix"&&d.policy=="switch";}));
        const auto changed=scratch+"/policy-config"; fs::copy(nestedTargets[0],changed,fs::copy_options::recursive);
        auto config=ofLoadJson(changed+"/Morph_Probe_1.json"); config["Configuration"]=1; ofSaveJson(changed+"/Morph_Probe_1.json",config);
        assert(r.importMacro(changed)); const auto failing=r.importedMacro(changed)->id();
        details=r.sourceDetails(failing);
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="fallback"&&d.message.find("Configuration")!=std::string::npos;}));
        r.request(changed,1000,0,true); r.update(1100); assert(r.morphing()&&!r.morphingInPlace());
        details=r.sourceDetails(failing);
        assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="fallback"&&d.message.find("Morph_Probe_1")!=std::string::npos;}));
    }
    // Inspector flag persists through presets/clipboard and old sessions default to off.
    {
        auto& m=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        assert(!m.getInspectorParameter<bool>("Morph macros")); m.getInspectorParameter<bool>("Morph macros")=true;
        MacroMixerContract collision; collision.routers[0].name="Morph macros"; assert(!m.setRouterContract(collision));
        assert(m.setRouterContract({})&&m.importMacroFolder(a)&&m.importMacroFolder(b));
        assert(m.setMorphConnectionPolicy("Float","switch",{{"threshold",.7}}));
        m.getParameter<float>("Duration Ms")=0; m.getParameter<void>("Transition").trigger(); parent.update(); assert(m.mixerRuntime()->morphMode());
        parent.savePreset(scratch+"/session"); parent.clearContainer(); assert(parent.loadPreset(scratch+"/session")); parent.update();
        auto& loaded=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(1)->getNodeModel()); assert(loaded.getInspectorParameter<bool>("Morph macros"));
        assert(loaded.mixerRuntime()->morphPolicies().at("Float").options.at("threshold")==.7);
        parent.saveClipboardModulesAndConnections({parent.getDynamicNodes().at("MacroMixer").at(1).get()},{0,0}); assert(parent.loadClipboardModulesAndConnections({10,10},false)); parent.update();
        auto& copy=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(2)->getNodeModel()); assert(copy.getInspectorParameter<bool>("Morph macros"));
        assert(copy.mixerRuntime()->origin()->container!=loaded.mixerRuntime()->origin()->container);
        ofJson old; loaded.presetSave(old); old["MacroMixerState"].erase("morph"); loaded.loadBeforeConnections(old); assert(!loaded.getInspectorParameter<bool>("Morph macros"));
        parent.clearContainer();
    }
    // Render the production Source section in an ImGui frame. Its visible text
    // includes both actionable fallback details and connection policy controls.
    {
        auto& node=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1);
        auto& m=static_cast<ofxOceanodeMacroMixer&>(node.getNodeModel());
        assert(m.setRouterContract({})&&m.importMacroFolder(plainNested)&&m.importMacroFolder(nestedTargets[0])&&m.importMacroFolder(scratch+"/policy-config"));
        m.getInspectorParameter<bool>("Morph macros")=true; m.getParameter<float>("Duration Ms")=0;
        m.getParameter<void>("Transition").trigger(); parent.update();
        auto* context=ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={1200,2400}; io.DeltaTime=1.f/60;
        unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ImGui::NewFrame(); ImGui::SetNextWindowSize({1100,2300}); ImGui::Begin("MacroMixer Source details");
        for(auto& source:m.mixerRuntime()->macros()) {
            ImGui::PushID(source->id().c_str()); ImGui::GetStateStorage()->SetInt(ImGui::GetID("Source"),1);
            ImGui::PushID("Source"); for(const char* tree:{"Morph details","##policies"}) ImGui::GetStateStorage()->SetInt(ImGui::GetID(tree),1);
            ImGui::PopID(); ImGui::PopID();
        }
        const auto log=scratch+"/source-ui.txt"; ImGui::LogToFile(5,log.c_str());
        m.getInspectorParameter<std::function<void()>>("Macros").get()(); ImGui::LogFinish();
        ImGui::End(); ImGui::Render(); ImGui::DestroyContext(context);
        const auto text=ofBufferFromFile(log).getText();
        assert(text.find("Connection mix")!=std::string::npos&&text.find("Morph policies into this source")!=std::string::npos);
        assert(text.find("Mixer default (Linear)")!=std::string::npos&&text.find("State: Checked")!=std::string::npos);
        assert(text.find("Graph fallback")!=std::string::npos&&text.find("Configuration")!=std::string::npos);
        parent.clearContainer();
    }
    assert(triggers==0 && liveGui==0);
    std::cout<<"MacroMixer graph morphs passed: nonlinear controls, shared buffers/clocks, added/removed modulation, live cable replacement, rollback/port removal, resource fallbacks, queues and persistence.\n";
}
'''

if __name__ == "__main__":
    mixer.main(extra_includes=[mixer.ROOT.parent / "ofxOceanodeVectorGraphics/src"])
