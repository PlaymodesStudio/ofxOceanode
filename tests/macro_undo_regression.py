#!/usr/bin/env python3
"""Native macro encapsulation/extraction history with ASan/UBSan and real OF.

Covers boundary fan-out, stable node pairing, name/ID collisions, nested macros,
parameter edits excluded from history, atomic rollback, GUI/snapshot migration
(including shared panels), and safe deferred undo from the child canvas.
Rendering and scope/timeline services use the existing headless host adapters.
"""
from pathlib import Path
import contextlib
import os
import tempfile
import macro_mixer_regression as native
from new_preset_regression import block

CASES = r'''
class Probe : public ofxOceanodeNodeModel {
public:
    Probe():ofxOceanodeNodeModel("Probe"){}
    void setup() override {
        addParameter(input.set("In",0.f,-100.f,100.f));
        addParameter(output.set("Out",0.f,-100.f,100.f));
        addInspectorParameter(marker.set("Marker",0.f,-100.f,100.f));
        listener=input.newListener([this](float& v){output=v;});
    }
private:
    ofParameter<float> input, output, marker;ofEventListener listener;
};
static auto& pin(ofxOceanodeNode& n,const string& name){return static_cast<ofxOceanodeAbstractParameter&>(n.getParameters().get(name));}
static auto* probe(ofxOceanodeContainer& c,int id){return c.getDynamicNodes().at("Probe").at(id).get();}
static float marker(ofxOceanodeNode& n){return n.getNodeModel().getInspectorParameter<float>("Marker").get();}
static auto* macroNode(ofxOceanodeContainer& c){
    for(auto* n:c.getAllModules())if(dynamic_cast<ofxOceanodeNodeMacro*>(&n->getNodeModel()))return n;
    return static_cast<ofxOceanodeNode*>(nullptr);
}
static void verifyPublications(ofxOceanodeContainer& c){
    for(const auto& p:c.customGuiPanelsData){
        for(const auto& w:p.layout.widgets){
            const auto& path=w.parameterRef.parameterPath;if(path.empty())continue;
            const auto slash=path.find('/');assert(slash!=string::npos);
            assert(c.getParameterGroupNodesMap().count(path.substr(0,slash)));
            assert(c.getParameterGroupNodesMap().at(path.substr(0,slash))->getParameters().contains(path.substr(slash+1)));
        }
    }
    for(const auto& b:c.customGuiSnapshotBanks)for(const auto& s:b.snapshots)for(const auto& v:s.parameterValues){
        const auto slash=v.first.find('/');assert(c.getParameterGroupNodesMap().count(v.first.substr(0,slash)));
    }
}
// Records the timeline path repairs the container hands to its automation service.
struct PathRecorder : ofxOceanodeAutomationExtension {
    std::vector<std::map<std::string, std::string>> calls;
    void retargetParameterPaths(const std::map<std::string, std::string>& oldToNew) override { calls.push_back(oldToNew); }
    void clear() override {} void beginParameterCache() override {} void evaluateAutomation() override {}
    void applyAutomation() override {} void endParameterCache() override {}
    bool loadPreset(const std::string&) override { return true; } bool savePreset(const std::string&) const override { return true; }
    bool getPlayOnPresetLoad() const override { return false; } bool isBpmAutomationEnabled() const override { return false; }
    bool preservesSecondsOnBpmChange() const override { return false; } void retimeForBpmChange(double, double) override {}
    double getBeatsPerBar() const override { return 4; } bool didLoopWrapThisFrame() const override { return false; }
    bool getParameterTrackColor(const ofxOceanodeAbstractParameter&, ofColor&) const override { return false; }
    void drawParameterMenu(ofxOceanodeAbstractParameter&) override {} void drawNodeMenu(ofxOceanodeNodeModel&) override {}
    void removeParameterBindings(ofxOceanodeAbstractParameter&) override {}
    void collectBoundParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const override {}
};
static CustomGuiWidget widget(string path){CustomGuiWidget w;w.parameterRef.parameterPath=path;return w;}
static void addPanels(ofxOceanodeContainer& c){
    CustomGuiPanelData a;a.id="inside";a.name="Inside";a.layout.widgets={widget("Probe_2/In"),widget("Probe_3/In")};
    CustomGuiPanelData b;b.id="mixed";b.name="Mixed";b.layout.widgets={widget("Probe_1/In"),widget("Probe_2/In")};
    c.customGuiPanelsData={a,b};
    for(const auto& p:c.customGuiPanelsData){
        CustomGuiSnapshotBank bank;bank.customGuiId=p.id;bank.customGuiName=p.name;
        CustomGuiSnapshotData snapshot;snapshot.id="s";snapshot.name="Saved";
        for(const auto& w:p.layout.widgets)snapshot.parameterValues[w.parameterRef.parameterPath]={"float",.15f};
        bank.snapshots.push_back(snapshot);c.customGuiSnapshotBanks.push_back(bank);
    }
}
int main(int argc,char** argv){
    assert(argc==3);namespace fs=std::filesystem;fs::current_path(argv[2]);ofDisableDataPath();ofSeedRandom();
    auto window=make_shared<ofAppNoWindow>();ofGetMainLoop()->setCurrentWindow(window);
    auto registry=make_shared<ofxOceanodeNodeRegistry>();auto types=make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f");singleton->registerType<float>("f");
    types->registerType<vector<float>>("v_f");singleton->registerType<vector<float>>("v_f");
    types->registerType<int>("i");singleton->registerType<int>("i");
    types->registerType<bool>("b");singleton->registerType<bool>("b");
    registry->registerModel<Probe>("Test");registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<router<vector<float>>>("Router","v_f",0.f,-100.f,100.f);
    auto transport=make_shared<ofxOceanodeTransport>();
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Test");
        for(int i=1;i<=5;++i){auto& n=c.createNode(make_unique<Probe>(),i);n.getNodeGui().setPosition({float(i*100),float(i*20)});n.getNodeModel().getInspectorParameter<float>("Marker")=float(i*11);}
        auto* source=probe(c,1);auto* sink=probe(c,4);auto* fan=probe(c,5);
        probe(c,2)->getNodeModel().getParameter<float>("In")=.2f;
        probe(c,3)->getNodeModel().getParameter<float>("In")=.3f;
        assert(c.createConnection(pin(*source,"Out"),pin(*probe(c,2),"In")));
        assert(c.createConnection(pin(*probe(c,2),"Out"),pin(*probe(c,3),"In")));
        assert(c.createConnection(pin(*probe(c,3),"Out"),pin(*sink,"In")));
        assert(c.createConnection(pin(*probe(c,3),"Out"),pin(*fan,"In")));
        source->getNodeModel().getParameter<float>("In")=.25f;
        addPanels(c);
        auto ownedRecorder=std::make_unique<PathRecorder>();auto* recorder=ownedRecorder.get();
        c.setAutomationExtension(std::move(ownedRecorder));
        const auto before=c.captureCanvasGraph();const auto beforeAux=c.captureCanvasAuxiliary();
        auto* wrapper=c.encapsulateNodes({probe(c,3),probe(c,2)},"Pair");assert(wrapper);
        {
            // Timeline bindings follow the encapsulated nodes, and paths resolve
            // relative to any container.
            auto inside=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer();
            assert(recorder->calls.size()==1);
            const auto& moved=recorder->calls.back();
            const std::string canvas=inside->getCanvasID();
            assert(canvas=="Test / Macro "+ofToString(wrapper->getNodeModel().getNumIdentifier()));
            assert(moved.at("Test::Probe_2/In")==canvas+"::Probe_1/In");
            assert(moved.at("Test::Probe_3/Out")==canvas+"::Probe_2/Out");
            auto& in=pin(*probe(*inside,1),"In");
            assert(inside->findTimelineParameter("Probe_1/In")==&in);
            const std::string fromParent="Macro "+ofToString(wrapper->getNodeModel().getNumIdentifier())+"::Probe_1/In";
            assert(c.findTimelineParameter(fromParent)==&in);
            assert(c.getTimelineParameterPath(in,*inside)=="Probe_1/In");
            assert(c.getTimelineParameterPath(in,c)==fromParent);
            assert(c.getTimelineParameterPath(pin(*source,"In"),*inside).empty()); // outside that macro
        }
        assert(c.canvasUndo.size()==1 && c.getAllModules().size()==4);
        auto& macro=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel());auto child=macro.getContainer();
        assert(child->getAllModules().size()==4); // Two probes and two boundary routers.
        assert(marker(*probe(*child,1))==22.f && marker(*probe(*child,2))==33.f);
        assert(c.customGuiPanelsData.size()==1 && child->customGuiPanelsData.size()==2);
        verifyPublications(c);verifyPublications(*child);
        source->getNodeModel().getParameter<float>("In")=.42f;
        assert(sink->getNodeModel().getParameter<float>("In")==.42f);
        assert(fan->getNodeModel().getParameter<float>("In")==.42f);
        // Parameter edits are NOT undo steps and must survive moving nodes back.
        probe(*child,1)->getNodeModel().getInspectorParameter<float>("Marker")=72.f;
        child->requestCanvasHistory();assert(c.getAllModules().size()==4);
        c.processCanvasHistoryRequest(); // Safe boundary, outside all canvas draws.
        assert(!macroNode(c) && c.getAllModules().size()==5 && c.getAllConnections().size()==4);
        assert(c.captureCanvasGraph()==before && c.captureCanvasAuxiliary()==beforeAux);
        assert(marker(*probe(c,2))==72.f && marker(*probe(c,3))==33.f);
        probe(c,2)->getNodeModel().getInspectorParameter<float>("Marker")=73.f;
        assert(c.redoCanvasEdit());wrapper=macroNode(c);assert(wrapper);
        child=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer();
        assert(marker(*probe(*child,1))==73.f);verifyPublications(*child);
        // A child edit precedes the parent's structural undo, regardless of focus.
        child->beginCanvasEdit();probe(*child,2)->getNodeGui().setPosition({700,300});child->endCanvasEdit();
        assert(c.undoCanvasEdit());assert(macroNode(c));
        assert(c.undoCanvasEdit());assert(!macroNode(c));assert(c.redoCanvasEdit());
        wrapper=macroNode(c);child=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer();
        assert(!child->canRedoCanvasEdit()); // Moved/recreated child histories are folded into the parent step.
        // Extract with collisions in the destination; keep both router bridges.
        const auto encapsulated=c.captureCanvasGraph();const auto encapsulatedAux=c.captureCanvasAuxiliary();
        const std::string extractedCanvas=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer()->getCanvasID();
        const size_t callsBefore=recorder->calls.size();
        assert(c.extractMacro(*wrapper));
        {
            // Extracted nodes' bindings follow them out, onto names that exist here.
            assert(recorder->calls.size()==callsBefore+1);
            for(const auto& entry:recorder->calls.back()){
                assert(entry.first.compare(0,extractedCanvas.size()+2,extractedCanvas+"::")==0);
                assert(entry.second.compare(0,6,"Test::")==0 && c.findTimelineParameter(entry.second.substr(6))!=nullptr);
            }
            assert(recorder->calls.back().count(extractedCanvas+"::Probe_1/In"));
        }
        assert(!macroNode(c) && c.getAllModules().size()==7 && c.getAllConnections().size()==6);
        verifyPublications(c);assert(c.customGuiPanelsData.size()==3);
        source->getNodeModel().getParameter<float>("In")=.57f;
        assert(sink->getNodeModel().getParameter<float>("In")==.57f && fan->getNodeModel().getParameter<float>("In")==.57f);
        ofxOceanodeNode* moved=nullptr;for(auto* n:c.getAllModules())if(n->getNodeModel().nodeName()=="Probe" && marker(*n)==73.f)moved=n;
        assert(moved);moved->getNodeModel().getInspectorParameter<float>("Marker")=74.f;
        assert(c.undoCanvasEdit());assert(c.captureCanvasGraph()==encapsulated && c.captureCanvasAuxiliary()==encapsulatedAux);
        child=static_cast<ofxOceanodeNodeMacro&>(macroNode(c)->getNodeModel()).getContainer();
        assert(marker(*probe(*child,1))==74.f);assert(c.redoCanvasEdit());verifyPublications(c);
        // Undo extraction, then undo encapsulation: one step for each operation.
        assert(c.undoCanvasEdit());assert(c.undoCanvasEdit());assert(c.captureCanvasGraph()==before);
        assert(marker(*probe(c,2))==74.f);
    }
    // A missing boundary type must reject before cutting the user's graph.
    {
        auto bare=make_shared<ofxOceanodeNodeRegistry>();bare->registerModel<Probe>("Test");bare->registerModel<ofxOceanodeNodeMacro>("Macro");
        ofxOceanodeContainer c(bare,types,transport);c.setCanvasID("Missing");
        auto& a=c.createNode(make_unique<Probe>(),1);auto& b=c.createNode(make_unique<Probe>(),2);
        assert(c.createConnection(pin(a,"Out"),pin(b,"In")));const auto before=c.captureCanvasGraph();
        assert(!c.encapsulateNodes({&b}));assert(c.captureCanvasGraph()==before && !c.canUndoCanvasEdit());
    }
    // A failed macro factory leaves the graph and earlier history intact.
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Rollback");
        auto& n=c.createNode(make_unique<Probe>(),1);n.getNodeGui().setPosition({0,0});n.getNodeModel().getInspectorParameter<float>("Marker")=19.f;
        c.beginCanvasEdit();n.getNodeGui().setPosition({90,40});c.endCanvasEdit();
        const auto before=c.captureCanvasGraph();
        auto& factories=const_cast<ofxOceanodeNodeRegistry::registeredModelsMap&>(registry->getRegisteredModels());
        const auto saved=factories.at("Macro");
        factories["Macro"]=[](){return std::unique_ptr<ofxOceanodeNodeModel>();};
        assert(!c.encapsulateNodes({&n}));factories["Macro"]=saved;
        assert(c.captureCanvasGraph()==before && marker(*probe(c,1))==19.f);
        assert(c.undoCanvasEdit() && probe(c,1)->getNodeGui().getPosition()==glm::vec2(0));
    }
    // A failed leaf factory during staging leaves the original instance intact.
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Staging failure");
        auto& n=c.createNode(make_unique<Probe>(),1);const auto before=c.captureCanvasGraph();
        auto& factories=const_cast<ofxOceanodeNodeRegistry::registeredModelsMap&>(registry->getRegisteredModels());
        const auto saved=factories.at("Probe");factories["Probe"]=[](){return std::unique_ptr<ofxOceanodeNodeModel>();};
        assert(!c.encapsulateNodes({&n}));factories["Probe"]=saved;
        assert(c.captureCanvasGraph()==before && probe(c,1)==&n);
    }
    // Extraction also validates destination constructors before destroying the macro.
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Extraction staging");
        auto& wrapper=c.createNode(make_unique<ofxOceanodeNodeMacro>(),1);
        auto child=static_cast<ofxOceanodeNodeMacro&>(wrapper.getNodeModel()).getContainer();
        child->createNode(make_unique<Probe>(),1);
        const auto before=c.captureCanvasGraph();
        auto& factories=const_cast<ofxOceanodeNodeRegistry::registeredModelsMap&>(registry->getRegisteredModels());
        const auto saved=factories.at("Probe");factories["Probe"]=[](){return std::unique_ptr<ofxOceanodeNodeModel>();};
        assert(!c.extractMacro(wrapper));factories["Probe"]=saved;
        assert(c.captureCanvasGraph()==before && macroNode(c)==&wrapper && child->getAllModules().size()==1);
    }
    // A boundary-build failure after the cut restores nodes, wires and history.
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Boundary rollback");
        auto& a=c.createNode(make_unique<Probe>(),1);auto& b=c.createNode(make_unique<Probe>(),2);
        b.getNodeGui().setPosition({0,0});
        b.getNodeModel().getInspectorParameter<float>("Marker")=29.f;
        assert(c.createConnection(pin(a,"Out"),pin(b,"In")));
        c.beginCanvasEdit();b.getNodeGui().setPosition({90,40});c.endCanvasEdit();const auto before=c.captureCanvasGraph();
        auto& factories=const_cast<ofxOceanodeNodeRegistry::registeredModelsMap&>(registry->getRegisteredModels());
        const auto saved=factories.at("Router f");factories["Router f"]=[](){return std::make_unique<Probe>();};
        assert(!c.encapsulateNodes({&b}));factories["Router f"]=saved;
        assert(c.captureCanvasGraph()==before && marker(*probe(c,2))==29.f);
        assert(c.getAllConnections().size()==1 && c.undoCanvasEdit());
        assert(probe(c,2)->getNodeGui().getPosition()==glm::vec2(0));
    }
    // Extracting into another macro must not overwrite its existing exposed pin.
    {
        ofxOceanodeContainer root(registry,types,transport);root.setCanvasID("Router collisions");
        auto& outerNode=root.createNode(make_unique<ofxOceanodeNodeMacro>(),1);
        auto parent=static_cast<ofxOceanodeNodeMacro&>(outerNode.getNodeModel()).getContainer();
        auto& existing=parent->createNode(make_unique<router<float>>("f",0.f,-100.f,100.f),1);
        static_cast<abstractRouter&>(existing.getNodeModel()).getNameParam()="In";
        auto& nestedNode=parent->createNode(make_unique<ofxOceanodeNodeMacro>(),1);
        auto nested=static_cast<ofxOceanodeNodeMacro&>(nestedNode.getNodeModel()).getContainer();
        auto& route=nested->createNode(make_unique<router<float>>("f",0.f,-100.f,100.f),1);
        static_cast<abstractRouter&>(route.getNodeModel()).getNameParam()="In";
        auto& sink=nested->createNode(make_unique<Probe>(),1);
        assert(nested->createConnection(pin(route,"Value"),pin(sink,"In")));
        assert(parent->createConnection(pin(existing,"Value"),pin(nestedNode,"In")));
        const auto before=parent->captureCanvasGraph();
        assert(parent->extractMacro(nestedNode));
        assert(outerNode.getParameters().contains("In") && outerNode.getParameters().contains("_In"));
        existing.getNodeModel().getParameter<float>("Value")=.61f;
        assert(probe(*parent,1)->getNodeModel().getParameter<float>("In")==.61f);
        assert(root.undoCanvasEdit());assert(parent->captureCanvasGraph()==before);
        assert(outerNode.getParameters().contains("In") && !outerNode.getParameters().contains("_In"));
        assert(root.redoCanvasEdit());assert(outerNode.getParameters().contains("_In"));
        existing.getNodeModel().getParameter<float>("Value")=.62f;
        assert(probe(*parent,1)->getNodeModel().getParameter<float>("In")==.62f);
        assert(root.undoCanvasEdit());assert(parent->captureCanvasGraph()==before);
    }
    // Moving a library macro carries its live edits, without saving the library.
    {
        {ofxOceanodeContainer source(registry,types,transport);source.setCanvasID("Library");
            source.createNode(make_unique<Probe>(),1).getNodeModel().getInspectorParameter<float>("Marker")=9.f;
            assert(source.savePreset("Macros/Globals/G"));}
        ofxOceanodeShared::updateMacrosStructure();
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Library move");
        auto& original=c.createNode(make_unique<ofxOceanodeNodeMacro>(),5,false,"Macros/Globals/G");
        auto& global=static_cast<ofxOceanodeNodeMacro&>(original.getNodeModel());assert(!global.isLocal());
        probe(*global.getContainer(),1)->getNodeModel().getInspectorParameter<float>("Marker")=64.f;
        auto* wrapper=c.encapsulateNodes({&original},"Library wrapper");assert(wrapper);
        auto container=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer();
        auto& moved=static_cast<ofxOceanodeNodeMacro&>(container->getAllModules().front()->getNodeModel());
        assert(!moved.isLocal() && moved.getCurrentMacroName()=="G" && marker(*probe(*moved.getContainer(),1))==64.f);
        probe(*moved.getContainer(),1)->getNodeModel().getInspectorParameter<float>("Marker")=65.f;
        assert(c.undoCanvasEdit());
        auto& restored=static_cast<ofxOceanodeNodeMacro&>(macroNode(c)->getNodeModel());
        assert(!restored.isLocal() && marker(*probe(*restored.getContainer(),1))==65.f);
        assert(c.redoCanvasEdit());
        ofxOceanodeContainer unchanged(registry,types,transport);unchanged.setCanvasID("Library check");
        assert(unchanged.loadPreset("Macros/Globals/G") && marker(*probe(unchanged,1))==9.f);
    }
    // Nested macros sharing parameter names retain their distinct definitions.
    {
        ofxOceanodeContainer c(registry,types,transport);c.setCanvasID("Nested");
        for(int id=1;id<=2;++id){auto& n=c.createNode(make_unique<ofxOceanodeNodeMacro>(),id);
            auto child=static_cast<ofxOceanodeNodeMacro&>(n.getNodeModel()).getContainer();
            child->createNode(make_unique<Probe>(),1).getNodeModel().getInspectorParameter<float>("Marker")=float(id*10);
        }
        const auto original=c.getAllModules();auto* wrapper=c.encapsulateNodes(original,"Nested pair");assert(wrapper);
        auto child=static_cast<ofxOceanodeNodeMacro&>(wrapper->getNodeModel()).getContainer();
        auto inner1=static_cast<ofxOceanodeNodeMacro&>(child->getDynamicNodes().at("Macro").at(1)->getNodeModel()).getContainer();
        auto inner2=static_cast<ofxOceanodeNodeMacro&>(child->getDynamicNodes().at("Macro").at(2)->getNodeModel()).getContainer();
        assert(marker(*probe(*inner1,1))==10.f && marker(*probe(*inner2,1))==20.f);
        // Force a Macro ID collision and retarget folder-based definitions.
        c.beginCanvasEdit();c.createNode(make_unique<ofxOceanodeNodeMacro>(),2);c.endCanvasEdit();
        assert(c.extractMacro(*wrapper));assert(c.getDynamicNodes().at("Macro").size()==3);
        assert(c.undoCanvasEdit());assert(c.redoCanvasEdit());
        bool ten=false,twenty=false;
        for(auto* n:c.getAllModules())if(auto* m=dynamic_cast<ofxOceanodeNodeMacro*>(&n->getNodeModel())){
            if(m->getContainer()->getAllModules().empty())continue;
            const auto v=marker(*probe(*m->getContainer(),1));ten|=v==10.f;twenty|=v==20.f;
        }
        assert(ten && twenty);
    }
    std::cout << "Macro undo regression passed: encapsulation/extraction, nested collisions, boundaries, GUI/snapshot migration, shared ordering and excluded parameter edits.\n";
}
'''


