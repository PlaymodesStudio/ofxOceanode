#!/usr/bin/env python3
"""Native MacroMixer loading, selected routers, feedback, exclusions and persistence.

Builds production Macro/node/runtime code with ASan/UBSan and real OF libraries.
Canvas rendering, root scope and timeline services are headless adapters. Tests
create their Macro source folders using the production saver in a temp directory.
This does not verify the windowed host, audio devices or external hardware.
"""
from pathlib import Path
import concurrent.futures, os, subprocess, tempfile, contextlib, json
from of_environment import ROOT, of_root
from new_preset_regression import block

PRELUDE = r'''
#include "MacroMixerRuntime.h"
#include "PresetSaveCleanup.h"
#include "ofxOceanodeMacroMixer.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeShared.h"
#include "ofAppNoWindow.h"
#include "ofMainLoop.h"
#include "router.h"
#include "phasor.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "imgui.h"
int liveGui=0, destroyedGui=0, scopeCalls=0;
bool publishedInCustomGui=false;
bool ofxOceanodeContainer::customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter&) const { return publishedInCustomGui; }
// Services deliberately excluded from the fixture. Unexpected calls fail.
void ofxOceanodeScope::setScopeChangedCallback(ScopeChangedCallback) { ++scopeCalls; }
void ofxOceanodeScope::clearScopedParameters() { ++scopeCalls; }
bool ofxOceanodeScope::removeParameter(ofxOceanodeAbstractParameter*) { assert(false); return false; }
void ofxOceanodeContainer::flushPendingScopeSave() { assert(false); }
std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter&) const { assert(false); return {}; }
ofxOceanodeFrameTimeState ofxOceanodeNodeModel::getFrameGlobalTimeState() const { assert(false); return {}; }
ofxOceanodeTimeState ofxOceanodeNodeModel::getGlobalTimeState() const { assert(false); return {}; }
'''
SUPPORT = r'''
#include "ofxOceanodeNodeMacro.h"
// UI-only services are outside this headless test. No rendering is allowed.
void ofxOceanodeCanvas::draw(bool*,ofColor,string) { assert(false); }
void ofxOceanodeContainer::draw() { ofEventArgs args; for(auto* n:getAllModules()) if(n->getActive()) n->draw(args); }
void ofxOceanodeNodeMacro::renderPresetControlGui() { assert(false); }
bool ofxOceanodeNodeMacro::drawNodeParameter(ofxOceanodeAbstractParameter&, float, float) { assert(false); return false; }
void ofxOceanodeNodeMacro::renderPresetNamingGui() { assert(false); }
void ofxOceanodeNodeMacro::renderMinimizedView(ImVec2) { assert(false); }
void ofxOceanodeNodeMacro::renderInspectorInterface() { assert(false); }
void ofxOceanodeNodeMacro::renderRouterSortInterface() { assert(false); }
void ofxOceanodeContainer::loadScope(const std::string&) {}
void ofxOceanodeContainer::saveScope(const std::string&) {}
void ofxOceanodeContainer::scheduleScopeSave(const std::string&) { assert(false); }
'''

