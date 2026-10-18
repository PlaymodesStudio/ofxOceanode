#!/usr/bin/env python3
"""Check real registration overloads and factory metadata without a host app.

Covers old category-only/constructor forms, explicit flags, flag combinations,
factory copies, duplicate names and model protection overrides under ASan/UBSan.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from new_preset_regression import block
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
    string name; bool cleanupSafe=false;
public:
    explicit ofxOceanodeNodeModel(string n):name(n){}
    virtual ~ofxOceanodeNodeModel()=default;
    string nodeName(){return name;}
    string getDescription(){return "Description: "+name;}
    virtual string macroMixerBlocker() const{return {};}
    void setUnusedCleanupSafe(bool b){cleanupSafe=b;}
    virtual bool canRemoveWhenUnused() const{return cleanupSafe;}
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
struct Safe:ofxOceanodeNodeModel{Safe():ofxOceanodeNodeModel("Safe") {}};
struct Integer:ofxOceanodeNodeModel{int value;explicit Integer(int i):ofxOceanodeNodeModel("Integer"),value(i){}};
struct Boolean:ofxOceanodeNodeModel{bool value;explicit Boolean(bool b):ofxOceanodeNodeModel("Boolean"),value(b){}};
enum class ConstructorEnum {Value};
struct EnumArgument:ofxOceanodeNodeModel{explicit EnumArgument(ConstructorEnum):ofxOceanodeNodeModel("EnumArgument") {}};
struct Arguments:ofxOceanodeNodeModel{
    int number;bool enabled;
    Arguments(string name,int i,bool b):ofxOceanodeNodeModel(name),number(i),enabled(b){}
};
struct Protected:ofxOceanodeNodeModel{
    Protected():ofxOceanodeNodeModel("Protected"){}
    bool canRemoveWhenUnused() const override{return false;}
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
    assert(!registry.create("Plain")->canRemoveWhenUnused());
    auto i=registry.create("Integer");assert(!i->canRemoveWhenUnused()&&static_cast<Integer&>(*i).value==23);
    auto b=registry.create("Boolean");assert(!b->canRemoveWhenUnused()&&static_cast<Boolean&>(*b).value);
    assert(!registry.create("EnumArgument")->canRemoveWhenUnused());
    auto legacy=registry.create("Legacy");assert(!legacy->canRemoveWhenUnused()&&static_cast<Arguments&>(*legacy).number==9);
    app.registerModel<Safe>("Collection",Flags::CanRemoveWhenUnused);
    assert(registry.create("Safe")->canRemoveWhenUnused());
    app.registerModel<Arguments>("Collection",Flags::None|Flags::CanRemoveWhenUnused,"Flagged",42,true);
    auto flagged=registry.create("Flagged");assert(flagged->canRemoveWhenUnused());
    assert(static_cast<Arguments&>(*flagged).number==42&&static_cast<Arguments&>(*flagged).enabled);
    app.registerModel<Arguments>("Collection",Flags::None,"ExplicitNone",7,true);
    assert(!registry.create("ExplicitNone")->canRemoveWhenUnused());
    // Direct registry callers get identical overload and constructor behavior.
    registry.registerModel<Arguments>("Direct",Flags::CanRemoveWhenUnused,"DirectFlagged",12,false);
    registry.registerModel<Arguments>("Direct","DirectLegacy",13,true);
    assert(registry.create("DirectFlagged")->canRemoveWhenUnused());
    assert(!registry.create("DirectLegacy")->canRemoveWhenUnused());
    // Re-registration cannot silently reclassify a protected model or its category.
    app.registerModel<Plain>("Other",Flags::CanRemoveWhenUnused);
    assert(!registry.create("Plain")->canRemoveWhenUnused());
    assert(registry.getRegisteredModelsCategoryAssociation().at("Plain")=="Default");
    app.registerModel<Safe>("Other");assert(registry.create("Safe")->canRemoveWhenUnused());
    app.registerModel<Protected>("Collection",Flags::CanRemoveWhenUnused);
    assert(!registry.create("Protected")->canRemoveWhenUnused());
    registry.registerModel<Counted>("Collection",Flags::CanRemoveWhenUnused);
    assert(Counted::constructions==1);auto counted=registry.create("Counted");assert(Counted::constructions==2);
    ofxOceanodeNodeRegistry copy=registry;assert(copy.create("Safe")->canRemoveWhenUnused());
    assert(registry.refreshModel<Integer>("Collection",99));
    assert(static_cast<Integer&>(*registry.create("Integer")).value==99);
    registry.unregisterModel<Arguments>("Direct","DirectLegacy",13,true);assert(!registry.create("DirectLegacy"));
    assert(registry.getModelDescriptions().at("Safe")=="Description: Safe");
    puts("Registration flags passed: legacy arguments, flagged factories, combinations, defaults, duplicate names, copies and protected overrides.");
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
        (work/'ofxOceanodeNodeHelpCatalog.h').write_text('#pragma once\ninline std::string ofxOceanodeNodeHelpDescription(const std::string&){return {};}\n')
        cpp=work/'test.cpp';cpp.write_text(CASES.replace('REGISTRATION_WRAPPERS',facade[a:b]).replace('PRODUCTION_METHODS',production));binary=work/'test'
        subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-I',str(work),'-I',str(ROOT/'src/Managers'),str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
if __name__=='__main__':main()
