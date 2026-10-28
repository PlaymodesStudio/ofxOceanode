#!/usr/bin/env python3
"""Unmatched Portals in an in-place MacroMixer morph.

Reuses the ASan/UBSan production Macro loader fixture with the real Portal
node. A Portal only one source has runs until completion (removed) or starts
with the morph (added) without a graph fallback. A Local Portal bus the
running graph uses and the target changes blends in place: from the origin's
driver or the live bus value to the target's driver or its saved value, through
one Portal, and the bus spreads it. Added Portals on it never publish their
saved value. A bus changing drivers among several cabled Portals falls back.
Routers are unchanged. No window or hardware.
"""
import macro_mixer_regression as mixer

mixer.CASES = r'''
#include "portal.h"
static void near(float a,float b) { if(std::abs(a-b)>=.0001f) { std::cerr<<a<<" != "<<b<<"\n"; assert(false); } }
static std::string morphKey(ofxOceanodeNode* n) { return ofJoinString(ofSplitString(n->getNodeModel().nodeName()," "),"_")+"_"+ofToString(n->getNodeModel().getNumIdentifier()); }
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
    auto morph=[&](const std::string& from,const std::string& to,float start,float end,float mid=NAN) {
        MacroMixerRuntime r(parent); assert(r.setContract(contract) && r.importMacro(from) && r.importMacro(to));
        r.request(from,0,0,true); r.update(0); r.update(1); near(r.value(),start);
        const auto details=r.sourceDetails(r.importedMacro(to)->id());
        assert(!has(details,"fallback"));
        r.request(to,1000,0,true); r.update(10); assert(r.morphingInPlace());
        r.update(510); near(r.value(),std::isnan(mid) ? start+(end-start)*.5f : mid);
        r.update(1020); assert(!r.transitioning()); near(r.value(),end);
        return details;
    };
    // Removed: the Portals run until completion, the output blends to 5.
    auto details=morph(with,without,.1f,5);
    assert(has(details,"removed","Portal_f_30") && has(details,"removed","Portal_f_8"));
    // Added on a free bus: they start with the morph, the output blends to .1.
    details=morph(without,with,5,.1f);
    assert(has(details,"added","Portal_f_30") && has(details,"added","Portal_f_8"));
    // A live "Thr" knob (.7) meets the added "Thr" Portals saved at .1: the bus
    // blends .7 -> .1 while the output cable blends 5 -> Portal 8.
    details=morph(busy,with,5,.1f,2.7f);
    assert(has(details,"mix","Portal bus Thr") && has(details,"mix","Bus value -> Saved value"));
    // driven: Feed (3) drives "Thr" through a cabled Portal 30; Portal 8
    // passes it to the output. extra 1 adds an uncabled "Thr" Portal saved at
    // 9 (BLODEV_2's leftover VEC_POS_X_1); extra 2 cables Feed into it.
    auto driven=[&](const std::string& folder,int extra) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("PortalSource");
        auto& out=c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),1).getNodeModel();
        out.getParameter<std::string>("Name")="Value"; out.getParameter<float>("Value")=5;
        auto& feed=c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),2).getNodeModel();
        feed.getParameter<std::string>("Name")="Feed"; feed.getParameter<float>("Value")=3;
        portalNode(c,30,"Thr",3); portalNode(c,8,"Thr",3);
        assert(c.createConnectionFromInfo("Router_f_2","Value","Portal_f_30","Value"));
        assert(c.createConnectionFromInfo("Portal_f_8","Value","Router_f_1","Value"));
        if(extra) portalNode(c,46,"Thr",9);
        if(extra==2) assert(c.createConnectionFromInfo("Router_f_2","Value","Portal_f_46","Value"));
        c.savePreset(folder);
    };
    const auto plain=scratch+"/driven",joined=scratch+"/joined",fed=scratch+"/fed";
    driven(plain,0); driven(joined,1); driven(fed,2);
    // Joins the driven bus with its value: the output never sees the saved 9.
    details=morph(plain,joined,3,3);
    assert(has(details,"added","Portal_f_46") && has(details,"added","Portal bus's value"));
    {
        // Two cabled Portals on one changing bus: no single driver to blend.
        MacroMixerRuntime r(parent); assert(r.setContract(contract) && r.importMacro(plain) && r.importMacro(fed));
        r.request(plain,0,0,true); r.update(0); r.update(1);
        assert(has(r.sourceDetails(r.importedMacro(fed)->id()),"fallback","several cabled Portals"));
        r.request(fed,1000,0,true); r.update(10); assert(r.morphFallback());
    }
    // bus: the output reads "Thr" through Portal 8. Feed (3) and Feed2 (9)
    // exist in every source. driver: "feed", "feed2", or "" for a knob Portal
    // 30 saved at knob; carrier 50 moves the driver to an added Portal 50.
    auto bus=[&](const std::string& folder,const std::string& driver,float knob,int carrier=30) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("PortalSource");
        auto routerNode=[&](int id,const std::string& name,float value) {
            auto& r=c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),id).getNodeModel();
            r.getParameter<std::string>("Name")=name; r.getParameter<float>("Value")=value;
        };
        routerNode(1,"Value",5); routerNode(2,"Feed",3); routerNode(3,"Feed2",9);
        const float value=driver=="feed" ? 3 : driver=="feed2" ? 9 : knob;
        portalNode(c,carrier,"Thr",value); portalNode(c,8,"Thr",value);
        const auto into="Portal_f_"+std::to_string(carrier);
        if(driver=="feed") assert(c.createConnectionFromInfo("Router_f_2","Value",into,"Value"));
        if(driver=="feed2") assert(c.createConnectionFromInfo("Router_f_3","Value",into,"Value"));
        assert(c.createConnectionFromInfo("Portal_f_8","Value","Router_f_1","Value"));
        c.savePreset(folder);
    };
    const auto knob2=scratch+"/knob2",knob8=scratch+"/knob8",feed=scratch+"/feed",feed2=scratch+"/feed2",moved=scratch+"/moved",knob7=scratch+"/knob7";
    bus(knob2,"",2); bus(knob8,"",8); bus(feed,"feed",0); bus(feed2,"feed2",0); bus(moved,"feed2",0,50); bus(knob7,"",7);
    // A matched knob's saved value: the bus glides like any control.
    details=morph(knob2,knob8,2,8);
    assert(has(details,"mix","Portal bus Thr") && !has(details,"fallback"));
    // A matched Portal's driver changes: its sources blend, not the bus.
    details=morph(feed,feed2,3,9);
    assert(has(details,"mix","Router_f_2 / Value -> Router_f_3 / Value"));
    // The driver moves to an added Portal; the origin's one retires.
    details=morph(feed,moved,3,9);
    assert(has(details,"removed","Portal_f_30") && has(details,"added","Portal_f_50") && has(details,"mix","Portal bus Thr"));
    // Driven, then a knob: it ends at the saved value.
    morph(feed,knob7,3,7); morph(knob7,feed2,7,9);
    {
        // Native wiring after completion: Feed2 still drives the bus.
        MacroMixerRuntime r(parent); assert(r.setContract(contract) && r.importMacro(feed) && r.importMacro(moved));
        r.request(feed,0,0,true); r.update(0); r.update(1);
        r.request(moved,1000,0,true); r.update(10); r.update(1020); assert(!r.transitioning());
        auto* graph=r.origin(); MacroMixerSource::Graph* g=const_cast<MacroMixerSource::Graph*>(graph);
        for(auto* n:g->container->getAllModules()) if(n->getNodeModel().nodeName()=="Router f" && n->getNodeModel().getParameter<std::string>("Name").get()=="Feed2")
            n->getNodeModel().getParameter<float>("Value")=4;
        r.update(1030); near(r.value(),4);
        // Cancelling restores the origin driver: Feed2 moves the bus again.
        r.request(feed,1000,0,true); r.update(1040); assert(r.morphingInPlace()); r.update(1540);
        const_cast<MacroMixerSource::Graph*>(r.destination())->cancelMorph();
        for(auto* n:g->container->getAllModules()) if(n->getNodeModel().nodeName()=="Router f" && n->getNodeModel().getParameter<std::string>("Name").get()=="Feed2")
            n->getNodeModel().getParameter<float>("Value")=6;
        for(auto* n:g->container->getAllModules()) if(morphKey(n)=="Router_f_1") near(n->getNodeModel().getParameter<float>("Value"),6);
    }
    std::cout<<"Portal morph passed: unmatched Portals morph in place; Local Portal buses blend knob values and drivers in place, end natively and cancel; several changing drivers fall back.\n";
}
'''
if __name__ == '__main__': mixer.main()
