#!/usr/bin/env python3
"""Unmatched Portals in an in-place MacroMixer morph.

Reuses the ASan/UBSan production Macro loader fixture with the real Portal
node. A Portal only one source has runs until completion (removed) or starts
with the morph (added) without a graph fallback. An added Portal that would
join a live Portal bus still falls back, because loading it publishes its
saved value to the running patch. Routers are unchanged. No window or hardware.
"""
import macro_mixer_regression as mixer

mixer.CASES = r'''
#include "portal.h"
static void near(float a,float b) { assert(std::abs(a-b)<.0001f); }
static bool has(const std::vector<MacroMixerMorphDetail>& details,const std::string& kind,const std::string& text="") {
    return std::any_of(details.begin(),details.end(),[&](const auto& d) {
        return d.kind==kind && (text.empty() || d.location.find(text)!=std::string::npos || d.message.find(text)!=std::string::npos);
    });
}
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    ofxOceanodeShared::initialize();
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<portal<float>>("Portal","f",0.f);
    registry->registerModel<ofxOceanodeNodeMacro>("Macro"); registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("PortalTest");
    auto portalNode=[](ofxOceanodeContainer& c,int id,const std::string& name,float value) -> portal<float>& {
        auto& p=static_cast<portal<float>&>(c.createNode(std::make_unique<portal<float>>("f",0.f),id).getNodeModel());
        p.setPortalName(name); p.setValue(value); return p;
    };
    // portals: a knob Portal "Thr" (id 30) drives a receiver (id 8) that
    // feeds the output. Without them the output holds its saved 5.
    // other: an unrelated live Portal named "Thr" in the same canvas.
    auto save=[&](const std::string& folder,bool portals,bool other=false) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("PortalSource");
        auto& out=c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),1).getNodeModel();
        out.getParameter<std::string>("Name")="Value"; out.getParameter<float>("Value")=5;
        if(portals) {
            portalNode(c,30,"Thr",.1f); portalNode(c,8,"Thr",.1f);
            assert(c.createConnectionFromInfo("Portal_f_8","Value","Router_f_1","Value"));
        }
        if(other) portalNode(c,1,"Thr",.7f);
        c.savePreset(folder);
    };
    const auto with=scratch+"/with",without=scratch+"/without",busy=scratch+"/busy";
    save(with,true); save(without,false); save(busy,false,true);
    MacroMixerContract contract;
    auto morph=[&](const std::string& from,const std::string& to,float start,float end) {
        MacroMixerRuntime r(parent); assert(r.setContract(contract) && r.importMacro(from) && r.importMacro(to));
        r.request(from,0,0,true); r.update(0); r.update(1); near(r.value(),start);
        const auto details=r.sourceDetails(r.importedMacro(to)->id());
        assert(!has(details,"fallback"));
        r.request(to,1000,0,true); r.update(10); assert(r.morphingInPlace());
        r.update(510); near(r.value(),start+(end-start)*.5f);
        r.update(1020); assert(!r.transitioning()); near(r.value(),end);
        return details;
    };
    // Removed: the Portals run until completion, the output blends to 5.
    auto details=morph(with,without,.1f,5);
    assert(has(details,"removed","Portal_f_30") && has(details,"removed","Portal_f_8"));
    // Added on a free bus: they start with the morph, the output blends to .1.
    details=morph(without,with,5,.1f);
    assert(has(details,"added","Portal_f_30") && has(details,"added","Portal_f_8"));
    {
        // A live "Thr" Portal would take the added Portals' saved value at once.
        MacroMixerRuntime r(parent); assert(r.setContract(contract) && r.importMacro(busy) && r.importMacro(with));
        r.request(busy,0,0,true); r.update(0); r.update(1);
        const auto details=r.sourceDetails(r.importedMacro(with)->id());
        assert(has(details,"fallback","Added Portal joins a live Portal bus"));
        assert(!has(details,"fallback","interface/service"));
        r.request(with,1000,0,true); r.update(10); assert(r.morphFallback());
    }
    std::cout<<"Portal morph passed: unmatched Portals morph in place; an added Portal on a live bus falls back.\n";
}
'''
if __name__ == '__main__': mixer.main()
