#!/usr/bin/env python3
"""Real Fat Line geometry opacity mixing, custom typed ports, reload, cables and Macro copies."""
import macro_mixer_regression as mixer
mixer.PRELUDE += '\n#include "MacroMixerFatline.h"\n#include "fatlineGenerator.h"\n'

mixer.CASES=r'''
using Lines=std::vector<ofxFatLine>;
static void near(float a,float b) { if(!std::isfinite(a)||std::abs(a-b)>.0001f) {std::cerr<<a<<" != "<<b<<"\n"; std::abort();} }
static Lines lines(const MacroMixerRuntime& r) { return MacroMixerMixing::object<Lines>(r.outputValue("lines")); }
static void sameGeometry(ofxFatLine a,ofxFatLine b) {
    assert(a.getVertices()==b.getVertices() && a.isClosed()==b.isClosed());
    assert(a.getMesh().getVertices()==b.getMesh().getVertices());
    for(size_t i=0;i<a.size();++i) near(a.getWeight(i),b.getWeight(i));
}
static void opacity(ofxFatLine output,ofxFatLine input,float gain) {
    sameGeometry(output,input); const auto a=output.getColors(),b=input.getColors(); assert(a.size()==b.size());
    for(size_t i=0;i<a.size();++i) for(int c=0;c<4;++c) near(a[i][c],b[i][c]*gain);
    const auto& am=output.getMesh().getColors(); const auto& bm=input.getMesh().getColors(); assert(am.size()==bm.size());
    for(size_t i=0;i<am.size();++i) for(int c=0;c<4;++c) near(am[i][c],bm[i][c]*gain);
}
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem; ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto definition=ofxOceanodeVectorGraphics::fatlineMixingDefinition(); auto& mixing=MacroMixerMixing::registry();
    assert(mixing.registerValueType<Lines>(definition.type,"Router v_Fatline",definition.validate));
    mixing.registerValuePolicy<Lines>(definition.type,definition.policy,definition.label,definition.mix,definition.options,true);
    assert(definition.allowSwitch); mixing.registerSwitchPolicy(definition.type);
    assert(mixing.policy(definition.type).id=="opacity");
    assert(mixing.policy(definition.type,"switch").label=="Switch");
    assert(mixing.policy(definition.type,"switch").options(ofJson::object()).at("threshold")==.5);
    assert(!mixing.registerValueType<Lines>(definition.type,"Router v_Fatline",definition.validate));
    types->registerType<Lines>("v_Fatline"); types->registerType<ofxFatLine>("Fatline");
    types->registerType<float>("f"); ofxOceanodeTypesRegistry::getInstance()->registerType<Lines>("v_Fatline");
    ofxOceanodeTypesRegistry::getInstance()->registerType<ofxFatLine>("Fatline"); ofxOceanodeTypesRegistry::getInstance()->registerType<float>("f");
    registry->registerModel<router<Lines>>("Router","v_Fatline",Lines{});
    registry->registerModel<router<ofxFatLine>>("Router","Fatline",ofxFatLine{});
    registry->registerModel<fatlineGenerator>("Vector Graphics"); registry->registerModel<ofxOceanodeMacroMixer>("Container");
    registry->registerModel<ofxOceanodeNodeMacro>("Container");
    auto transport=std::make_shared<ofxOceanodeTransport>(); transport->setDriverMode(TransportDriverMode::FrameStep);
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("TestParent");
    const std::string scratch=argv[2]; const auto cwd=fs::current_path(); fs::current_path(scratch);
    auto write=[](const std::string& folder,const std::string& file,const ofJson& j){fs::create_directories(folder);std::ofstream(folder+"/"+file)<<j.dump(2);};
    const auto a=scratch+"/a",b=scratch+"/b";
    for(const auto& folder:{a,b}) {
        write(folder,"modules.json",{{"Fat Line",{{"01",{0,0}}}},{"Router v_Fatline",{{"01",{0,0}}}}});
        write(folder,"Router_v_Fatline_1.json",{{"Name","Lines"}});
        write(folder,"Fat_Line_1.json",{{"X",folder==a?ofJson({.1,.3}):ofJson({.2,.4,-1,.6,.8})},
            {"Y",folder==a?ofJson({.2,.4}):ofJson({.5,.6,-1,.7,.8})},
            {"R",{.8}},{"G",{.4}},{"B",{.2}},{"A",{.6}},{"W",{1}},{"Opacity",folder==a?.5:.75}});
        write(folder,"connections.json",{{"Fat_Line_1",{{"Output",{{"Router_v_Fatline_1",{{"Value",nullptr}}}}},
            {"R",{{"Fat_Line_1",{{"G",nullptr}}}}},{"G",{{"Fat_Line_1",{{"B",nullptr}}}}}}}});
    }
    MacroMixerContract contract; contract.routers={{"lines","Lines","Fatline Vector"}};
    {
        MacroMixerRuntime runtime(parent); assert(runtime.setContract(contract)); assert(runtime.importMacro(a)&&runtime.importMacro(b));
        const auto entries=runtime.macros(); runtime.requestMacro(entries[0]->id(),0,0); runtime.update(0); auto originalA=lines(runtime); assert(originalA.size()==1);
        for(auto color:originalA[0].getColors()) { near(color.r,color.g); near(color.g,color.b); }
        runtime.requestMacro(entries[1]->id(),100,0); runtime.update(0);
        const auto sourceB=MacroMixerMixing::object<Lines>(runtime.destination()->read("lines")); assert(sourceB.size()==2);
        // Switch selects the entire original set, preserving geometry and opacity.
        const auto objectA=MacroMixerMixing::object("Fatline Vector",originalA),objectB=MacroMixerMixing::object("Fatline Vector",sourceB);
        for(double threshold:{0.,.25,.5,1.}) for(float progress:{0.f,.125f,.25f,.5f,.75f,1.f}) {
            const auto& expected=(progress==0 || (progress!=1 && progress<threshold)) ? originalA : sourceB;
            auto selected=MacroMixerMixing::object<Lines>(mixing.mix("Fatline Vector","switch",objectA,objectB,progress,{{"threshold",threshold}}));
            assert(selected.size()==expected.size()); for(size_t i=0;i<selected.size();++i) opacity(selected[i],expected[i],1);
        }
        for(const auto& invalid:std::vector<ofJson>{{{"threshold",-.1}},{{"threshold",1.1}},{{"threshold","half"}},{{"size",32}}}) {
            bool rejected=false; try { mixing.mix("Fatline Vector","switch",objectA,objectB,.5,invalid); } catch(const std::exception&) { rejected=true; }
            assert(rejected);
        }
        auto switchContract=contract; switchContract.routers[0].policy="switch"; switchContract.routers[0].options={{"threshold",.25}};
        MacroMixerRuntime switched(parent); assert(switched.setContract(switchContract)); assert(switched.importMacro(a) && switched.importMacro(b));
        const auto switches=switched.macros(); switched.requestMacro(switches[0]->id(),0,0); switched.update(0);
        switched.requestMacro(switches[1]->id(),100,0); switched.update(0); switched.update(24); assert(lines(switched).size()==1);
        switched.update(25); auto selected=lines(switched); assert(selected.size()==2); opacity(selected[0],sourceB[0],1);
        auto switchSaved=switched.saveState(); MacroMixerRuntime switchCopy(parent); assert(switchCopy.restoreState(switchSaved)); switchCopy.update(0);
        assert(switchCopy.contract().routers[0].policy=="switch" && switchCopy.contract().routers[0].options.at("threshold")==.25);
        selected=lines(switchCopy); assert(selected.size()==2); opacity(selected[0],sourceB[0],1);
        auto v=lines(runtime); assert(v.size()==1); opacity(v[0],originalA[0],1);
        runtime.update(25); v=lines(runtime); assert(v.size()==3); opacity(v[0],originalA[0],.75); opacity(v[1],sourceB[0],.25); opacity(v[2],sourceB[1],.25);
        runtime.update(50); v=lines(runtime); for(size_t i=0;i<v.size();++i) opacity(v[i],i==0?originalA[0]:sourceB[i-1],.5);
        // Source colors and geometry remain unchanged across repeated fades.
        const auto untouched=MacroMixerMixing::object<Lines>(runtime.origin()->read("lines")); opacity(untouched[0],originalA[0],1);
        auto saved=runtime.saveState(); assert(!saved.contains("files")); MacroMixerRuntime restored(parent); assert(restored.restoreState(saved)); restored.update(0);
        auto rv=lines(restored); assert(rv.size()==2); opacity(rv[0],sourceB[0],1);
        runtime.finish(); runtime.update(100); v=lines(runtime); assert(v.size()==2); opacity(v[0],sourceB[0],1);
        runtime.requestMacro(entries[0]->id(),100,0); runtime.update(100); runtime.update(150); v=lines(runtime);
        assert(v.size()==3); opacity(v[0],sourceB[0],.5); opacity(v[2],originalA[0],.5);
        fs::rename(b,b+"-moved"); auto capture=runtime.macro(entries[1]->id()); assert(!runtime.importMacro(b,true) && runtime.macro(entries[1]->id())==capture);
        MacroMixerRuntime missing(parent); auto sourceMissing=saved; assert(missing.restoreState(sourceMissing)); missing.update(0); assert(!missing.origin()); fs::rename(b+"-moved",b);
        // Empty sets simply fade the other set, with exact empty endpoint.
        auto zero=MacroMixerMixing::object("Fatline Vector",Lines{}), full=MacroMixerMixing::object("Fatline Vector",originalA);
        auto blended=mixing.mix("Fatline Vector","opacity",zero,full,.5); auto half=MacroMixerMixing::object<Lines>(blended); assert(half.size()==1); opacity(half[0],originalA[0],.5);
        assert(MacroMixerMixing::object<Lines>(mixing.mix("Fatline Vector","opacity",zero,full,0)).empty());
        // Custom Input routers seed both graphs through ordinary typed cables.
        const auto feed=scratch+"/feed";
        write(feed,"modules.json",{{"Router v_Fatline",{{"01",{0,0}},{"02",{0,0}}}}});
        write(feed,"Router_v_Fatline_1.json",{{"Name","Feed"}}); write(feed,"Router_v_Fatline_2.json",{{"Name","Lines"}});
        write(feed,"connections.json",{{"Router_v_Fatline_1",{{"Value",{{"Router_v_Fatline_2",{{"Value",nullptr}}}}}}}});
        MacroMixerContract inputContract; inputContract.routers={{"feed","Feed","Fatline Vector","",ofJson::object(),"Input"},{"lines","Lines","Fatline Vector"}};
        MacroMixerRuntime inputRuntime(parent); assert(inputRuntime.setContract(inputContract)); assert(inputRuntime.setInputs({{"feed",full}}));
        assert(inputRuntime.importMacro(feed)); inputRuntime.requestMacro(inputRuntime.macros()[0]->id(),0,0); inputRuntime.update(0);
        opacity(lines(inputRuntime)[0],originalA[0],1);
        assert(inputRuntime.setInputs({{"feed",blended}})); inputRuntime.update(1); opacity(lines(inputRuntime)[0],originalA[0],.5);
        auto inputSaved=inputRuntime.saveState(); MacroMixerRuntime inputCopy(parent); assert(inputCopy.restoreState(inputSaved)); inputCopy.update(0);
        assert(lines(inputCopy).empty()); // Opaque shape inputs await fresh upstream data after reload.
        assert(inputCopy.setInputs({{"feed",full}})); inputCopy.update(1); opacity(lines(inputCopy)[0],originalA[0],1);
    }
    {
        auto& n=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); auto& mix=static_cast<ofxOceanodeMacroMixer&>(n.getNodeModel()); assert(mix.setRouterContract(contract));
        auto& sink=parent.createNode(registry->create("Router v_Fatline"),9);
        assert(parent.createConnectionFromInfo("MacroMixer_1","Lines","Router_v_Fatline_9","Value")); assert(mix.importMacroFolder(a));
        mix.getParameter<float>("Duration Ms")=0; mix.getParameter<void>("Transition").trigger(); parent.update();
        auto baseline=sink.getNodeModel().getParameter<Lines>("Value").get(); assert(baseline.size()==1);
        const auto folder=scratch+"/saved"; n.savePreset(folder);
        parent.saveClipboardModulesAndConnections({&n,&sink},{0,0}); assert(parent.loadClipboardModulesAndConnections({40,40},false)); parent.update();
        auto& copy=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(2)->getNodeModel());
        assert(copy.mixerRuntime()->origin()->container!=mix.mixerRuntime()->origin()->container);
        copy.deactivate(); assert(mix.getParameter<Lines>("Lines").get().size()==1); parent.clearContainer();
        auto& loaded=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); loaded.loadPresetBeforeConnections(folder);
        auto& sink2=parent.createNode(registry->create("Router v_Fatline"),9); assert(parent.createConnectionFromInfo("MacroMixer_1","Lines","Router_v_Fatline_9","Value"));
        assert(loaded.loadPreset(folder)); parent.update(); opacity(sink2.getNodeModel().getParameter<Lines>("Value").get()[0],baseline[0],1); parent.clearContainer();
        auto& fedNode=parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); auto& fed=static_cast<ofxOceanodeMacroMixer&>(fedNode.getNodeModel());
        MacroMixerContract inputs; inputs.routers={{"feed","Feed","Fatline Vector","",ofJson::object(),"Input"},{"lines","Lines","Fatline Vector"}};
        assert(fed.setRouterContract(inputs)); auto& scalar=parent.createNode(registry->create("Router Fatline"),8).getNodeModel();
        scalar.getParameter<ofxFatLine>("Value")=baseline[0];
        assert(parent.createConnectionFromInfo("Router_Fatline_8","Value","MacroMixer_1","Feed"));
        assert(fed.importMacroFolder(scratch+"/feed")); fed.getParameter<float>("Duration Ms")=0;
        fed.getParameter<void>("Transition").trigger(); parent.update(); assert(fed.getParameter<Lines>("Lines").get().size()==1);
        opacity(fed.getParameter<Lines>("Lines").get()[0],baseline[0],1); parent.clearContainer();
    }
    {
        auto& outer=parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1); auto& macro=static_cast<ofxOceanodeNodeMacro&>(outer.getNodeModel()); auto child=macro.getContainer();
        auto& n=child->createNode(std::make_unique<ofxOceanodeMacroMixer>(),1); auto& mix=static_cast<ofxOceanodeMacroMixer&>(n.getNodeModel()); assert(mix.setRouterContract(contract));
        auto& route=child->createNode(registry->create("Router v_Fatline"),1).getNodeModel(); route.getParameter<std::string>("Name")="MixedLines";
        assert(child->createConnectionFromInfo("MacroMixer_1","Lines","Router_v_Fatline_1","Value")); assert(mix.importMacroFolder(a));
        mix.getParameter<float>("Duration Ms")=0; mix.getParameter<void>("Transition").trigger(); parent.update(); assert(macro.getParameter<Lines>("MixedLines").get().size()==1);
        const auto folder=scratch+"/macro"; parent.savePreset(folder); child.reset(); parent.clearContainer(); assert(parent.loadPreset(folder)); parent.update();
        auto* original=parent.getDynamicNodes().at("Macro").at(1).get(); parent.saveClipboardModulesAndConnections({original},{0,0}); assert(parent.loadClipboardModulesAndConnections({40,40},false)); parent.update();
        auto& copy=static_cast<ofxOceanodeNodeMacro&>(parent.getDynamicNodes().at("Macro").at(2)->getNodeModel());
        assert(copy.getParameter<Lines>("MixedLines").get().size()==1); parent.clearContainer();
    }
    assert(liveGui==0); fs::current_path(cwd); std::cout<<"Fatline mixer passed: opacity fades, whole-set Switch thresholds/endpoints/options/persistence, unequal shape counts, unchanged vertices/mesh/widths/source colors, empty sets, typed cables, source refs, clipboard and real Macro reload/copies.\n";
}
'''
if __name__=='__main__':
    mixer.main(extra_includes=[mixer.ROOT.parent/'ofxOceanodeVectorGraphics/src',mixer.ROOT.parent/'ofxFatLines/src'],
        extra_sources=[str(mixer.ROOT.parent/'ofxOceanodeVectorGraphics/src/MacroMixerFatline.cpp'),
            str(mixer.ROOT.parent/'ofxFatLines/src/ofxFatLine.cpp'),str(mixer.ROOT.parent/'ofxFatLines/src/sincosineLut.cpp'),
            str(mixer.of_root()/'libs/openFrameworks/3d/of3dPrimitives.cpp')])
