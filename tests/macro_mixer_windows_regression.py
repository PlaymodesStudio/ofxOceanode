#!/usr/bin/env python3
"""Node windows (SC instrument editors) inside MacroMixer sources.

Reuses the ASan/UBSan production Macro loader fixture with a fake editor node
exposing getWindowTitle/getWindowToggle. Windows start closed whatever the
source saved, open only from MacroMixer per source, show for both sources
during a transition (in place and in Mix mode) until the origin's close at
completion, adopt a close from the window itself but never an update-time
write, and persist with the mixer state.
No window or hardware.
"""
import macro_mixer_regression as mixer

mixer.CASES = r'''
// Draws "its window" while Show is on; can close it from that window (its X)
// or turn Show on during update, as a snapshot recall would.
class Editor : public ofxOceanodeNodeModel {
public:
    explicit Editor(const std::string& name="Editor"):ofxOceanodeNodeModel(name) {}
    void setup() override { addParameter(show.set("Show",false)); addParameter(level.set("Level",0,0,10)); }
    std::string getWindowTitle() override { return "SomeCanvas/"+nodeName()+" "+ofToString(getNumIdentifier()); }
    ofParameter<bool>* getWindowToggle() override { return &show; }
    void update(ofEventArgs&) override { if(recall) show=true; }
    void draw(ofEventArgs&) override {
        if(!show) return;
        ++drawn; id=getWindowId();
        if(closeOnDraw) show=false;
    }
    ofParameter<bool> show; ofParameter<float> level;
    int drawn=0; bool closeOnDraw=false, recall=false; std::string id;
};
struct EditorA : Editor { EditorA():Editor("EditorA") {} };
struct EditorB : Editor { EditorB():Editor("EditorB") {} };
static Editor* editor(const MacroMixerSource::Graph* graph,const std::string& model) {
    for(auto* node:graph->container->getAllModules())
        if(node->getNodeModel().nodeName()==model) return static_cast<Editor*>(&node->getNodeModel());
    return nullptr;
}
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<EditorA>("Test"); registry->registerModel<EditorB>("Test");
    registry->registerModel<ofxOceanodeNodeMacro>("Macro"); registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("WindowsTest");
    // Each source saves its editor with Show on, as BLODEV presets do.
    auto save=[&](const std::string& folder,bool b) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("WindowSource");
        c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),1).getNodeModel().getParameter<std::string>("Name")="Value";
        auto& e=static_cast<Editor&>(b ? c.createNode(std::make_unique<EditorB>(),1).getNodeModel() : c.createNode(std::make_unique<EditorA>(),1).getNodeModel());
        e.show=true; e.level=b?8:2;
        c.savePreset(folder);
    };
    const auto a=scratch+"/a",b=scratch+"/b"; save(a,false); save(b,true);
    MacroMixerRuntime r(parent); assert(r.importMacro(a) && r.importMacro(b));
    const auto aID=r.importedMacro(a)->id(), bID=r.importedMacro(b)->id();
    r.request(a,0,0); r.update(0); r.draw();
    auto* ea=editor(r.origin(),"EditorA"); assert(ea);
    // Saved Show = 1 never opens it inside the mixer.
    assert(!ea->show && ea->drawn==0 && r.showsWindows(aID) && !r.showsWindows(bID));
    auto listed=r.sourceWindows(aID);
    assert(listed.size()==1 && listed[0].key=="EditorA 1" && listed[0].title=="EditorA 1" && !listed[0].open);
    assert(r.sourceWindows(bID).empty() && r.openWindows(aID).empty());
    // Opened from MacroMixer, under a name naming the source.
    assert(r.setSourceWindow(aID,"EditorA 1",true) && !r.setSourceWindow("missing","EditorA 1",true));
    r.update(1); r.draw(); assert(ea->show && ea->drawn==1);
    assert(ea->id=="EditorA 1 ("+r.macro(aID)->name()+")" && r.sourceWindows(aID)[0].open);
    // A close from the window itself is remembered.
    ea->closeOnDraw=true; r.draw(); ea->closeOnDraw=false;
    assert(!ea->show && r.openWindows(aID).empty());
    r.update(2); r.draw(); assert(!ea->show && ea->drawn==2);
    // Show written during update (a recall) is not the user: forced closed.
    ea->recall=true; r.update(3); assert(!ea->show); r.draw(); assert(ea->drawn==2 && r.openWindows(aID).empty()); ea->recall=false;
    // In place, the one graph shows A's and B's choice for the whole morph,
    // each named after the source keeping it; A's close at completion.
    assert(r.setSourceWindow(aID,"EditorA 1",true) && r.setSourceWindow(bID,"EditorB 1",true));
    r.request(b,1000,0,true); r.update(10); assert(r.morphingInPlace());
    auto* eb=editor(r.origin(),"EditorB"); assert(eb && editor(r.origin(),"EditorA")==ea);
    assert(r.showsWindows(aID) && r.showsWindows(bID));
    r.draw(); assert(ea->show && eb->show && eb->drawn==1);
    assert(ea->id=="EditorA 1 ("+r.macro(aID)->name()+")" && eb->id=="EditorB 1 ("+r.macro(bID)->name()+")");
    r.update(400); r.draw(); assert(ea->show && eb->show);
    r.update(600); r.draw(); assert(ea->show && eb->show && eb->drawn==3);
    assert(r.sourceWindows(bID).size()==2 && r.sourceWindows(aID).size()==2);
    // A close during the morph is remembered by the source keeping it.
    eb->closeOnDraw=true; r.draw(); eb->closeOnDraw=false;
    assert(!eb->show && !r.openWindows(bID).count("EditorB 1") && r.openWindows(aID).count("EditorA 1"));
    assert(r.setSourceWindow(bID,"EditorB 1",true)); r.update(700); r.draw(); assert(eb->show);
    r.update(1020); assert(!r.transitioning() && r.currentID()==bID && !editor(r.origin(),"EditorA"));
    assert(!r.showsWindows(aID) && r.sourceWindows(aID).empty());
    r.draw(); assert(eb->show && r.openWindows(aID).count("EditorA 1"));
    // Persisted per source; older states have none; bad entries are refused.
    const auto saved=r.saveState();
    MacroMixerRuntime copy(parent); assert(copy.restoreState(saved));
    assert(copy.openWindows(aID)==std::set<std::string>{"EditorA 1"} && copy.openWindows(bID)==std::set<std::string>{"EditorB 1"});
    auto legacy=saved; for(auto& entry:legacy["macros"]) entry.erase("windows");
    assert(copy.restoreState(legacy) && copy.openWindows(aID).empty());
    auto bad=saved; bad["macros"][0]["windows"]=42; assert(!copy.restoreState(bad));
    bad=saved; bad["macros"][0]["windows"]={""}; assert(!copy.restoreState(bad));
    // Mix mode: two graphs, each showing its own source's windows until the
    // origin's graph retires.
    assert(copy.restoreState(saved)); copy.update(0); copy.draw();
    const auto* first=copy.origin(); auto* mb=editor(first,"EditorB"); assert(mb && mb->show);
    copy.request(a,1000,0); copy.update(10); assert(copy.transitioning() && !copy.morphMode());
    auto* ma=editor(copy.destination(),"EditorA"); assert(ma);
    copy.draw(); assert(mb->show && ma->show && ma->drawn==1);
    copy.update(600); copy.draw(); assert(mb->show && ma->show && ma->drawn==2);
    // A close is the graph's own source's alone.
    mb->closeOnDraw=true; copy.draw(); mb->closeOnDraw=false;
    assert(!mb->show && !copy.openWindows(bID).count("EditorB 1") && copy.openWindows(aID).count("EditorA 1"));
    assert(copy.setSourceWindow(bID,"EditorB 1",true));
    copy.update(1020); assert(!copy.transitioning() && copy.currentID()==aID); copy.draw(); assert(ma->show);
    // clear() keeps the choice; removing the source forgets it.
    copy.clear(); assert(copy.openWindows(aID).count("EditorA 1"));
    assert(copy.removeMacro(aID) && copy.openWindows(aID).empty());
    std::cout<<"MacroMixer windows passed: closed on load, opened per source, both sources' windows during transitions in place and in Mix mode, close adoption, update writes ignored, persistence.\n";
}
'''
if __name__ == '__main__': mixer.main()
