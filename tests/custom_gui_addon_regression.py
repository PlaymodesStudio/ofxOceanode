#!/usr/bin/env python3
"""Native optional-addon coverage. Run once normally and once with --with-addon.

The first build has no Custom GUI headers or objects. The second links the real
addon and checks panels, snapshots, vector shapes, macros, rendering and reload.
Both use real OF/ImGui with ASan/UBSan; no graphics window/backend is launched.
"""
import sys
import macro_mixer_regression as native

COMMON = r'''
class GuiProbe : public ofxOceanodeNodeModel {
public:
    GuiProbe():ofxOceanodeNodeModel("Gui Probe"){}
    void setup() override {
        addParameter(value.set("Value",.2f,0.f,1.f));
        addParameter(values.set("Values",{.1f,.2f},{0.f},{1.f}));
    }
    ofParameter<float> value;
    ofParameter<vector<float>> values;
};
static void writeJson(const string& path,const ofJson& data){std::ofstream(path)<<data;}
static string bytes(const string& path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
static auto& parameter(ofxOceanodeNode& node,const string& name){return static_cast<ofxOceanodeAbstractParameter&>(node.getParameters().get(name));}
int main(int argc,char** argv){
    assert(argc==3);namespace fs=std::filesystem;fs::current_path(argv[2]);ofDisableDataPath();ofSeedRandom();
    auto window=make_shared<ofAppNoWindow>();ofGetMainLoop()->setCurrentWindow(window);
    auto registry=make_shared<ofxOceanodeNodeRegistry>();auto types=make_shared<ofxOceanodeTypesRegistry>();
    types->registerType<float>("f");ofxOceanodeTypesRegistry::getInstance()->registerType<float>("f");
    types->registerType<vector<float>>("v_f");ofxOceanodeTypesRegistry::getInstance()->registerType<vector<float>>("v_f");
    registry->registerModel<GuiProbe>("Test");registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport=make_shared<ofxOceanodeTransport>();
'''

