#!/usr/bin/env python3
"""MacroMixer morphs scPolyComb's own saved sound on the running node through the
node morph hooks. Production scPolyComb, no SuperCollider server."""
import macro_mixer_regression as mixer

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
    // Voices need other synths and buses: only the polyComb is replaced.
    MacroMixerRuntime s(parent); assert(s.importMacro(a)&&s.importMacro(voices));
    s.request(a,0,0,true); s.update(0);
    details=s.sourceDetails(s.importedMacro(voices)->id());
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="replaced"&&d.location=="SC_polycombBox_1"&&d.message.find("(Voices)")!=std::string::npos;}));
    s.clear();
    std::cout<<"polyComb node morph passed: hidden sound glides (log for decay), modes switch half way, Snapshot kept, cancellation restores, Voices replaces.\n";
}
'''

if __name__ == "__main__":
    sources=[str(mixer.ROOT/'src/Managers/ofxOceanodeScheduling.cpp')]
    sources+=[str(SC/'src'/name) for name in ('scNode.cpp','serverManager.cpp','scTimelineMenu.cpp',
             'nodes/scPolyComb.cpp','nodes/scModulation.cpp')]
    sources+=[str(p) for p in sorted((SUPERCOLLIDER/'src').glob('*.cpp'))]
    sources+=[str(p) for p in sorted((OSC/'src').glob('*.cpp'))]
    sources+=[str(p) for p in sorted((OSC/'libs/oscpack/src').rglob('*.cpp')) if 'win32' not in str(p)]
    includes=[SC/'src',*sorted({p.parent for p in (SC/'src').rglob('*.h')}),
              SUPERCOLLIDER/'src',OSC/'src',OSC/'libs/oscpack/src',*sorted({p.parent for p in (OSC/'libs/oscpack/src').rglob('*.h')})]
    mixer.main(extra_sources=sources,extra_includes=includes,std='c++20')
