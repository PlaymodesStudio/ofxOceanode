#!/usr/bin/env python3
"""Production router/macro graphs, metadata changes, overrides and persistence.

Uses the headless native Macro fixture with ASan/UBSan and real OF parameters.
"""
import macro_mixer_regression as fixture

fixture.CASES = r'''
class DropdownProbe : public ofxOceanodeNodeModel {
public:
    DropdownProbe():ofxOceanodeNodeModel("Dropdown Probe") {}
    ofParameter<int> mode;
    ofParameter<vector<string>> options;
    void setup() override {
        addParameterDropdown(mode,"Mode",0,{"A","B","C"});
        addOutputParameter(options.set("Options",{"Manual A","Manual B"}));
    }
};
static routerDropdown& routerAt(ofxOceanodeContainer& c,int id=1){
    return static_cast<routerDropdown&>(c.createNode(std::make_unique<routerDropdown>(),id).getNodeModel());
}
static DropdownProbe& probeAt(ofxOceanodeContainer& c,int id=1){
    return static_cast<DropdownProbe&>(c.createNode(std::make_unique<DropdownProbe>(),id).getNodeModel());
}
static auto& pin(ofxOceanodeNodeModel& m,const string& name){
    return dynamic_cast<ofxOceanodeAbstractParameter&>(m.getParameterGroup().get(name));
}
static vector<string> labels(ofxOceanodeNodeModel& m,const string& name){return pin(m,name).getDropdownOptions();}
int main(int argc,char** argv){
    assert(argc==3); std::filesystem::current_path(argv[2]); ofDisableDataPath();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>();
    auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    types->registerType<int>("i"); ofxOceanodeTypesRegistry::getInstance()->registerType<int>("i");
    types->registerType<vector<string>>("v_s"); ofxOceanodeTypesRegistry::getInstance()->registerType<vector<string>>("v_s");
    registry->registerModel<routerDropdown>("Router");
    registry->registerModel<DropdownProbe>("Test");
    registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("DropdownTest");
    auto& r=routerAt(c); auto& p=probeAt(c);
    // Saved values survive the period before cables and metadata are restored.
    r.getValue()=2; r.refreshOptions(); assert(r.getValue()==2);
    assert(r.getCurrentOptions().empty());
    auto* cable=c.createConnection(pin(r,"Value"),pin(p,"Mode")); assert(cable);
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"A","B","C"}));
    assert(r.getValue().getMax()==2 && r.getValue()==2);
    // Metadata changes without any change to the selected integer.
    p.getOceanodeParameter(p.mode).setDropdownOptions({"One","Two"});
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"One","Two"}));
    assert(r.getValue()==0 && r.getValue().getMax()==1);
    // Multiple identical lists work; differing lists require a reference.
    auto& other=probeAt(c,2);
    other.getOceanodeParameter(other.mode).setDropdownOptions({"One","Two"});
    auto* branch=c.createConnection(pin(r,"Value"),pin(other,"Mode")); assert(branch);
    r.refreshOptions(); assert(r.getCurrentOptions().size()==2);
    other.getOceanodeParameter(other.mode).setDropdownOptions({"X","Y","Z","W"});
    r.refreshOptions(); assert(r.getCurrentOptions().empty());
    assert(!r.getNodeTitleSuffix().empty());
    r.getInspectorParameterGroup().get("Options Reference").cast<string>()=
        other.getParameterGroup().getName()+"/Mode";
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"X","Y","Z","W"}));
    const string chosenSave=string(argv[2])+"/chosen";
    c.savePreset(chosenSave);
    {
        ofxOceanodeContainer restored(registry,types,transport); restored.setCanvasID("Different Parent");
        assert(restored.loadPreset(chosenSave));
        auto& loaded=static_cast<routerDropdown&>(restored.getDynamicNodes().at("Router Dropdown").at(1)->getNodeModel());
        auto& firstProbe=restored.getDynamicNodes().at("Dropdown Probe").at(1)->getNodeModel();
        auto& secondProbe=restored.getDynamicNodes().at("Dropdown Probe").at(2)->getNodeModel();
        pin(firstProbe,"Mode").cast<int>().setDropdownOptions({"One","Two"});
        pin(secondProbe,"Mode").cast<int>().setDropdownOptions({"X","Y","Z","W"});
        loaded.refreshOptions(); assert(loaded.getCurrentOptions()==vector<string>({"X","Y","Z","W"}));
    }
    branch->deleteSelf();
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"One","Two"}));
    // Existing explicit Options cables remain authoritative, including empty lists.
    auto* overrideCable=c.createConnection(pin(p,"Options"),pin(r,"Options")); assert(overrideCable);
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"Manual A","Manual B"}));
    p.options=vector<string>{}; r.refreshOptions(); assert(r.getCurrentOptions().empty());
    overrideCable->deleteSelf(); r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"One","Two"}));
    // Chains, cycles and incoming dropdown sources.
    auto& chained=routerAt(c,2);
    auto* chain=c.createConnection(pin(chained,"Value"),pin(r,"Value")); assert(chain);
    chained.refreshOptions(); assert(chained.getCurrentOptions()==r.getCurrentOptions());
    auto* cycle=c.createConnection(pin(r,"Value"),pin(chained,"Value")); assert(cycle);
    chained.refreshOptions(); assert(chained.getCurrentOptions()==r.getCurrentOptions());
    cycle->deleteSelf(); chain->deleteSelf(); cable->deleteSelf();
    r.refreshOptions(); assert(r.getCurrentOptions().empty());
    auto* incoming=c.createConnection(pin(p,"Mode"),pin(r,"Value")); assert(incoming);
    r.refreshOptions(); assert(r.getCurrentOptions()==vector<string>({"One","Two"}));
    incoming->deleteSelf();
    // A processing port without dropdown metadata is a boundary.
    ofParameter<int> plain; plain.set("Plain",0);
    auto ordinary=p.addParameter(plain);
    auto* numeric=c.createConnection(pin(r,"Value"),*ordinary); assert(numeric);
    r.refreshOptions(); assert(r.getCurrentOptions().empty()); numeric->deleteSelf();
    // Nested macro pins relay metadata in either direction across canvas boundaries.
    auto& macro=static_cast<ofxOceanodeNodeMacro&>(c.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1).getNodeModel());
    auto& inner=routerAt(*macro.getContainer()); inner.getNameParam()="Mode";
    auto& nested=static_cast<ofxOceanodeNodeMacro&>(macro.getContainer()->createNode(std::make_unique<ofxOceanodeNodeMacro>(),1).getNodeModel());
    auto& deepest=routerAt(*nested.getContainer()); deepest.getNameParam()="Mode";
    auto& target=probeAt(*nested.getContainer());
    assert(nested.getContainer()->createConnection(pin(deepest,"Value"),pin(target,"Mode")));
    assert(macro.getContainer()->createConnection(pin(inner,"Value"),pin(nested,"Mode")));
    assert(c.createConnection(pin(r,"Value"),pin(macro,"Mode")));
    deepest.refreshOptions(); inner.refreshOptions(); r.refreshOptions();
    assert(r.getCurrentOptions()==vector<string>({"A","B","C"}));
    assert(labels(macro,"Mode")==r.getCurrentOptions());
    assert(labels(nested,"Mode")==r.getCurrentOptions());
    target.getOceanodeParameter(target.mode).setDropdownOptions({"Dynamic A","Dynamic B"});
    deepest.refreshOptions(); inner.refreshOptions(); r.refreshOptions();
    assert(labels(macro,"Mode")==vector<string>({"Dynamic A","Dynamic B"}));
    assert(pin(macro,"Mode").cast<int>().getParameter().getMax()==1);
    // Save/load discovers labels after connection restoration; no Options cable.
    const string saved=string(argv[2])+"/saved";
    c.savePreset(saved);
    {
        ofxOceanodeContainer restored(registry,types,transport); restored.setCanvasID("Restored");
        assert(restored.loadPreset(saved)); restored.update(); restored.update();
        auto& loaded=static_cast<routerDropdown&>(restored.getDynamicNodes().at("Router Dropdown").at(1)->getNodeModel());
        assert(loaded.getCurrentOptions()==vector<string>({"A","B","C"}));
    }
    // Destroying all peers and cables must leave no dangling metadata links.
    c.clearContainer();
}
'''

if __name__ == '__main__':
    fixture.main()
