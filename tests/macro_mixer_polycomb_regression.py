#!/usr/bin/env python3
"""MacroMixer morphs scPolyComb's own saved sound on the running node through the
node morph hooks. Production scPolyComb, no SuperCollider server."""
import macro_mixer_regression as mixer
from of_environment import addon_source

SC=mixer.ROOT.parent/'ofxOceanodeSuperCollider'
SUPERCOLLIDER=mixer.ROOT.parent/'ofxSuperCollider'
OSC=mixer.ROOT.parent/'ofxOsc'

BASE=mixer.native_production
def production():
    model=(mixer.ROOT/'src/Nodes/ofxOceanodeNodeModel.cpp').read_text()
    return BASE()+mixer.block(model,'void ofxOceanodeNodeModel::removeSeparator(')+'\n'
mixer.native_production=production
mixer.PRELUDE += r'''
#include "scPolyComb.h"
'''
mixer.CASES=r'''
static void near(float a,float b) { if(!std::isfinite(a)||std::abs(a-b)>=.0001f) { std::cerr<<a<<" != "<<b<<"\n"; std::abort(); } }
// Registered controls listed in blended keys are owned by the node even
// without a Skip hint. Generic interpolation would fire this listener outside
// the node's guarded morph callback and fail the regression.
class OwnedCount : public ofxOceanodeNodeModel {
public:
    OwnedCount():ofxOceanodeNodeModel("Owned Count") {}
    ofParameter<int> voices; ofEventListener listener;
    bool monitor=false,applying=false; int from=4,to=4;
    void setup() override {
        addParameter(voices.set("Voices",4,1,32));
        listener=voices.newListener([this](int&){assert(!monitor||applying);});
    }
    MorphKeys morphKeys()const override{return {{"Voices"},{}};}
    bool morphBegin(const ofJson& j,std::string&)override{from=voices;to=j.value("Voices",from);monitor=true;return true;}
    void morphApply(float t,bool)override{applying=true;voices=t<=0?from:t>=1?to:std::max(from,to);applying=false;}
    void morphEnd(bool completed)override{monitor=false;if(!completed)voices=from;}
};
static void policyEntryPoints() {
    scSnapshots snapshots; snapshots.setupParameters();snapshots.morphCurveP.set(0);
    ofParameter<bool> enabled;enabled.set("Enabled",false);
    snapshots.morph.bypass(enabled);
    auto listener=enabled.newListener([&](bool&){snapshots.morph.edited(enabled);});
    float activation=-1;
    scSnapshots::Host h;h.folder="MacroMixerPolicyPilot";
    h.capture=[&](){return ofJson{{"Enabled",enabled.get()}};};
    h.apply=[&](const ofJson& a,const ofJson& b,float,bool sw){enabled.set((sw?b:a).value("Enabled",false));};
    h.morphChanged=[&](){activation=snapshots.morph.activation(enabled);};
    snapshots.setup(h);
    const ofJson a={{"Enabled",false}},b={{"Enabled",true}};
    snapshots.slots[0]=a;snapshots.slots[1]=b;snapshots.used[0]=snapshots.used[1]=true;
    snapshots.morphAP.set(1);snapshots.morphBP.set(2);snapshots.morphP.set(.25f);
    near(activation,.25f);const float local=activation;
    snapshots.applyExternalMorph(a,b,.25f,false);near(activation,local);
    snapshots.applyExternalMorph(a,b,.75f,true);near(activation,.75f);
    snapshots.glideTo(a,0);assert(snapshots.gliding());
    const auto interrupted=snapshots.captureForExternalMorph();assert(!snapshots.gliding());
    assert(interrupted.contains(scMorphPolicies::metadata));
    snapshots.applyExternalMorph(interrupted,a,.5f,true);near(activation,.375f);
    snapshots.applyExternalMorph(interrupted,interrupted,1,true);near(activation,.75f);
}
static auto& floating(ofxOceanodeContainer& c,int id=1) {
    return c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),id).getNodeModel();
}
static ofJson saved(ofxOceanodeNodeModel& model) { ofJson j; model.presetSave(j); return j; }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<router<float>>("Router","f",0.f,-FLT_MAX,FLT_MAX);
    registry->registerModel<scPolyComb>("SuperCollider",std::vector<serverManager*>{});
    registry->registerModel<OwnedCount>("Test");
    policyEntryPoints();
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("PolyCombMorph");
    // Two saved patches with a polyComb whose hidden sound differs, as in the
    // Bloobs presets: numbers glide, modes switch half way, Snapshot stays.
    const auto a=scratch+"/a",b=scratch+"/b",voices=scratch+"/voices";
    {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("PolyCombSource");
        auto& out=floating(c); out.getParameter<std::string>("Name")="Value";
        c.createNode(registry->create("SC polycombBox"),1);
        assert(c.savePreset(a));
    }
    auto source=ofLoadJson(a+"/SC_polycombBox_1.json"); auto& sound=source["polyComb"];
    assert(sound.contains("Cutoff")&&sound.contains("Echo_Model")&&sound.contains("Snapshot")&&sound.contains("Reverb_Decay"));
    sound["Cutoff"]={80.f}; sound["Echo_Model"]=0; sound["Snapshot"]=1; sound["Reverb_Decay"]=1.f;
    ofSaveJson(a+"/SC_polycombBox_1.json",source);
    fs::copy(a,b,fs::copy_options::recursive); fs::copy(a,voices,fs::copy_options::recursive);
    auto target=source; target["polyComb"]["Cutoff"]={100.f}; target["polyComb"]["Echo_Model"]=1; target["polyComb"]["Snapshot"]=3; target["polyComb"]["Reverb_Decay"]=4.f;
    ofSaveJson(b+"/SC_polycombBox_1.json",target);
    auto moreVoices=source; moreVoices["Voices"]=8; ofSaveJson(voices+"/SC_polycombBox_1.json",moreVoices);

    MacroMixerRuntime r(parent); assert(r.importMacro(a)&&r.importMacro(b)&&r.importMacro(voices));
    r.request(a,0,0,true); r.update(0); const auto* graph=r.origin(); assert(graph);
    auto polyComb=[&]() -> ofxOceanodeNodeModel& { return graph->container->getDynamicNodes().at("SC polycombBox").begin()->second->getNodeModel(); };
    auto value=[&](const char* key) { return saved(polyComb()).at("polyComb").at(key); };
    near(value("Cutoff")[0].get<float>(),80); assert(value("Echo_Model")==0);
    auto details=r.sourceDetails(r.importedMacro(b)->id());
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="node"&&d.location=="SC_polycombBox_1";}));
    assert(std::none_of(details.begin(),details.end(),[](const auto& d){return d.kind=="fallback"||d.kind=="replaced";}));
    r.request(b,1000,0,true); r.update(10); assert(r.morphingInPlace()&&r.origin()==graph);
    r.update(260); near(value("Cutoff")[0].get<float>(),85); assert(value("Echo_Model")==0);
    near(value("Reverb_Decay").get<float>(),std::exp(ofLerp(std::log(1.f),std::log(4.f),.25f))); // Decay glides in log, as in its snapshots.
    r.update(760); near(value("Cutoff")[0].get<float>(),95); assert(value("Echo_Model")==1);
    r.finish(); r.update(770); assert(!r.transitioning()&&r.origin()==graph);
    near(value("Cutoff")[0].get<float>(),100); assert(value("Echo_Model")==1);
    near(value("Reverb_Decay").get<float>(),4);
    assert(value("Snapshot")==1); // The node's own snapshot slot is not the sound.
    // Cancelling returns to the sound the morph started from.
    r.request(a,1000,0,true); r.update(780); r.update(1030); near(value("Cutoff")[0].get<float>(),95);
    const_cast<MacroMixerSource::Graph*>(r.destination())->cancelMorph();
    near(value("Cutoff")[0].get<float>(),100); assert(value("Echo_Model")==1);
    r.clear();
    // A count-only change belongs to the running synth's sound morph. It must
    // appear in Details, keep the same node, and expose the destination early.
    MacroMixerRuntime s(parent); assert(s.importMacro(a)&&s.importMacro(voices));
    s.request(a,0,0,true); s.update(0);
    const auto* countGraph=s.origin();
    auto* countNode=countGraph->container->getDynamicNodes().at("SC polycombBox").at(1).get();
    auto& countModel=countNode->getNodeModel();
    details=s.sourceDetails(s.importedMacro(voices)->id());
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="node"&&d.location=="SC_polycombBox_1";}));
    assert(std::none_of(details.begin(),details.end(),[](const auto& d){return d.kind=="replaced"||d.kind=="fallback";}));
    s.request(voices,1000,0,true); s.update(10);
    assert(s.morphingInPlace()&&s.origin()==countGraph);
    assert(countModel.getParameter<int>("Voices")==4);
    s.update(11.01); assert(countModel.getParameter<int>("Voices")==8);
    s.update(509); assert(countModel.getParameter<int>("Voices")==8);
    s.update(511); assert(countModel.getParameter<int>("Voices")==8);
    assert(countGraph->container->getDynamicNodes().at("SC polycombBox").at(1).get()==countNode);
    const_cast<MacroMixerSource::Graph*>(s.destination())->cancelMorph();
    assert(countModel.getParameter<int>("Voices")==4);
    s.clear();
    // A complete forward/backward transition adopts each endpoint count.
    MacroMixerRuntime v(parent); assert(v.importMacro(a)&&v.importMacro(voices));
    v.request(a,0,0,true); v.update(0); const auto* stable=v.origin();
    auto& stableModel=stable->container->getDynamicNodes().at("SC polycombBox").at(1)->getNodeModel();
    v.request(voices,1000,0,true); v.update(10); v.finish(); v.update(20);
    assert(v.origin()==stable&&!v.transitioning()&&stableModel.getParameter<int>("Voices")==8);
    v.request(a,1000,0,true); v.update(30); v.update(31.01);
    assert(stableModel.getParameter<int>("Voices")==8);
    v.finish(); v.update(40);
    assert(v.origin()==stable&&stableModel.getParameter<int>("Voices")==4);
    v.clear();
    // Output layout still requires the existing structural transition.
    const auto channels=scratch+"/channels";
    fs::copy(a,channels,fs::copy_options::recursive);
    auto channelTarget=source; channelTarget["Channels"]=4;
    ofSaveJson(channels+"/SC_polycombBox_1.json",channelTarget);
    MacroMixerRuntime layout(parent); assert(layout.importMacro(a)&&layout.importMacro(channels));
    layout.request(a,0,0,true); layout.update(0);
    details=layout.sourceDetails(layout.importedMacro(channels)->id());
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="replaced"&&d.message.find("(Channels)")!=std::string::npos;}));
    layout.clear();
    // A cable remains the authoritative writer of Voices during the sound morph.
    const auto drivenA=scratch+"/driven-a",drivenB=scratch+"/driven-b";
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("DrivenVoices");
        auto& driver=floating(c);driver.getParameter<std::string>("Name")="Value";
        driver.getParameter<float>("Value")=7;
        c.createNode(registry->create("SC polycombBox"),1);
        assert(c.createConnectionFromInfo("Router_f_1","Value","SC_polycombBox_1","Voices"));
        assert(c.savePreset(drivenA));
    }
    fs::copy(drivenA,drivenB,fs::copy_options::recursive);
    auto drivenTarget=ofLoadJson(drivenB+"/SC_polycombBox_1.json");drivenTarget["Voices"]=12;
    ofSaveJson(drivenB+"/SC_polycombBox_1.json",drivenTarget);
    MacroMixerRuntime driven(parent);assert(driven.importMacro(drivenA)&&driven.importMacro(drivenB));
    driven.request(drivenA,0,0,true);driven.update(0);
    auto& drivenModel=driven.origin()->container->getDynamicNodes().at("SC polycombBox").at(1)->getNodeModel();
    assert(drivenModel.getParameter<int>("Voices")==7);
    driven.request(drivenB,1000,0,true);driven.update(10);driven.update(511);
    assert(driven.morphingInPlace()&&drivenModel.getParameter<int>("Voices")==7);
    driven.finish();driven.update(520);assert(drivenModel.getParameter<int>("Voices")==7);driven.clear();
    const auto ownedA=scratch+"/owned-a",ownedB=scratch+"/owned-b";
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("OwnedCount");
        auto& out=floating(c);out.getParameter<std::string>("Name")="Value";
        c.createNode(registry->create("Owned Count"),1);assert(c.savePreset(ownedA));
    }
    fs::copy(ownedA,ownedB,fs::copy_options::recursive);
    auto ownedTarget=ofLoadJson(ownedB+"/Owned_Count_1.json");ownedTarget["Voices"]=12;
    ofSaveJson(ownedB+"/Owned_Count_1.json",ownedTarget);
    MacroMixerRuntime owned(parent);assert(owned.importMacro(ownedA)&&owned.importMacro(ownedB));
    owned.request(ownedA,0,0,true);owned.update(0);
    auto& ownedModel=owned.origin()->container->getDynamicNodes().at("Owned Count").at(1)->getNodeModel();
    owned.request(ownedB,1000,0,true);owned.update(10);owned.update(260);
    assert(owned.morphingInPlace()&&ownedModel.getParameter<int>("Voices")==12);
    owned.finish();owned.update(270);assert(ownedModel.getParameter<int>("Voices")==12);owned.clear();
    std::cout<<"polyComb node morph passed: hidden sound/log glides, count-only Details, early Voices, same-node forward/reverse transitions, cancellation and structural channel layouts.\n";
}
'''

if __name__ == "__main__":
    sources=[str(mixer.ROOT/'src/Managers/ofxOceanodeScheduling.cpp')]
    sources+=[str(addon_source(SC,name)) for name in ('scNode.cpp','serverManager.cpp','scTimelineMenu.cpp',
             'nodes/scPolyComb.cpp','nodes/scModulation.cpp')]
    sources+=[str(p) for p in sorted((SUPERCOLLIDER/'src').glob('*.cpp'))]
    sources+=[str(p) for p in sorted((OSC/'src').glob('*.cpp'))]
    sources+=[str(p) for p in sorted((OSC/'libs/oscpack/src').rglob('*.cpp')) if 'win32' not in str(p)]
    includes=[SC/'src',*sorted({p.parent for p in (SC/'src').rglob('*.h')}),
              SUPERCOLLIDER/'src',OSC/'src',OSC/'libs/oscpack/src',*sorted({p.parent for p in (OSC/'libs/oscpack/src').rglob('*.h')})]
    mixer.main(extra_sources=sources,extra_includes=includes,std='c++20')
