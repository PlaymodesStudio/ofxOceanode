#!/usr/bin/env python3
"""Native preset round trips for Display Output visibility, without OS windows.

Uses the real external-window base, Display Output and node serialization.
Only the base's OS-window event listeners are removed to avoid creating a
graphics window; parameter events and serialization remain active.
"""
import macro_mixer_regression as native

native.CASES = r'''
#include "displayOutput.h"
class QuietDisplay : public displayOutput {
public:
    QuietDisplay(){windowListenerEvents.unsubscribeAll();}
    void lateClose(){ofEventArgs args;closeExternalWindow(args);}
};
int main(int argc,char** argv){
    assert(argc==3);std::filesystem::current_path(argv[2]);ofDisableDataPath();ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>();ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>();
    auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<QuietDisplay>("Test");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer root(registry,types,transport);root.setCanvasID("DisplayTest");
    auto& first=root.createNode(std::make_unique<QuietDisplay>(),1);
    static_cast<QuietDisplay&>(first.getNodeModel()).lateClose();
    auto visible=[&]() -> ofParameter<bool>& {
        return root.getDynamicNodes().at("Display Output").at(1)->getNodeModel().getParameter<bool>("Show.Win");
    };
    visible()=false;assert(root.savePreset("Hidden"));
    visible()=true;assert(root.savePreset("Shown"));
    for(int repeat=0;repeat<3;++repeat){
        assert(root.loadPreset("Hidden"));assert(!visible().get());
        assert(root.loadPreset("Shown"));assert(visible().get());
    }
    assert(root.loadPreset("Hidden"));assert(root.savePreset("Copy"));
    visible()=true;assert(root.loadPreset("Copy"));assert(!visible().get());
    // Old Display Output presets still open by default; saving migrates them.
    auto legacy=ofLoadJson("Hidden/Display_Output_1.json");
    legacy.erase(visible().getEscapedName());
    assert(oceanodePresetSave::saveJson("Hidden/Display_Output_1.json",legacy));
    assert(root.loadPreset("Hidden"));assert(visible().get());
    visible()=false;assert(root.savePreset("Hidden"));
    assert(root.loadPreset("Hidden"));assert(!visible().get());
    std::cout<<"External window visibility passed: hidden/shown presets, repeated switching, Save As, legacy defaults and migration.\n";
}
'''

if __name__ == "__main__":
    native.main(["src/Nodes/Base_Nodes/ofxOceanodeNodeModelExternalWindow.cpp"],
                [native.ROOT.parent / "ofxOceanodeTextures/src"])
