#!/usr/bin/env python3
"""Check real registration overloads and factory metadata without a host app.

Covers old category-only/constructor forms, explicit flags, flag combinations,
factory copies, refresh paths, duplicate names and isSink overrides under ASan/UBSan.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block
from of_environment import of_root
ROOT=Path(__file__).resolve().parents[1]
MODEL=r'''
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
class ofxOceanodeNodeModel {
    string name; bool declaredSink=false;
public:
    explicit ofxOceanodeNodeModel(string n):name(n){}
    virtual ~ofxOceanodeNodeModel()=default;
    string nodeName(){return name;}
    string getDescription(){return "Description: "+name;}
    virtual string macroMixerBlocker() const{return {};}
    void setDeclaredSink(bool b){declaredSink=b;}
    virtual bool isSink() const{return declaredSink;}
};
'''
CASES=r'''
#include "ofxOceanodeNodeRegistry.h"
#include <cassert>
#include <cstdio>
class ofxOceanode {
public:
    shared_ptr<ofxOceanodeNodeRegistry> nodeRegistry=make_shared<ofxOceanodeNodeRegistry>();
    REGISTRATION_WRAPPERS
};
struct Plain:ofxOceanodeNodeModel{Plain():ofxOceanodeNodeModel("Plain") {}};
struct Sinker:ofxOceanodeNodeModel{Sinker():ofxOceanodeNodeModel("Sinker") {}};
struct Integer:ofxOceanodeNodeModel{int value;explicit Integer(int i):ofxOceanodeNodeModel("Integer"),value(i){}};
struct Boolean:ofxOceanodeNodeModel{bool value;explicit Boolean(bool b):ofxOceanodeNodeModel("Boolean"),value(b){}};
enum class ConstructorEnum {Value};
struct EnumArgument:ofxOceanodeNodeModel{explicit EnumArgument(ConstructorEnum):ofxOceanodeNodeModel("EnumArgument") {}};
struct Arguments:ofxOceanodeNodeModel{
    int number;bool enabled;
    Arguments(string name,int i,bool b):ofxOceanodeNodeModel(name),number(i),enabled(b){}
};
struct AlwaysSink:ofxOceanodeNodeModel{
    AlwaysSink():ofxOceanodeNodeModel("AlwaysSink"){}
    bool isSink() const override{return true;}
};
struct NeverSink:ofxOceanodeNodeModel{
    NeverSink():ofxOceanodeNodeModel("NeverSink"){}
    bool isSink() const override{return false;}
};
struct Counted:ofxOceanodeNodeModel{
    inline static int constructions=0;
    Counted():ofxOceanodeNodeModel("Counted"){++constructions;}
};
PRODUCTION_METHODS
int main(){
    using Flags=ofxOceanodeNodeRegistrationFlags;
    ofxOceanode app;
    app.registerModel<Plain>();
    app.registerModel<Integer>("Collection",23);
    app.registerModel<Boolean>("Collection",true);
    app.registerModel<EnumArgument>("Collection",ConstructorEnum::Value);
    app.registerModel<Arguments,string,int,bool>("Collection","Legacy",9,false);
    auto& registry=*app.nodeRegistry;
    auto sink=[&](const char* name){return registry.create(name)->isSink();};
    // Legacy forms stay plain models and keep their constructor arguments.
    assert(!sink("Plain"));
    auto i=registry.create("Integer");assert(!i->isSink()&&static_cast<Integer&>(*i).value==23);
    auto b=registry.create("Boolean");assert(!b->isSink()&&static_cast<Boolean&>(*b).value);
    assert(!sink("EnumArgument"));
    auto legacy=registry.create("Legacy");assert(!legacy->isSink()&&static_cast<Arguments&>(*legacy).number==9);
    // Sink sets the declared state, alone and combined, with constructor arguments after it.
    app.registerModel<Sinker>("Collection",Flags::Sink);
    assert(sink("Sinker"));
    app.registerModel<Arguments>("Collection",Flags::None|Flags::Sink,"Flagged",42,true);
    auto flagged=registry.create("Flagged");assert(flagged->isSink());
    assert(static_cast<Arguments&>(*flagged).number==42&&static_cast<Arguments&>(*flagged).enabled);
    app.registerModel<Arguments>("Collection",Flags::None,"ExplicitNone",7,true);
    assert(!sink("ExplicitNone"));
    app.registerModel<Arguments>("Collection",Flags::CanRemoveWhenUnused|Flags::Sink,"Both",5,false);
    assert(sink("Both"));
    // The deprecated flag compiles, is ignored, and never makes a sink.
    app.registerModel<Arguments>("Collection",Flags::CanRemoveWhenUnused,"Deprecated",6,true);
    assert(!sink("Deprecated")&&static_cast<Arguments&>(*registry.create("Deprecated")).number==6);
    // Direct registry callers get identical overload and constructor behavior.
    registry.registerModel<Arguments>("Direct",Flags::Sink,"DirectFlagged",12,false);
    registry.registerModel<Arguments>("Direct","DirectLegacy",13,true);
    assert(sink("DirectFlagged")&&!sink("DirectLegacy"));
    // A model's own isSink override beats the registration flag in both directions.
    app.registerModel<AlwaysSink>("Collection");
    assert(sink("AlwaysSink"));
    app.registerModel<NeverSink>("Collection",Flags::Sink);
    assert(!sink("NeverSink"));
    // Re-registration cannot silently reclassify a model or its category.
    app.registerModel<Plain>("Other",Flags::Sink);
    assert(!sink("Plain"));
    assert(registry.getRegisteredModelsCategoryAssociation().at("Plain")=="Default");
    app.registerModel<Sinker>("Other");assert(sink("Sinker"));
    // One prototype plus one live model per registration: flags add no constructions.
    registry.registerModel<Counted>("Collection",Flags::Sink);
    assert(Counted::constructions==1);auto counted=registry.create("Counted");assert(Counted::constructions==2);
    assert(counted->isSink());
    // Copies carry the factories and the flags behind them.
    ofxOceanodeNodeRegistry copy=registry;
    assert(copy.create("Sinker")->isSink()&&!copy.create("Plain")->isSink());
    // Refreshing a factory keeps the registered flags, by both refresh paths.
    assert(registry.refreshModel<Integer>("Collection",99));
    assert(static_cast<Integer&>(*registry.create("Integer")).value==99&&!sink("Integer"));
    assert(registry.refreshModel<Sinker>("Collection"));
    assert(sink("Sinker"));
    assert(registry.refreshModelNamed<Sinker>("Sinker","Collection"));
    assert(sink("Sinker"));
    assert(registry.refreshModelNamed<Plain>("Plain","Default"));
    assert(!sink("Plain"));
    // Unregistering forgets the flags, so a later registration starts clean.
    registry.unregisterModel<Arguments>("Direct","DirectLegacy",13,true);assert(!registry.create("DirectLegacy"));
    registry.unregisterModel<Sinker>("Collection");assert(!registry.create("Sinker"));
    registry.registerModel<Sinker>("Collection");assert(!sink("Sinker"));
    assert(registry.getModelDescriptions().at("Sinker")=="Description: Sinker");
    puts("Registration flags passed: legacy arguments, Sink factories, combinations, deprecated flag, overrides, duplicate names, copies, refresh and unregister.");
}
'''
def main():
    facade=(ROOT/'src/ofxOceanode.h').read_text()
    a=facade.index('    template<typename T>\n    void registerModel(')
    b=facade.index('    template<typename T, typename... Args>\n    bool refreshModel(',a)
    registry=(ROOT/'src/Managers/ofxOceanodeNodeRegistry.cpp').read_text()
    production='\n'.join(block(registry,s) for s in (
        'ofxOceanodeNodeRegistry::ofxOceanodeNodeRegistry()',
        'std::unique_ptr<ofxOceanodeNodeModel> ofxOceanodeNodeRegistry::create(',
        'ofxOceanodeNodeRegistry::registeredModelsCategoryMap const &ofxOceanodeNodeRegistry::getRegisteredModelsCategoryAssociation()'))
    with tempfile.TemporaryDirectory(prefix='oceanode-registration-') as directory:
        work=Path(directory);(work/'ofxOceanodeNodeModel.h').write_text(MODEL)
        (work/'ofJson.h').write_text('#pragma once\n#include <nlohmann/json.hpp>\nusing ofJson = nlohmann::json;\n')
        (work/'ofxOceanodeNodeHelpCatalog.h').write_text('#pragma once\ninline std::string ofxOceanodeNodeHelpDescription(const std::string&){return {};}\n')
        cpp=work/'test.cpp';cpp.write_text(CASES.replace('REGISTRATION_WRAPPERS',facade[a:b]).replace('PRODUCTION_METHODS',production));binary=work/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-I',str(work),'-I',str(ROOT/'src/Managers'),'-I',str(of_root()/'libs/json/include'),str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
if __name__=='__main__':main()