CORE = r'''
    ofxOceanodeContainer source(registry,types,transport);source.setCanvasID("Offline");
    auto& node=source.createNode(make_unique<GuiProbe>(),1);
    node.getNodeModel().getParameter<float>("Value")=.7f;
    assert(source.savePreset("Original"));
    auto modules=ofLoadJson("Original/modules.json");
    modules["CustomGui Snapshots"]["01"]={100,200};writeJson("Original/modules.json",modules);
    const ofJson cables={{"Gui_Probe_1",{{"Value",{{"CustomGui_Snapshots_1",{{"Snapshot",nullptr}}}}}}}};
    writeJson("Original/connections.json",cables);
    const string panelData="{\n  \"panels\": [{\"id\": \"keep\", \"unknown_future_field\": 42}]\n}\n";
    const string snapshotData="{\"banks\":[{\"customGuiId\":\"keep\",\"snapshots\":[{\"value\":123}]}]}\n";
    const string nodeData="{\"SelectedPanelId\":\"keep\",\"SelectedSnapshotSlot\":3}\n";
    std::ofstream("Original/custom_guis.json")<<panelData;
    std::ofstream("Original/custom_gui_snapshots.json")<<snapshotData;
    std::ofstream("Original/CustomGui_Snapshots_1.json")<<nodeData;
    ofxOceanodeContainer offline(registry,types,transport);offline.setCanvasID("Offline");
    assert(!offline.getCustomGuiExtension() && !registry->getRegisteredModels().count("CustomGui Snapshots"));
    assert(offline.loadPreset("Original"));
    assert(offline.getAllModules().size()==1 && offline.getAllConnections().empty());
    auto* loaded=offline.getAllModules().front();
    assert(loaded->getNodeModel().getParameter<float>("Value").get()==.7f);
    loaded->getNodeModel().getParameter<float>("Value")=.4f;
    assert(offline.savePreset("Original") && offline.savePreset("Original") && offline.savePreset("Copy"));
    for(const string path:{"Original","Copy"}){
        assert(bytes(path+"/custom_guis.json")==panelData);
        assert(bytes(path+"/custom_gui_snapshots.json")==snapshotData);
        assert(bytes(path+"/CustomGui_Snapshots_1.json")==nodeData);
        assert(ofLoadJson(path+"/modules.json").at("CustomGui Snapshots")==modules.at("CustomGui Snapshots"));
        assert(ofLoadJson(path+"/connections.json")==cables);
    }
    // Explicit exclusions take precedence over unavailable-addon preservation.
    registry->setNodeLoadFilter([](const string&,const string& key){return key=="CustomGui_Snapshots_1";});
    ofxOceanodeContainer filteredUnknown(registry,types,transport);filteredUnknown.setCanvasID("Offline");
    assert(filteredUnknown.loadPreset("Original") && filteredUnknown.savePreset("FilteredUnknown"));
    const auto filteredModules=ofLoadJson("FilteredUnknown/modules.json");
    assert(!filteredModules.contains("CustomGui Snapshots") || filteredModules.at("CustomGui Snapshots").empty());
    assert(ofLoadJson("FilteredUnknown/connections.json").empty());
    assert(!fs::exists("FilteredUnknown/CustomGui_Snapshots_1.json"));
    registry->setNodeLoadFilter([](const string&,const string& key){return key=="Gui_Probe_1";});
    ofxOceanodeContainer filteredSource(registry,types,transport);filteredSource.setCanvasID("Offline");
    assert(filteredSource.loadPreset("Original") && filteredSource.savePreset("FilteredSource"));
    assert(filteredSource.getAllModules().empty());
    assert(ofLoadJson("FilteredSource/modules.json").at("CustomGui Snapshots")==modules.at("CustomGui Snapshots"));
    assert(ofLoadJson("FilteredSource/connections.json").empty());
    registry->setNodeLoadFilter({});
    // A different preset cannot inherit the previous preset's opaque documents.
    ofxOceanodeContainer blank(registry,types,transport);blank.setCanvasID("Offline");assert(blank.savePreset("Blank"));
    assert(offline.loadPreset("Blank") && offline.savePreset("BlankCopy"));
    assert(!fs::exists("BlankCopy/custom_guis.json") && !fs::exists("BlankCopy/CustomGui_Snapshots_1.json"));
    std::cout<<"Core-only Custom GUI compatibility passed: graph execution, unavailable snapshot node/cables, byte-preserving save and Save As, preset isolation.\n";
}
'''