def main():
    debug = os.environ.get("OCEANODE_TEST_BUILD_DIR")
    context = contextlib.nullcontext(debug) if debug else tempfile.TemporaryDirectory(prefix="macro-undo-access-")
    with context as folder:
        access = Path(folder) / "ContainerUndoTestAccess.h"
        access.parent.mkdir(parents=True, exist_ok=True)
        access.write_text((native.ROOT / "src/Managers/ofxOceanodeContainer.h").read_text().replace("private:", "public:"))
        native.PRELUDE = '#include "' + str(access) + '"\n' + native.PRELUDE
        extra_sources, extra_includes = [], []
        cases = CASES
        if "--with-custom-gui" in __import__("sys").argv:
            from custom_gui_test_support import enable_custom_gui
            extra_sources, extra_includes = enable_custom_gui(native, graph_helpers=False)
            cases = cases.replace('c.customGuiPanelsData', 'ofxOceanodeCustomGui::get(c).getCustomGuiPanelsData()')
            cases = cases.replace('child->customGuiPanelsData', 'ofxOceanodeCustomGui::get(*child).getCustomGuiPanelsData()')
            cases = cases.replace('c.customGuiSnapshotBanks', 'customGuiSnapshotBanksFromJson(ofxOceanodeCustomGui::get(c).snapshotsForReview())')
            start = cases.index('    ofxOceanodeCustomGui::get(c).getCustomGuiPanelsData()={a,b};')
            end = cases.index('\n}\nint main', start)
            cases = cases[:start] + r'''
    ofxOceanodeCustomGui::attach(c);
    std::vector<CustomGuiPanelData> panels{a,b};
    std::vector<CustomGuiSnapshotBank> banks;
    for(const auto& p:panels){
        CustomGuiSnapshotBank bank;bank.customGuiId=p.id;bank.customGuiName=p.name;
        CustomGuiSnapshotData snapshot;snapshot.id="s";snapshot.name="Saved";
        for(const auto& w:p.layout.widgets)snapshot.parameterValues[w.parameterRef.parameterPath]={"float",.15f};
        bank.snapshots.push_back(snapshot);banks.push_back(bank);
    }
    auto& gui = ofxOceanodeCustomGui::get(c);
    gui.restoreState({{"panels",customGuiPanelsToJson(panels)},{"snapshots",customGuiSnapshotBanksToJson(banks)}},gui.captureState());
''' + cases[end:]
        else:
            for signature in ['static void verifyPublications(', 'static void addPanels(']:
                body = block(cases, signature)
                cases = cases.replace(body, body[:body.index('{')+1] + '}')
            cases = cases.replace('GUI/snapshot migration', 'optional GUI absent')
            cases = '\n'.join(line for line in cases.split('\n') if 'customGuiPanelsData.size()' not in line and 'static CustomGuiWidget widget(' not in line)
        base_production = native.native_production

        def production():
            result = base_production()
            source = (native.ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
            # Encapsulation and extraction hand moved timeline paths over: use
            # the real path functions instead of the fixture's stub.
            result = result.replace('std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter&) const { assert(false); return {}; }', '')
            start = source.index('namespace {\nconst char* const kTimelineCanvasSeparator')
            result += source[start:source.index('std::string ofxOceanodeContainer::getTimelineParameterPath', start)]
            for signature in ['std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter) const',
                              'void ofxOceanodeContainer::collectMovedTimelinePaths(',
                              'std::string ofxOceanodeContainer::timelineCanvasPrefix(',
                              'std::string ofxOceanodeContainer::timelineMacroCanvas(',
                              'std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter,\n',
                              'ofxOceanodeAbstractParameter* ofxOceanodeContainer::findTimelineParameter(',
                              'ofxOceanodeAbstractParameter* ofxOceanodeContainer::findParameter(',
                              'ofxOceanodeContainer::ParsedParameterPath ofxOceanodeContainer::parseParameterPath(',
                              'ofxOceanodeContainer::ResolvedParameter ofxOceanodeContainer::resolveParameterFromPath(',
                              'ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(',
                              'const ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(']:
                if signature in source and block(source, signature) not in result:
                    result += block(source, signature) + '\n'
            for signature in [
                'void ofxOceanodeContainer::encapsulateSelectedNodes(',
                'ofxOceanodeNode* ofxOceanodeContainer::encapsulateNodes(',
                'vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(',
                'void ofxOceanodeContainer::createRoutersAndReconnect(',
                'bool ofxOceanodeContainer::isNodeInList(',
                'string ofxOceanodeContainer::generateRouterName(',
                'ofxOceanodeNode* ofxOceanodeContainer::getNodeFromParameter(',
                'ofxOceanodeAbstractParameter* ofxOceanodeContainer::findInternalParameter(',
                'string ofxOceanodeContainer::extractNodeType(',
                'void ofxOceanodeContainer::ensureMacroParametersExposed(',
                'bool ofxOceanodeContainer::copySelectedModulesWithConnections(',
                'bool ofxOceanodeContainer::cutSelectedModulesWithConnections(',
                'bool ofxOceanodeContainer::pasteModulesAndConnectionsInPosition(',
                'vector<int> ofxOceanodeContainer::getSelectedCommentIndices()']:
                result += block(source, signature) + '\n'
            return result

        native.native_production = production
        native.CASES = cases
        native.main(extra_sources, extra_includes)


if __name__ == "__main__":
    main()