def history_adapters():
    c=(ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    return (block(c, 'ofxOceanodeContainer::CanvasNodeSnapshot::~CanvasNodeSnapshot()') + '\n' +
            block(c, 'void ofxOceanodeContainer::clearCanvasHistory()') + '\n' +
            'void ofxOceanodeContainer::captureDeletedCanvasNode(ofxOceanodeNode&) { assert(!pendingCanvasEdit); clearCanvasHistory(); }\n' +
            'void ofxOceanodeContainer::processCanvasHistoryRequest() { assert(!requestedCanvasHistory); }\n')


def production():
    c=(ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    guard=c[c.index('    thread_local unsigned int containerClearDepth'):c.index('    struct ConnectionFailureInfo')]
    result=PRELUDE+'\nnamespace {\n'+guard+'}\n'
    for sig in ['ofxOceanodeContainer::ofxOceanodeContainer(', 'ofxOceanodeContainer::~ofxOceanodeContainer()',
                'bool ofxOceanodeContainer::isClearingContainers()',
                'void ofxOceanodeContainer::clearContainer()', 'void ofxOceanodeContainer::update()',
                'ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr',
                'ofxOceanodeNode* ofxOceanodeContainer::createNodeFromName(',
                'ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(',
                'ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(',
                'bool ofxOceanodeContainer::loadClipboardModulesAndConnections(',
                'void ofxOceanodeContainer::saveClipboardModulesAndConnections(',
                'vector<ofxOceanodeNode*> ofxOceanodeContainer::getAllModules()',
                'ofxOceanodeTransportState ofxOceanodeContainer::getTransportState()',
                'ofxOceanodeFrameTransportState ofxOceanodeContainer::getFrameTransportState()']:
        result+=block(c,sig)+'\n'
    registry=(ROOT/'src/Managers/ofxOceanodeNodeRegistry.cpp').read_text()
    result+=block(registry,'ofxOceanodeNodeRegistry::ofxOceanodeNodeRegistry()')+'\n'
    result+=block(registry,'ofxOceanodeNodeRegistry::registeredModelsMap const &ofxOceanodeNodeRegistry::getRegisteredModels()')+'\n'
    result+=block(registry,'std::unique_ptr<ofxOceanodeNodeModel> ofxOceanodeNodeRegistry::create(')+'\n'
    result+=block(registry,'ofxOceanodeNodeRegistry::registeredModelsCategoryMap const &ofxOceanodeNodeRegistry::getRegisteredModelsCategoryAssociation()')+'\n'
    m=(ROOT/'src/Nodes/ofxOceanodeNodeModel.cpp').read_text()
    result+=m[m.index('template<typename T>\nstatic bool oceanodeJsonToVector'):m.index('void ofxOceanodeNodeModel::deserializeParameter(')]+'\n'
    for sig in ['ofColor colorFromString(', 'ofxOceanodeNodeModel::ofxOceanodeNodeModel(',
                'string ofxOceanodeNodeModel::getDescription()', 'string ofxOceanodeNodeModel::getParents()',
                'void ofxOceanodeNodeModel::setNumIdentifier(', 'void ofxOceanodeNodeModel::setContainer(',
                'void ofxOceanodeNodeModel::deserializeParameter(',
                'ofxOceanodeFrameTransportState ofxOceanodeNodeModel::getFrameTransportState()']:
        result+=block(m,sig)+'\n'
    gui=(ROOT/'src/Nodes/ofxOceanodeNodeGui.cpp').read_text()
    for sig in ['ofxOceanodeNodeGui::ofxOceanodeNodeGui(', 'ofxOceanodeNodeGui::~ofxOceanodeNodeGui()',
                'void ofxOceanodeNodeGui::enable()', 'void ofxOceanodeNodeGui::disable()',
                'void ofxOceanodeNodeGui::setPosition(', 'glm::vec2 ofxOceanodeNodeGui::getPosition()']:
        body=block(gui,sig)
        if sig.startswith('ofxOceanodeNodeGui::ofxOceanodeNodeGui'): body=body[:-1]+'++liveGui;}'
        if sig.startswith('ofxOceanodeNodeGui::~'): body=body[:-1]+'--liveGui; ++destroyedGui;}'
        result+=body+'\n'
    return result + history_adapters()

def native_production():
    result=production().replace(history_adapters(), '')+'\n'+SUPPORT+'\n'
    c=(ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    declarations=c[c.index('    struct ConnectionFailureInfo'):c.index('    void drawConnectionErrorDialogContents')]
    result+='namespace {\n'+declarations+'}\n'
    result += c[c.index('ofxOceanodeContainer::CanvasNodeSnapshot::~CanvasNodeSnapshot()'):]
    for signature in ['vector<ofxOceanodeNode*> ofxOceanodeContainer::getSelectedModules()']:
        result += block(c, signature) + '\n'

    for signature in [
        'void ofxOceanodeContainer::activate()', 'void ofxOceanodeContainer::deactivate()',
        'bool ofxOceanodeContainer::loadPreset(', 'bool ofxOceanodeContainer::savePreset(',
        'void ofxOceanodeContainer::loadPreset_presetWillBeLoaded()',
        'void ofxOceanodeContainer::loadPreset_loadNodes(',
        'void ofxOceanodeContainer::loadPreset_loadBeforeConnections(',
        'void ofxOceanodeContainer::loadPreset_deactivateConnections()',
        'void ofxOceanodeContainer::loadPreset_loadConnections(',
        'void ofxOceanodeContainer::loadPreset_midiBindings(',
        'void ofxOceanodeContainer::loadPreset_loadNodePreset(',
        'void ofxOceanodeContainer::loadPreset_activateConnections()',
        'void ofxOceanodeContainer::loadPreset_loadComments(',
        'void ofxOceanodeContainer::loadPreset_presetHasLoaded()',
        'void ofxOceanodeContainer::clearUserEditedValues()',
        'void ofxOceanodeContainer::setBpm(', 'void ofxOceanodeContainer::resetPhase(']:
        result+=block(c,signature)+'\n'
    model=(ROOT/'src/Nodes/ofxOceanodeNodeModel.cpp').read_text()
    for signature in ['shared_ptr<ofxOceanodeAbstractParameter> ofxOceanodeNodeModel::addParameter(',
                      'void ofxOceanodeNodeModel::addSeparator(',
                      'void ofxOceanodeNodeModel::addInspectorSeparator(',
                      'void ofxOceanodeNodeModel::addInspectorSeparator(string name, ofColor labelColor']:
        result+=block(model,signature)+'\n'
    gui=(ROOT/'src/Nodes/ofxOceanodeNodeMacroGui.cpp').read_text()
    result+=block(gui,'void ofxOceanodeNodeMacro::syncParameterGroupToSortOrder()')+'\n'
    canvas=(ROOT/'src/Managers/ofxOceanodeCanvas.cpp').read_text()
    result+=block(canvas,'void ofxOceanodeCanvas::setup(')+'\n'
    return result

def main(extra_sources=(), extra_includes=(), std='c++17'):
    of=of_root(); libdir=of/'libs/openFrameworksCompiled/lib/macos'
    includes=[*extra_includes, ROOT/'src', *sorted({p.parent for p in (ROOT/'src').rglob('*.h')})]
    includes+=sorted({p.parent for p in (of/'libs/openFrameworks').rglob('*.h')})
    includes += [p for p in (of/'libs').glob('*/include') if p.is_dir()]
    includes += [of/'libs/freetype/include/freetype',of/'libs/cairo/include/cairo']
    for part in ('src', 'libs'):
        includes += sorted({p.parent for p in (of/'addons/ofxImGui'/part).rglob('*.h')})
    libraries=[str(p) for p in sorted(libdir.glob('*.a')) if 'openframeworks' not in p.name.lower()]
    frameworks=['Accelerate','AppKit','ApplicationServices','AudioToolbox','AVFoundation','Cocoa','CoreAudio','CoreFoundation','CoreMedia','CoreServices','CoreVideo','IOKit','OpenGL','QuartzCore','Security','SystemConfiguration']
    debug_build=os.environ.get('OCEANODE_TEST_BUILD_DIR')
    with (contextlib.nullcontext(debug_build) if debug_build else tempfile.TemporaryDirectory(prefix='oceanode-macro-mixer-')) as tmp:
        tmp=Path(tmp); tmp.mkdir(parents=True,exist_ok=True)
        cpp=tmp/'regression.cpp'; cpp.write_text(native_production()+CASES)
        sources=['src/Nodes/MacroMixerMixing.cpp','src/Nodes/MacroMixerSource.cpp','src/Nodes/MacroMixerRuntime.cpp','src/Nodes/ofxOceanodeMacroMixer.cpp',
                 'src/Nodes/ofxOceanodeNode.cpp','src/ofxOceanodeParameter.cpp',
                 'src/Managers/ofxOceanodeTypesRegistry.cpp',
                 'src/Managers/ofxOceanodeTransport.cpp']
        sources += [f'src/Nodes/Default_Nodes/{part}/{name}.cpp' for part,name in (
            ('Modifiers','mapper'),('Modifiers','ranger'),('Modifiers','reindexer'),
            ('Modifiers','smoother'),('Modifiers','switcher'),('Generators','indexer'),
            ('Generators','oscillator'),('Generators','phasor'),('Base','baseIndexer'),
            ('Base','baseOscillator'),('Base','basePhasor'))]
        sources += [f'src/Nodes/{name}.cpp' for name in (
            'ofxOceanodeNodeMacro','MacroPresetManager','MacroRouterManager',
            'MacroSnapshotSystem','MacroMorphEngine','MacroRouterSortOrder','MacroRouterValueDispatch')]
        sources += [str(of/'libs/openFrameworks/utils/ofUtils.cpp')]
        sources += list(extra_sources)
        sources = list(dict.fromkeys(sources))
        # Compile the production Inspector along with the native Macro runtime.
        imgui=of/'addons/ofxImGui/libs/imgui/src'
        flags=[os.environ.get('CXX','clang++'),'-std='+std,'-O0','-g','-w','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DOF_NO_FMOD',
               *['-I'+str(p) for p in includes]]
        units=[cpp,*[ROOT/s for s in sources],
               *[imgui/s for s in ('imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp')],
               of/'libs/openFrameworks/types/ofParameter.cpp',of/'libs/openFrameworks/types/ofParameterGroup.cpp']
        objects=[tmp/(str(i)+'.o') for i in range(len(units))]
        def compile_unit(pair):
            source,obj=pair
            subprocess.run(flags+['-c',str(source),'-o',str(obj)],check=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=int(os.environ.get('OCEANODE_TEST_JOBS','2'))) as pool:
            list(pool.map(compile_unit,zip(units,objects)))
        cmd=[flags[0],'-fsanitize=address,undefined',*[str(obj) for obj in objects],
             str(libdir/'openFrameworks.a'),*libraries,'-Wl,-dead_strip']
        for f in frameworks: cmd+=['-framework',f]
        if debug_build:
            (tmp/'commands.json').write_text(json.dumps({'compile':[flags+['-c',str(s),'-o',str(o)] for s,o in zip(units,objects)],'link':cmd+['-o',str(tmp/'regression')]}))
        subprocess.run(cmd+['-o',str(tmp/'regression')],check=True)
        run_env={**os.environ, 'UBSAN_OPTIONS':os.environ.get('UBSAN_OPTIONS','')+':halt_on_error=1'}
        with tempfile.TemporaryDirectory(prefix='macro-mixer-sources-') as run_dir:
            subprocess.run([str(tmp/'regression'),'unused',run_dir],check=True,env=run_env)
    print('MacroMixer source tests completed.')


CASES = r'''static void near(float a,float b) { assert(std::abs(a-b)<.00001f); }
static int probes=0, blockedSetups=0, probeRecalls=0, probeRecallsWhileLoading=0;
class UnrestrictedProbe : public ofxOceanodeNodeModel {
public:
    UnrestrictedProbe():ofxOceanodeNodeModel("Unrestricted Probe") { ++probes; }
    void setup() override { addParameter(value.set("Value",.3f,0,1)); }
    // Nodes skip recall-time side effects while a preset loads (a synth's
    // saved Snapshot number must not recall that snapshot over its sound).
    void presetRecallAfterSettingParameters(ofJson&) override {
        ++probeRecalls; if(ofxOceanodeShared::isPresetLoading()) ++probeRecallsWhileLoading;
    }
    ofParameter<float> value;
};
class ExternalEndpoint : public ofxOceanodeNodeModel {
public:
    ExternalEndpoint():ofxOceanodeNodeModel("External Endpoint") {}
    std::string macroMixerBlocker() const override { return "Test hardware output"; }
    void setup() override { ++blockedSetups; }
};
static auto& floating(ofxOceanodeContainer& c,int id=1) {
    return c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f),id).getNodeModel();
}
static void write(const std::string& file,const ofJson& value) { std::ofstream(file)<<value; }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>();
    auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    types->registerType<std::vector<float>>("v_f"); singleton->registerType<std::vector<float>>("v_f");
    types->registerType<int>("i"); singleton->registerType<int>("i");
    types->registerType<std::vector<int>>("v_i"); singleton->registerType<std::vector<int>>("v_i");
    types->registerType<bool>("b"); singleton->registerType<bool>("b");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<router<std::vector<float>>>("Router","v_f",0.f,-100.f,100.f);
    registry->registerModel<router<int>>("Router","i",0,INT_MIN,INT_MAX);
    registry->registerModel<router<std::vector<int>>>("Router","v_i",0,INT_MIN,INT_MAX);
    registry->registerModel<router<bool>>("Router","b",false);
    registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    registry->registerModel<ofxOceanodeMacroMixer>("Macro");
    registry->registerModel<UnrestrictedProbe>("Previously Unsupported");
    registry->registerModel<ExternalEndpoint>("Hardware");
    registry->registerModel<counter>("Generators"); registry->registerModel<ramp>("Generators");
    registry->registerModel<simpleNumberGenerator>("Generators");
    auto transport=std::make_shared<ofxOceanodeTransport>(); transport->setBpm(137);
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("TestParent");
    const auto baseline=transport.use_count();
    auto saveSource=[&](const std::string& folder,float value,bool probe=false) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("TestSource");
        auto& selected=floating(c); selected.getParameter<std::string>("Name")="Value";
        selected.getParameter<float>("Value")=value;
        auto& extra=floating(c,2); extra.getParameter<std::string>("Name")="Unselected";
        if(probe) c.createNode(std::make_unique<UnrestrictedProbe>(),1);
        c.savePreset(folder);
    };
    const auto a=scratch+"/a", b=scratch+"/b", invalid=scratch+"/invalid";
    saveSource(a,.2,true); saveSource(b,.8); saveSource(invalid,.4);
    const auto constructorCount=probes;
    {
        MacroMixerRuntime mixer(parent);
        assert(mixer.importMacro(a) && mixer.importMacro(b));
        assert(probes==constructorCount); // Analysis must not instantiate source models.
        mixer.request(a,0,0); mixer.update(0); near(mixer.value(),.2);
        assert(mixer.origin()->macro && mixer.origin()->container->getAllModules().size()==3);
        assert(probes==constructorCount+1);
        const auto* origin=mixer.origin();
        mixer.request(b,1000,0); mixer.update(10); assert(mixer.destination());
        mixer.update(510); near(mixer.value(),.5); mixer.draw();
        const auto saved=mixer.saveState(); assert(saved.at("macros").size()==2 && saved.at("version")==1);
        assert(saved.at("playback")==mixer.destinationID());
        MacroMixerRuntime restored(parent); assert(restored.restoreState(saved)); restored.update(0); near(restored.value(),.8);
        // Sources inside the data root are saved relative to it and resolved on load.
        ofEnableDataPath(); ofSetDataPathRoot(scratch+"/");
        const auto relative=mixer.saveState();
        assert(relative.at("macros")[0].at("source")=="a" && relative.at("macros")[1].at("source")=="b");
        MacroMixerRuntime reloaded(parent); assert(reloaded.restoreState(relative)); reloaded.update(0); near(reloaded.value(),.8);
        assert(reloaded.macros()[0]->source()==mixer.macros()[0]->source());
        ofDisableDataPath();
        mixer.update(1010); near(mixer.value(),.8); assert(!mixer.destination() && mixer.origin()!=origin);
        const auto* running=mixer.origin();
        // Router mismatch and direct/nested external output fail without running it.
        auto json=ofLoadJson(invalid+"/Router_f_1.json");json["Name"]="Wrong";write(invalid+"/Router_f_1.json",json);
        assert(!mixer.importMacro(invalid)); assert(mixer.origin()==running);
        json["Name"]="Value";write(invalid+"/Router_f_1.json",json);
        auto modules=ofLoadJson(invalid+"/modules.json");modules["External Endpoint"]["01"]={0,0};write(invalid+"/modules.json",modules);
        write(invalid+"/External_Endpoint_1.json",ofJson::object());
        assert(!mixer.importMacro(invalid) && blockedSetups==0);
        // Copied registries reject later native reloads before endpoint factories.
        bool threw=false;try{mixer.origin()->container->getRegistry()->create("External Endpoint");}catch(const std::exception&){threw=true;}
        assert(threw && blockedSetups==0); assert(registry->create("External Endpoint"));
        const auto retainedNodes=mixer.origin()->container->getAllModules().size();
        assert(!mixer.origin()->container->loadPreset(invalid));
        assert(mixer.origin()->container->getAllModules().size()==retainedNodes && blockedSetups==0);
        mixer.update(1050);near(mixer.value(),.8);
        // Refresh/missing source preserves playback, even after source removal.
        fs::rename(b,b+"-missing");assert(!mixer.importMacro(b,true));mixer.update(1100);near(mixer.value(),.8);
        mixer.request(b,0,0);mixer.update(1200);assert(mixer.origin()==running);near(mixer.value(),.8);
        fs::rename(b+"-missing",b);
    }
    assert(transport.use_count()==baseline);
    // Real nested local Macro execution and recursive resource analysis.
    const auto nested=scratch+"/nested";saveSource(nested,.6);
    auto modules=ofLoadJson(nested+"/modules.json");modules["Macro"]["01"]={0,0};write(nested+"/modules.json",modules);
    write(nested+"/Macro_1.json",{{"LocalPreset",true}});fs::copy(a,nested+"/Macro_1",fs::copy_options::recursive);
    {
        MacroMixerRuntime mixer(parent);assert(mixer.importMacro(nested));mixer.request(nested,0,0);mixer.update(0);near(mixer.value(),.6);
        bool found=false;for(auto* n:mixer.origin()->container->getAllModules())if(auto* macro=dynamic_cast<ofxOceanodeNodeMacro*>(&n->getNodeModel())) {found=true;assert(macro->getContainer()->getAllModules().size()==3);}
        assert(found);
        auto nestedModules=ofLoadJson(nested+"/Macro_1/modules.json");nestedModules["External Endpoint"]["01"]={0,0};write(nested+"/Macro_1/modules.json",nestedModules);
        write(nested+"/Macro_1/External_Endpoint_1.json",ofJson::object());
        assert(!mixer.importMacro(nested,true) && blockedSetups==0);
    }
    // Every blocked node is reported. Keeping each one out of the graph lets the
    // import, activation and native reloads continue without creating it or
    // its cables; the choices persist, and a blocked activation resumes.
    {
        const auto blockedSource=scratch+"/blocked";saveSource(blockedSource,.35);
        auto blockedModules=ofLoadJson(blockedSource+"/modules.json");
        blockedModules["External Endpoint"]["01"]={0,0};blockedModules["External Endpoint"]["02"]={0,0};
        write(blockedSource+"/modules.json",blockedModules);
        write(blockedSource+"/External_Endpoint_1.json",ofJson::object());write(blockedSource+"/External_Endpoint_2.json",ofJson::object());
        auto cables=ofLoadJson(blockedSource+"/connections.json");if(!cables.is_object())cables=ofJson::object();
        cables["Router_f_1"]["Value"]["External_Endpoint_1"]["Value"]=ofJson::object();write(blockedSource+"/connections.json",cables);
        const auto canonical=fs::canonical(blockedSource).string();
        MacroMixerRuntime mixer(parent);
        assert(!mixer.importMacro(blockedSource) && blockedSetups==0);
        const auto& blocked=mixer.blockedNodes();
        assert(blocked.size()==2 && blocked[0].file==canonical && blocked[0].node=="External_Endpoint_1");
        assert(blocked[0].model=="External Endpoint" && blocked[0].message=="Test hardware output" && blocked[1].node=="External_Endpoint_2");
        assert(mixer.keepOutOfGraph(canonical,"External_Endpoint_1") && !mixer.importedMacro(blockedSource) && mixer.blockedNodes().size()==1);
        assert(mixer.keepOutOfGraph(canonical,"External_Endpoint_2") && mixer.blockedNodes().empty());
        auto revision=mixer.importedMacro(blockedSource);assert(revision);
        assert(revision->sourceDefinition().at("nodes").size()==2);
        assert(revision->sourceDefinition().at("connections").dump().find("External_Endpoint")==std::string::npos);
        mixer.request(blockedSource,0,0);mixer.update(0);near(mixer.value(),.35);
        assert(mixer.origin()->container->getAllModules().size()==2 && blockedSetups==0);
        assert(mixer.origin()->container->loadPreset(canonical));
        assert(mixer.origin()->container->getAllModules().size()==2 && blockedSetups==0);
        const auto saved=mixer.saveState();assert(saved.at("keptOutNodes").size()==2);
        MacroMixerRuntime restored(parent);assert(restored.restoreState(saved));restored.update(0);near(restored.value(),.35);
        assert(restored.keptOutNodes()==mixer.keptOutNodes() && blockedSetups==0);
        auto older=saved;older.erase("keptOutNodes");
        MacroMixerRuntime asking(parent);assert(asking.restoreState(older));asking.update(0);
        assert(!asking.origin() && asking.blockedNodes().size()==2);
        assert(asking.keepOutOfGraph(canonical,"External_Endpoint_1"));asking.update(1);assert(!asking.origin());
        assert(asking.keepOutOfGraph(canonical,"External_Endpoint_2"));asking.update(2);
        assert(asking.origin() && blockedSetups==0);near(asking.value(),.35);
        // Restoring a node asks again; cancelling keeps the running graph.
        assert(mixer.restoreKeptOutNode(canonical,"External_Endpoint_2"));
        assert(!mixer.importMacro(blockedSource,true) && mixer.blockedNodes().size()==1);
        mixer.cancelBlocked();assert(mixer.blockedNodes().empty());mixer.update(10);near(mixer.value(),.35);
    }
    // Global references use the real library resolver; live edits are checked
    // before the native global-Macro update callback can alter running nodes.
    const auto library=scratch+"/Macros/group/source";fs::create_directories(scratch+"/Macros/group");
    fs::copy(a,library,fs::copy_options::recursive);ofxOceanodeShared::readMacros();
    const auto global=scratch+"/global";saveSource(global,.55);
    auto globalModules=ofLoadJson(global+"/modules.json");globalModules["Macro"]["01"]={0,0};write(global+"/modules.json",globalModules);
    write(global+"/Macro_1.json",{{"LocalPreset",false},{"CategoryStruct",ofJson::array({"group"})},{"Macro","source"}});
    {
        MacroMixerRuntime mixer(parent);assert(mixer.importMacro(global));mixer.request(global,0,0);mixer.update(0);near(mixer.value(),.55);
        auto* child=dynamic_cast<ofxOceanodeNodeMacro*>(&mixer.origin()->container->getDynamicNodes().at("Macro").at(1)->getNodeModel());
        assert(child && child->getContainer()->getAllModules().size()==3);
        std::string path=ofxOceanodeShared::getMacroDirectoryStructure()->categories.front()->macros.front().second;
        // Saving a global definition updates the graph but keeps each
        // instance's own exposed values.
        child->getParameter<float>("Value")=.7;
        auto controls=ofLoadJson(library+"/Router_f_1.json");controls["Value"]=.9;write(library+"/Router_f_1.json",controls);
        ofxOceanodeShared::getMacroUpdatedEvent().notify(path);near(child->getParameter<float>("Value"),.7);
        auto edited=ofLoadJson(library+"/modules.json");edited["External Endpoint"]["01"]={0,0};write(library+"/modules.json",edited);
        write(library+"/External_Endpoint_1.json",ofJson::object());
        ofxOceanodeShared::getMacroUpdatedEvent().notify(path);
        assert(child->getContainer()->getAllModules().size()==3 && blockedSetups==0);near(child->getParameter<float>("Value"),.7);
        mixer.update(10);near(mixer.value(),.55);assert(!mixer.importMacro(global,true));
    }
    // Instances of one global Macro share a single canvas tab; local Macros keep their own.
    {
        // The library definition now holds the External Endpoint added above.
        const int blockedBefore=blockedSetups;
        auto globalInstance=[&](int id)->ofxOceanodeNodeMacro& {
            auto& macro=static_cast<ofxOceanodeNodeMacro&>(parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),id).getNodeModel());
            ofJson reference={{"LocalPreset",false},{"CategoryStruct",ofJson::array({"group"})},{"Macro","source"}};
            macro.macroLoad(reference,scratch);assert(!macro.isLocal());return macro;
        };
        auto& first=globalInstance(1); auto& second=globalInstance(2);
        auto& local=static_cast<ofxOceanodeNodeMacro&>(parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),3).getNodeModel());
        const int recallsBefore=probeRecalls, loadingBefore=probeRecallsWhileLoading;
        assert(!ofxOceanodeShared::isPresetLoading());
        assert(local.loadMacroSource(a));
        // A source load is a preset recall for its nodes, without app-level preset events.
        assert(probeRecalls>recallsBefore && probeRecallsWhileLoading-loadingBefore==probeRecalls-recallsBefore);
        assert(!ofxOceanodeShared::isPresetLoading());
        // Showing any instance opens the first one; only it can close the tab.
        assert(&second.windowInstance()==&first && &local.windowInstance()==&local);
        // Only the first instance may save over the library Macro.
        assert(first.canSaveDefinition() && !second.canSaveDefinition() && local.canSaveDefinition());
        assert(first.sharesGlobalMacroWith(second) && !first.sharesGlobalMacroWith(local));
        second.setWindowVisible(true);
        assert(first.isWindowVisible() && !second.isWindowVisible() && second.isSharedWindowVisible());
        assert(second.isWindowShownByOtherInstance() && !first.isWindowShownByOtherInstance());
        assert(!local.isSharedWindowVisible());
        second.activateWindow(); assert(first.isWindowVisible() && !second.isWindowVisible());
        local.setWindowVisible(true); assert(local.isWindowVisible() && first.isWindowVisible());
        // Navigation into the second instance lands on the matching node of the first.
        auto& firstNodes=first.getContainer()->getParameterGroupNodesMap();
        auto& secondNodes=second.getContainer()->getParameterGroupNodesMap();
        assert(!secondNodes.empty());
        const auto target=second.windowTargetFor(secondNodes.begin()->second);
        assert(target.macro==&first && target.node==firstNodes.at(secondNodes.begin()->first));
        assert(first.windowTargetFor(firstNodes.begin()->second).node==firstNodes.begin()->second);
        first.setWindowVisible(false);
        assert(!first.isSharedWindowVisible() && !second.isSharedWindowVisible() && local.isWindowVisible());
        parent.clearContainer(); blockedSetups=blockedBefore;
    }
    const auto cycle=scratch+"/cycle";saveSource(cycle,.5);
    auto cyclicModules=ofLoadJson(cycle+"/modules.json");cyclicModules["Macro"]["01"]={0,0};write(cycle+"/modules.json",cyclicModules);
    write(cycle+"/Macro_1.json",{{"LocalPreset",true}});fs::create_directory_symlink(cycle,cycle+"/Macro_1");
    { MacroMixerRuntime mixer(parent);assert(!mixer.importMacro(cycle));assert(blockedSetups==0); }
    // Actual feedback cables have the same native loader result as a standalone Macro.
    const auto feedback=scratch+"/feedback";saveSource(feedback,.25);
    write(feedback+"/connections.json",{{"Router_f_1",{{"Value",{{"Router_f_2",{{"Value",true}}}}}}},{"Router_f_2",{{"Value",{{"Router_f_1",{{"Value",true}}}}}}}});
    {
        auto& n=parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(),1);
        auto& native=static_cast<ofxOceanodeNodeMacro&>(n.getNodeModel());assert(native.loadMacroSource(feedback));
        MacroMixerRuntime mixer(parent); assert(mixer.importMacro(feedback));mixer.request(feedback,0,0);mixer.update(0);
        assert(mixer.origin() && mixer.origin()->container->getAllConnections().size()==2);
        ofEventArgs args;native.update(args);
        auto& ref=native.getContainer()->getDynamicNodes().at("Router f").at(1)->getNodeModel();
        near(mixer.value(),ref.getParameter<float>("Value"));
        parent.clearContainer();
    }
    // Shared Input injection and selected Output, with extra routers retained.
    const auto inputs=scratch+"/inputs";saveSource(inputs,.1);
    write(inputs+"/connections.json",{{"Router_f_2",{{"Value",{{"Router_f_1",{{"Value",true}}}}}}}});
    MacroMixerContract contract;contract.routers.push_back({"input","Unselected","Float","",ofJson::object(),"Input"});
    {
        MacroMixerRuntime mixer(parent);assert(mixer.setContract(contract));assert(mixer.importMacro(inputs));
        assert(mixer.setInputs({{"input",.7f}}));mixer.request(inputs,0,0);mixer.update(0);near(mixer.value(),.7);
        assert(mixer.setInputs({{"input",.4f}}));mixer.update(10);near(mixer.value(),.4);
    }
    // Removing a running, destination or queued Macro clears playback in the
    // same operation. Unrelated/unknown removals leave playback intact.
    const auto third=scratch+"/third"; saveSource(third,.6f);
    for(int role=0;role<3;++role) {
        MacroMixerRuntime mixer(parent);
        assert(mixer.importMacro(a) && mixer.importMacro(b) && mixer.importMacro(third));
        const auto aID=mixer.importedMacro(a)->id(), bID=mixer.importedMacro(b)->id(), thirdID=mixer.importedMacro(third)->id();
        mixer.requestMacro(aID,0,0); mixer.update(0); near(mixer.value(),.2f);
        const auto* playing=mixer.origin();
        assert(!mixer.removeMacro("unknown") && mixer.origin()==playing);
        assert(mixer.removeMacro(thirdID) && mixer.origin()==playing); near(mixer.value(),.2f);
        assert(mixer.importMacro(third));
        const auto queuedID=mixer.importedMacro(third)->id();
        mixer.requestMacro(bID,1000,0); mixer.update(10); assert(mixer.destination());
        mixer.requestMacro(queuedID,1000,0); assert(mixer.queuedID()==queuedID);
        assert(mixer.removeMacro(role==0 ? mixer.origin()->revision->id() : role==1 ? bID : queuedID));
        assert(!mixer.origin() && !mixer.destination() && mixer.queuedID().empty());
        assert(mixer.macros().size()==2 && mixer.saveState().at("playback").is_null());
        near(mixer.value(),0); near(mixer.progress(),0);
        mixer.update(2000); assert(!mixer.origin());
    }
    // Router removal disconnects all outer cables, preserving other routers,
    // imports and the running graph without consulting the source folder again.
    {
        auto& m=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        MacroMixerContract c; c.routers.push_back({"input","Unselected","Float","",ofJson::object(),"Input"});
        assert(m.setRouterContract(c) && m.importMacroFolder(a));
        m.getParameter<float>("Duration Ms")=0; m.getParameter<void>("Transition").trigger(); parent.update();
        auto& source=floating(parent,7); auto& sink=floating(parent,8); floating(parent,9);
        assert(parent.createConnectionFromInfo("Router_f_7","Value","MacroMixer_1","Unselected"));
        assert(parent.createConnectionFromInfo("MacroMixer_1","Value","Router_f_8","Value"));
        assert(parent.createConnectionFromInfo("MacroMixer_1","Value","Router_f_9","Value"));
        assert(parent.getAllConnections().size()==3);
        auto rejected=c; rejected.routers[0].name="Renamed";
        assert(!m.setRouterContract(rejected) && parent.getAllConnections().size()==3);
        const auto* playing=m.mixerRuntime()->origin();
        fs::rename(a,a+"-offline");
        c=m.mixerRuntime()->contract(); c.routers.erase(c.routers.begin());
        assert(m.setRouterContract(c));
        assert(!m.getParameterGroup().contains("Value") && parent.getAllConnections().size()==1);
        assert(!static_cast<ofxOceanodeAbstractParameter&>(sink.getParameterGroup().get("Value")).hasInConnection());
        assert(m.mixerRuntime()->origin()==playing && m.mixerRuntime()->macros().size()==1);
        source.getParameter<float>("Value")=.45f; parent.update();
        assert(m.mixerRuntime()->origin()==playing);
        near(m.getParameter<float>("Unselected"),.45f);
        c.routers.clear(); assert(m.setRouterContract(c));
        assert(parent.getAllConnections().empty() && !m.mixerRuntime()->origin());
        assert(!static_cast<ofxOceanodeAbstractParameter&>(source.getParameterGroup().get("Value")).hasOutConnections());
        assert(m.mixerRuntime()->macros().size()==1);
        MacroMixerRuntime restored(parent); assert(restored.restoreState(m.mixerRuntime()->saveState()));
        assert(restored.contract().routers.empty() && restored.macros().size()==1 && !restored.origin());
        fs::rename(a+"-offline",a); parent.clearContainer();
    }
    // Node persistence recreates pins before outer cables; duplicate runtimes are independent.
    {
        auto& m=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        assert(m.setRouterContract({}));assert(m.importMacroFolder(a));assert(m.importMacroFolder(b));
        m.getParameter<float>("Duration Ms")=0;m.getParameter<void>("Transition").trigger();parent.update();
        auto& out=floating(parent,8);out.getParameter<std::string>("Name")="Outer";
        assert(parent.createConnectionFromInfo("MacroMixer_1","Value","Router_f_8","Value"));near(out.getParameter<float>("Value"),.2);
        parent.savePreset(scratch+"/session");parent.clearContainer();assert(parent.loadPreset(scratch+"/session"));parent.update();
        auto& loaded=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(1)->getNodeModel());
        assert(loaded.mixerRuntime()->origin());near(loaded.getParameter<float>("Value"),.2);assert(parent.getAllConnections().size()==1);
        // Loading a preset reads every source before playback starts.
        for(const auto& source:loaded.mixerRuntime()->macros()) assert(source->analysed()&&loaded.mixerRuntime()->sourceProblem(source->id()).empty());
        assert(loaded.getParameter<std::string>("Mixer Status").get()=="Run 0 a | 2/2 ok");
        {
            const auto session=scratch+"/offline-session"; parent.savePreset(session); parent.clearContainer();
            fs::rename(b,b+"-offline"); assert(parent.loadPreset(session)); parent.update();
            auto& offline=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(1)->getNodeModel());
            const auto& sources=offline.mixerRuntime()->macros();
            assert(sources.size()==2&&offline.mixerRuntime()->sourceProblem(sources[0]->id()).empty());
            assert(!sources[1]->analysed()&&!offline.mixerRuntime()->sourceProblem(sources[1]->id()).empty());
            assert(offline.mixerRuntime()->origin()&&offline.getParameter<std::string>("Mixer Status").get()=="Run 0 a | 1/2 ok");
            parent.clearContainer(); fs::rename(b+"-offline",b); assert(parent.loadPreset(session)); parent.update();
        }
        auto& reloaded=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(1)->getNodeModel());
        assert(reloaded.getParameter<std::string>("Mixer Status").get()=="Run 0 a | 2/2 ok");
        reloaded.getParameter<int>("Target")=1; reloaded.getParameter<float>("Duration Ms")=1e6; reloaded.getParameter<void>("Transition").trigger(); parent.update();
        assert(reloaded.getParameter<std::string>("Mixer Status").get().rfind("Mix 0>1 ",0)==0);
        reloaded.getParameter<void>("Finish Transition").trigger(); parent.update();
        assert(reloaded.getParameter<std::string>("Mixer Status").get()=="Run 1 b | 2/2 ok");
        reloaded.getParameter<int>("Target")=0; reloaded.getParameter<float>("Duration Ms")=0; reloaded.getParameter<void>("Transition").trigger(); parent.update();
        parent.saveClipboardModulesAndConnections({parent.getDynamicNodes().at("MacroMixer").at(1).get()},{0,0});
        assert(parent.loadClipboardModulesAndConnections({10,10},false));parent.update();
        auto& copy=static_cast<ofxOceanodeMacroMixer&>(parent.getDynamicNodes().at("MacroMixer").at(2)->getNodeModel());
        assert(copy.mixerRuntime()->origin()->container!=reloaded.mixerRuntime()->origin()->container);
        copy.getParameter<int>("Target")=1;copy.getParameter<void>("Transition").trigger();parent.update();near(copy.getParameter<float>("Value"),.8);near(reloaded.getParameter<float>("Value"),.2);
        const auto groupPath=scratch+"/playing-group.json";
        assert(reloaded.saveRouterGroup(groupPath));
        auto group=ofLoadJson(groupPath); group["routers"][0]["id"]="another-mixer-id"; write(groupPath,group);
        const auto* playing=reloaded.mixerRuntime()->origin();
        assert(reloaded.loadRouterGroup(groupPath) && reloaded.mixerRuntime()->origin()==playing);
        assert(parent.getAllConnections().size()==1);
        group["routers"][0]["name"]="Different Interface"; write(groupPath,group);
        assert(!reloaded.loadRouterGroup(groupPath) && reloaded.mixerRuntime()->origin()==playing);
        assert(parent.getAllConnections().size()==1); near(reloaded.getParameter<float>("Value"),.2);
        assert(reloaded.removeImportedMacro(reloaded.mixerRuntime()->currentID()));
        assert(!reloaded.mixerRuntime()->origin() && reloaded.mixerRuntime()->macros().size()==1);
        near(reloaded.getParameter<float>("Value"),0);
        near(parent.getDynamicNodes().at("Router f").at(8)->getNodeModel().getParameter<float>("Value"),0);
        near(copy.getParameter<float>("Value"),.8);
        parent.clearContainer();
    }
    // Router groups round-trip definitions/order/policy options without storing
    // Macro sources or live input values. Invalid files leave ports/cables intact.
    {
        auto& m=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),1).getNodeModel());
        MacroMixerContract group;
        group.routers[0].policy="switch"; group.routers[0].options={{"threshold",.3}};
        group.routers.push_back({"vector","Positions","Float Vector","resample",{{"size",13}},"Output"});
        group.routers.push_back({"count","Count","Integer","",ofJson::object(),"Input"});
        group.routers.push_back({"gate","Gate","Boolean","switch",{{"threshold",.75}},"Output"});
        assert(m.setRouterContract(group)); m.getParameter<int>("Count")=42;
        const auto filename=scratch+"/group.json";
        assert(m.saveRouterGroup(scratch+"/group")); assert(fs::exists(filename));
        const auto saved=ofLoadJson(filename);
        assert(saved.at("format")=="ofxOceanodeMacroMixer.routergroup" && saved.at("version")==1);
        assert(saved.size()==3 && saved.at("routers")==m.mixerRuntime()->saveState().at("routers"));
        auto& copy=static_cast<ofxOceanodeMacroMixer&>(parent.createNode(std::make_unique<ofxOceanodeMacroMixer>(),2).getNodeModel());
        assert(copy.loadRouterGroup(filename));
        assert(copy.mixerRuntime()->saveState().at("routers")==saved.at("routers"));
        assert(copy.getParameter<int>("Count")==0);
        floating(parent,9);
        assert(parent.createConnectionFromInfo("MacroMixer_1","Value","Router_f_9","Value"));
        auto* port=&m.getParameterGroup().get("Value");
        auto foreign=saved;
        for(auto& router:foreign["routers"]) router["id"]="foreign-"+router.at("id").get<std::string>();
        write(filename,foreign); assert(m.loadRouterGroup(filename));
        assert(&m.getParameterGroup().get("Value")==port && parent.getAllConnections().size()==1);
        assert(m.getParameter<int>("Count")==42);
        const auto before=m.mixerRuntime()->saveState();
        auto reject=[&](ofJson invalid) {
            write(filename,invalid); assert(!m.loadRouterGroup(filename));
            assert(m.mixerRuntime()->saveState()==before && parent.getAllConnections().size()==1);
            assert(&m.getParameterGroup().get("Value")==port);
        };
        auto bad=saved; bad["version"]=2; reject(bad);
        bad=saved; bad["routers"][0].erase("type"); reject(bad);
        bad=saved; bad["routers"][0]["name"]="Target"; reject(bad);
        bad=saved; bad["routers"][1]["id"]=bad["routers"][0]["id"]; reject(bad);
        bad=saved; bad["routers"][1]["name"]="Value"; reject(bad);
        bad=saved; bad["routers"][0]["direction"]="Sideways"; reject(bad);
        bad=saved; bad["routers"][0]["type"]="Unavailable"; reject(bad);
        bad=saved; bad["routers"][0]["type"]="Texture"; reject(bad); // No Texture router registered in this host.
        bad=saved; bad["routers"][0]["policy"]="unknown"; reject(bad);
        bad=saved; bad["routers"][0]["options"]["threshold"]=2; reject(bad);
        bad=saved; bad["routers"][1]["options"]["size"]=-1; reject(bad);
        bad=saved; bad["routers"][2]["policy"]="linear"; reject(bad);
        bad=saved; bad["routers"]=ofJson::array(); for(int i=0;i<65;++i) bad["routers"].push_back(saved["routers"][0]); reject(bad);
        std::ofstream(filename)<<"{ broken json"; assert(!m.loadRouterGroup(filename));
        assert(m.mixerRuntime()->saveState()==before && parent.getAllConnections().size()==1);
        assert(!m.loadRouterGroup(scratch+"/missing.json"));
        fs::create_directory(scratch+"/directory.json"); assert(!m.saveRouterGroup(scratch+"/directory.json"));
        // Replacing an unrelated local ID removes its old cable, as normal router removal does.
        bad=saved; bad["routers"]=ofJson::array({saved["routers"][0]}); bad["routers"][0]["name"]="Replacement";
        write(filename,bad); assert(m.loadRouterGroup(filename));
        assert(!m.getParameterGroup().contains("Value") && m.getParameterGroup().contains("Replacement"));
        assert(parent.getAllConnections().empty());
        bad["routers"]=ofJson::array(); write(filename,bad); assert(m.loadRouterGroup(filename));
        assert(m.mixerRuntime()->contract().routers.empty());
        assert(m.saveRouterGroup(filename) && copy.loadRouterGroup(filename));
        assert(copy.mixerRuntime()->contract().routers.empty());
        parent.clearContainer();
    }
    assert(transport.use_count()==baseline && blockedSetups==0 && liveGui==0);
    std::cout<<"Native MacroMixer lifecycle, selection, feedback, nested analysis, automatic removal cleanup, router group files and persistence passed.\n";
}
'''

if __name__=="__main__": main()