ADDON = r'''
    types->registerType<int>("i");ofxOceanodeTypesRegistry::getInstance()->registerType<int>("i");
    ofxOceanodeContainer root(registry,types,transport);root.setCanvasID("WithGui");
    auto& node=root.createNode(make_unique<GuiProbe>(),1);
    auto& macroNode=root.createNode(make_unique<ofxOceanodeNodeMacro>(),1);
    ofxOceanodeCustomGui::attach(root);
    auto* original=root.getCustomGuiExtension();ofxOceanodeCustomGui::attach(root);assert(root.getCustomGuiExtension()==original);
    assert(static_cast<ofxOceanodeNodeMacro&>(macroNode.getNodeModel()).getContainer()->getCustomGuiExtension());
    auto& laterMacro=root.createNode(make_unique<ofxOceanodeNodeMacro>(),2);
    assert(static_cast<ofxOceanodeNodeMacro&>(laterMacro.getNodeModel()).getContainer()->getCustomGuiExtension());
    auto& gui=ofxOceanodeCustomGui::get(root);
    const auto panelId=gui.createCustomGuiPanel("Performance").id;
    assert(gui.addParameterToCustomGui(panelId,parameter(node,"Value"),CustomGuiWidgetType::Slider));
    assert(gui.addParameterToCustomGui(panelId,parameter(node,"Values"),CustomGuiWidgetType::MultiSlider));
    assert(root.customGuiContainsParameterAnywhere(parameter(node,"Value")));
    gui.resizeCustomGuiParameterVector(parameter(node,"Values"),5);
    assert(node.getNodeModel().getParameter<vector<float>>("Values").get().size()==5);
    assert(!gui.storeCustomGuiSnapshotToSlot(panelId,3,"Scene").empty());
    node.getNodeModel().getParameter<float>("Value")=.9f;
    assert(gui.recallCustomGuiSnapshotSlot(panelId,3));
    assert(node.getNodeModel().getParameter<float>("Value").get()==.2f);
    root.createNodeFromName("CustomGui Snapshots",1);
    assert(root.savePreset("WithGui"));
    // Fresh registry: even a binary linked with the addon has no GUI until attached.
    auto bareRegistry=make_shared<ofxOceanodeNodeRegistry>();bareRegistry->registerModel<GuiProbe>("Test");bareRegistry->registerModel<ofxOceanodeNodeMacro>("Macro");
    ofxOceanodeContainer bare(bareRegistry,types,transport);bare.setCanvasID("Offline");
    assert(bare.loadPreset("WithGui") && !bare.getCustomGuiExtension());
    assert(bare.savePreset("WithoutGui"));
    ofxOceanodeContainer restored(registry,types,transport);restored.setCanvasID("Restored");
    assert(restored.loadPreset("WithoutGui"));
    auto& copy=ofxOceanodeCustomGui::get(restored);
    assert(copy.getCustomGuiPanelsData().size()==1 && copy.getCustomGuiPanelData(panelId));
    assert(copy.getCustomGuiSnapshotBySlot(panelId,3));
    assert(restored.getDynamicNodes().at("CustomGui Snapshots").size()==1);
    auto* restoredNode=restored.getDynamicNodes().at("Gui Probe").at(1).get();
    assert(restoredNode->getNodeModel().getParameter<vector<float>>("Values").get().size()==5);
    restoredNode->getNodeModel().getParameter<float>("Value")=.8f;
    assert(copy.recallCustomGuiSnapshotSlot(panelId,3));
    assert(restoredNode->getNodeModel().getParameter<float>("Value").get()==.2f);
    // Render the real editor and run controls in a headless ImGui context.
    auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1400,1000);
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    copy.openCustomGuiPanel(panelId,true);
    for(int frame=0;frame<3;++frame){io.DeltaTime=1.f/60;ImGui::NewFrame();copy.draw();ImGui::Render();}
    ofJson windows=ofJson::object();copy.windowState(windows,false);
    copy.getCustomGuiPanelData(panelId)->windowState.isOpen=false;copy.windowState(windows,true);
    assert(copy.getCustomGuiPanelData(panelId)->windowState.isOpen);
    copy.openCustomGuiPanel(panelId,false);
    for(int frame=0;frame<3;++frame){io.DeltaTime=1.f/60;ImGui::NewFrame();copy.draw();ImGui::Render();}
    ImGui::DestroyContext(context);
    assert(copy.removeParameterFromCustomGui(panelId,parameter(*restoredNode,"Values")));
    assert(restoredNode->getNodeModel().getParameter<vector<float>>("Values").get().size()==2);
    std::cout<<"Custom GUI addon passed: explicit/idempotent attachment, existing/new macros, panel bindings, vector shapes, snapshots, offline round trip, snapshot node recovery, editor/run rendering and visibility.\n";
}
'''

if __name__ == "__main__":
    extra_sources, extra_includes = [], []
    if "--with-addon" in sys.argv:
        from custom_gui_test_support import enable_custom_gui
        extra_sources, extra_includes = enable_custom_gui(native)
        native.CASES = COMMON + ADDON
    else:
        native.CASES = COMMON + CORE
    native.main(extra_sources, extra_includes)
