#!/usr/bin/env python3
"""MacroMixer scenes: playback driven by an outside, non-monotonic progress.

present() with a progress that moves forwards, backwards and across both ends,
commit and cancel by leaving the overlap, scene changes, the empty scene, a
morph on a second graph, per-transition mix policies, failures that must not
throw or reload every frame, and requests when no scene drives the mixer.
Reuses the ASan/UBSan production Macro loader fixture. No window or hardware;
no resource (audio, texture) routers, so backend readiness is not covered.
"""
import macro_mixer_regression as mixer

mixer.CASES = r'''
static void near(float a,float b) { if(!std::isfinite(a)||std::abs(a-b)>.0001f) { std::cerr<<a<<" != "<<b<<"\n"; std::abort(); } }
// Output follows X: a control the morph moves, read through the Value router.
class SceneProbe : public ofxOceanodeNodeModel {
public:
    static int built;
    SceneProbe():ofxOceanodeNodeModel("Scene Probe") { ++built; }
    ofParameter<float> x,output;
    void setup() override { addParameter(x.set("X",0,-100,100)); addOutputParameter(output.set("Output",0,-100,100)); }
    void update(ofEventArgs&) override { output=x.get(); }
};
int SceneProbe::built=0;
static SceneProbe& probe(const MacroMixerSource::Graph* graph) {
    return static_cast<SceneProbe&>(graph->container->getDynamicNodes().at("Scene Probe").at(1)->getNodeModel());
}
static auto& floating(ofxOceanodeContainer& c,int id=1) {
    return c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),id).getNodeModel();
}
static MacroMixerScene scene(const std::string& origin,const std::string& target="",float progress=0,bool morph=false) {
    MacroMixerScene s; s.origin=origin; s.target=target; s.progress=progress; s.morph=morph; return s;
}
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<SceneProbe>("Test"); registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("SceneTest");
    auto save=[&](const std::string& folder,float x) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("SceneSource");
        auto& out=floating(c); out.getParameter<std::string>("Name")="Value";
        auto& p=static_cast<SceneProbe&>(c.createNode(std::make_unique<SceneProbe>(),1).getNodeModel()); p.x=x;
        assert(c.createConnectionFromInfo("Scene_Probe_1","Output","Router_f_1","Value"));
        c.savePreset(folder);
    };
    const auto a=scratch+"/a",b=scratch+"/b",c=scratch+"/c"; save(a,2); save(b,6); save(c,10);
    double clock=0;
    // One frame of a scene-driven host: the scene, then the update.
    auto show=[&](MacroMixerRuntime& r,const MacroMixerScene& s) { r.present(s); r.update(clock+=16); return r.value(); };
    {
        // Mix. The default Float policy is linear: A=2, B=6, C=10.
        MacroMixerRuntime r(parent); assert(r.importMacro(a)&&r.importMacro(b)&&r.importMacro(c));
        const auto A=r.importedMacro(a)->id(),B=r.importedMacro(b)->id(),C=r.importedMacro(c)->id();
        assert(!r.sceneDriven()&&!r.sceneReady()&&r.sceneStatus().empty());
        const int builds=SceneProbe::built;
        near(show(r,scene(A)),2);
        assert(r.sceneDriven()&&r.sceneReady()&&r.currentID()==A&&!r.destination()&&SceneProbe::built==builds+1);
        assert(r.state()==MacroMixerRuntime::State::Running&&r.sceneStatus()==r.macro(A)->name());
        const auto* ga=r.origin();
        // Forwards. The origin's graph is kept; the destination loads at once.
        near(show(r,scene(A,B,0)),2);
        assert(r.origin()==ga&&r.destination()&&r.destinationID()==B&&r.sceneReady()&&SceneProbe::built==builds+2);
        assert(!r.morphing()&&!r.morphMode()&&!r.outputsBypassed());
        const auto* gb=r.destination();
        near(show(r,scene(A,B,.25f)),3); near(r.progress(),.25f);
        near(show(r,scene(A,B,.75f)),5); r.draw(); near(r.value(),5);
        // Backwards.
        near(show(r,scene(A,B,.5f)),4); near(r.progress(),.5f);
        // Both ends, in both directions: 1 does not commit and 0 does not cancel.
        near(show(r,scene(A,B,1)),6); assert(r.transitioning()&&r.origin()==ga&&r.destination()==gb&&r.currentID()==A);
        near(show(r,scene(A,B,0)),2); assert(r.transitioning()&&r.origin()==ga&&r.destination()==gb);
        near(show(r,scene(A,B,1)),6); near(show(r,scene(A,B,1)),6);
        near(show(r,scene(A,B,0)),2); assert(r.origin()==ga&&r.destination()==gb&&SceneProbe::built==builds+2);
        // Out of range and non-finite progress is clamped.
        near(show(r,scene(A,B,7)),6); near(show(r,scene(A,B,-3)),2); near(show(r,scene(A,B,NAN)),2);
        // A scene's playback is not saved state, and requests are refused.
        assert(r.saveState().at("playback").is_null()&&r.saveState().at("morph")==false);
        r.request(c,0,0); assert(r.queuedID().empty()&&!r.diagnostics().empty());
        r.requestMacro(C,0,0); assert(r.queuedID().empty());
        near(show(r,scene(A,B,.5f)),4); assert(r.origin()==ga&&r.destination()==gb);
        // Leaving to the right commits: the destination's graph is the origin.
        near(show(r,scene(B)),6);
        assert(r.origin()==gb&&!r.destination()&&r.currentID()==B&&r.sceneReady()&&SceneProbe::built==builds+2);
        // Entering from the right keeps B as the destination and loads A.
        near(show(r,scene(A,B,.5f)),4);
        assert(r.destination()==gb&&r.currentID()==A&&SceneProbe::built==builds+3);
        const auto* ga2=r.origin();
        // Leaving to the left cancels: the origin plays on, the destination goes.
        near(show(r,scene(A)),2); assert(r.origin()==ga2&&!r.destination()&&SceneProbe::built==builds+3);
        // Straight to a third source from inside an overlap.
        near(show(r,scene(A,B,.5f)),4); assert(SceneProbe::built==builds+4);
        near(show(r,scene(C)),10); assert(r.currentID()==C&&!r.destination()&&SceneProbe::built==builds+5);
        // A playing source stays as the origin of its next transition.
        const auto* gc=r.origin();
        near(show(r,scene(C,A,.5f)),6); assert(r.origin()==gc&&SceneProbe::built==builds+6);
        // Straight to another overlap: A (the destination) becomes its origin.
        const auto* ga3=r.destination();
        near(show(r,scene(A,B,.5f)),4); assert(r.origin()==ga3&&r.destinationID()==B&&SceneProbe::built==builds+7);
        // Roles swapped: both graphs are kept.
        const auto* gb2=r.destination();
        near(show(r,scene(B,A,.25f)),5); assert(r.origin()==gb2&&r.destination()==ga3&&SceneProbe::built==builds+7);
        // A source overlapping itself is that source alone.
        near(show(r,scene(B,B,.5f)),6); assert(r.origin()==gb2&&!r.destination());
        // A mix policy for this transition only, by router ID.
        auto switched=scene(B,A,.5f);
        switched.routerOverrides["value"].policy="switch"; switched.routerOverrides["value"].options={{"threshold",.9}};
        near(show(r,switched),6); switched.progress=.95f; near(show(r,switched),2);
        near(show(r,scene(B,A,.5f)),4); // The contract's policy again.
        auto unknown=scene(B,A,.5f); unknown.routerOverrides["value"].policy="no such policy";
        near(show(r,unknown),4); assert(r.sceneReady());
        // The empty scene: no graphs, outputs at their defaults.
        near(show(r,scene("")),0);
        assert(!r.origin()&&!r.destination()&&r.sceneDriven()&&r.sceneReady()&&r.sceneStatus()=="Empty");
        assert(r.state()==MacroMixerRuntime::State::Empty); near(std::get<float>(r.outputValue("value")),0);
        near(show(r,scene("",B,.5f)),0); assert(!r.origin()); // No origin: nothing plays.
        r.draw();
        // Into an overlap from nothing: both graphs load in one call.
        const int before=SceneProbe::built;
        near(show(r,scene(A,B,.5f)),4); assert(SceneProbe::built==before+2&&r.sceneReady());
        assert(r.sceneStatus()==r.macro(A)->name()+" -> "+r.macro(B)->name());

        // Failures are reported, never thrown, and not retried every frame.
        near(show(r,scene("")),0);
        near(show(r,scene("no such source")),0);
        assert(!r.sceneReady()&&!r.sceneStatus().empty()&&!r.origin()&&!r.diagnostics().empty());
        near(show(r,scene(A)),2); assert(r.sceneReady());
        fs::rename(b,b+"-offline");
        const int offline=SceneProbe::built; const auto* playing=r.origin();
        near(show(r,scene(A,B,.5f)),2);
        assert(!r.sceneReady()&&!r.sceneStatus().empty()&&r.origin()==playing&&!r.destination());
        fs::rename(b+"-offline",b);
        near(show(r,scene(A,B,.5f)),2); assert(!r.sceneReady()&&SceneProbe::built==offline); // Not read again by itself.
        // Reading the source again (Refresh) lets the scene load it.
        assert(r.importMacro(b,true));
        near(show(r,scene(A,B,.5f)),4); assert(r.sceneReady()&&r.origin()==playing&&SceneProbe::built==offline+1);
        // So does a scene that stopped naming it.
        fs::rename(b,b+"-offline");
        near(show(r,scene(A)),2); near(show(r,scene(A,B,.5f)),2); assert(!r.sceneReady());
        fs::rename(b+"-offline",b);
        near(show(r,scene(A)),2); assert(r.sceneReady());
        near(show(r,scene(A,B,.5f)),4); assert(r.sceneReady());

        // clear() hands playback back to requests.
        r.clear(); assert(!r.sceneDriven()&&!r.origin());
        r.request(a,0,0); r.update(clock+=16); near(r.value(),2); assert(r.currentID()==A);
        r.request(b,1000,0); r.update(clock+=10); assert(r.destination());
        r.update(clock+=500); near(r.value(),4);
        // A scene takes over a clock-driven transition and keeps its graphs.
        const auto* from=r.origin(); const auto* to=r.destination();
        near(show(r,scene(A,B,.25f)),3); assert(r.sceneDriven()&&r.origin()==from&&r.destination()==to);
        near(show(r,scene(A,B,1)),6); assert(r.transitioning()); // The clock no longer completes it.
        near(show(r,scene(B)),6); assert(r.origin()==to&&!r.destination());
    }
    {
        // Morph on a second graph: the destination's controls start from the
        // origin's values and the router outputs still crossfade.
        MacroMixerRuntime m(parent); assert(m.importMacro(a)&&m.importMacro(b));
        const auto A=m.importedMacro(a)->id(),B=m.importedMacro(b)->id();
        near(show(m,scene(A)),2); const auto* ga=m.origin();
        near(show(m,scene(A,B,.5f,true)),3); // X: 2 -> 6 at .5 is 4; outputs 2 and 4 mix to 3.
        assert(m.morphing()&&m.morphMode()&&m.morphFallback()&&!m.morphingInPlace()&&!m.outputsBypassed()&&m.origin()==ga&&m.sceneReady());
        assert(m.hasTransitionDetails(B));
        const auto* gb=m.destination();
        near(probe(gb).x,4); near(probe(ga).x,2);
        // 1 leaves the morph prepared: it goes back.
        near(show(m,scene(A,B,1,true)),6); near(probe(gb).x,6); assert(m.morphing()&&m.destination()==gb);
        near(show(m,scene(A,B,.25f,true)),2.25f); near(probe(gb).x,3);
        near(show(m,scene(A,B,0,true)),2); near(probe(gb).x,2); assert(m.morphing());
        near(show(m,scene(A,B,1,true)),6); near(show(m,scene(A,B,.5f,true)),3); near(probe(gb).x,4);
        // Leaving to the right from mid progress commits on the destination's own values.
        near(show(m,scene(B)),6); assert(m.origin()==gb&&!m.destination()&&!m.morphing()&&!m.morphMode()); near(probe(gb).x,6);
        near(show(m,scene(B)),6);
        // Entered from the right: the kept destination morphs from a new origin.
        near(show(m,scene(A,B,.5f,true)),3); assert(m.destination()==gb&&m.morphing()); near(probe(gb).x,4);
        near(show(m,scene(A,B,.75f,true)),4.25f); near(probe(gb).x,5);
        // Leaving to the left cancels.
        near(show(m,scene(A)),2); assert(!m.destination()&&!m.morphing()&&m.currentID()==A);
        // Mix and Morph switched inside the overlap.
        near(show(m,scene(A,B,.5f,true)),3); near(probe(m.destination()).x,4);
        near(show(m,scene(A,B,.5f,false)),4); near(probe(m.destination()).x,6); assert(!m.morphing());
        near(show(m,scene(A,B,.5f,true)),3); near(probe(m.destination()).x,4); assert(m.morphing());
        m.draw(); near(m.value(),3);
        near(show(m,scene("")),0); assert(!m.origin()&&!m.morphing());
    }
    {
        // Without a scene, requests run on the clock as before.
        MacroMixerRuntime r(parent); assert(r.importMacro(a)&&r.importMacro(b));
        r.request(a,0,0); r.update(0); near(r.value(),2); assert(!r.sceneDriven());
        const auto* origin=r.origin();
        r.request(b,1000,0); r.update(10); assert(r.destination()&&r.origin()==origin);
        r.update(510); near(r.value(),4); near(r.progress(),.5f);
        assert(r.saveState().at("playback")==r.destinationID());
        r.update(1010); near(r.value(),6); assert(!r.destination()&&r.origin()!=origin&&r.currentID()==r.importedMacro(b)->id());
        r.request(a,1000,0,true); r.update(1020); r.finish(); r.update(1030); near(r.value(),2);
        assert(!r.destination()&&r.morphMode()&&r.currentID()==r.importedMacro(a)->id());
    }
    std::cout<<"MacroMixer scene tests passed.\n";
}
'''
if __name__ == "__main__":
    mixer.main()
