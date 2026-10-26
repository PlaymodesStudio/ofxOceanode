#!/usr/bin/env python3
"""Exercise production proposal validation and the embedded help catalog.
No credentials, model calls, GUI, audio server or OF event loop are required.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from of_environment import ROOT, of_root

CASES = r'''
#include "Managers/ofxOceanodeHelpPlan.h"
#include "ofxOceanodeNodeHelpCatalog.h"
#include "Managers/ofxOceanodeNodeRegistry.h"
#include <cassert>
#include <iostream>
using nlohmann::json;
ofxOceanodeNodeRegistry::ofxOceanodeNodeRegistry() {}
struct ImportedSpec { std::string description; };
struct HeavyModel : ofxOceanodeNodeModel {
    static int constructions;
    HeavyModel() { ++constructions; }
    HeavyModel(ImportedSpec) { ++constructions; }
};
int HeavyModel::constructions = 0;
struct ExternalModel : HeavyModel {
    std::string macroMixerBlocker() const override { return "External test output"; }
};
static bool known(const std::string& name) { return name == "Phasor" || name == "Oscillator"; }
static bool rejected(const json& plan) {
    try { ofxOceanodeHelpPlan::validate(plan, known); return false; }
    catch(const std::exception&) { return true; }
}
int main() {
    ofxOceanodeNodeRegistry registry;
    assert(registry.refreshModelNamed<HeavyModel>("SC Imported", "Synths", ImportedSpec{"A supplied synth description"}));
    assert(HeavyModel::constructions == 0);
    assert(registry.getModelDescriptions().at("SC Imported") == "A supplied synth description");
    assert(!registry.refreshModelNamed<HeavyModel>("SC Imported", "Other", ImportedSpec{"Wrong category"}));
    assert(registry.getModelDescriptions().at("SC Imported") == "A supplied synth description");
    assert(registry.refreshModelNamed<HeavyModel>("Phasor", "Core"));
    assert(!registry.getModelDescriptions().at("Phasor").empty());
    assert(HeavyModel::constructions == 0);
    // Named refreshes retain explicit mixer exclusions, but ordinary expensive
    // models are still registered without constructing a prototype.
    assert(registry.refreshModelNamed<ExternalModel>("External", "Core"));
    assert(registry.macroMixerBlocker("External") == "External test output");
    assert(HeavyModel::constructions == 1);
    assert(registry.refreshModelNamed<HeavyModel>("External", "Core"));
    assert(registry.macroMixerBlocker("External").empty());
    assert(HeavyModel::constructions == 1);
    json answer = {{"answer", "Use a Phasor."}, {"nodes", json::array()}, {"connections", json::array()}};
    assert(!rejected(answer));
    json plan = {{"answer", "Connect Phase to Phase."},
        {"nodes", {{{"id", "a"}, {"type", "Phasor"}}, {{"id", "b"}, {"type", "Oscillator"}}}},
        {"connections", {{{"from", "a"}, {"output", "Phase"}, {"to", "b"}, {"input", "Phase"}}}}};
    assert(!rejected(plan));
    auto bad = plan; bad["nodes"][1]["id"] = "a"; assert(rejected(bad));
    bad = plan; bad["nodes"][0]["type"] = "Imaginary"; assert(rejected(bad));
    bad = plan; bad["connections"][0]["to"] = "existing_node"; assert(rejected(bad));
    bad = plan; bad["connections"].push_back(bad["connections"][0]); assert(rejected(bad));
    bad = plan; bad["connections"][0]["from"] = "b"; assert(rejected(bad));
    bad = plan; bad["connections"].push_back({{"from", "b"}, {"output", "Output"}, {"to", "a"}, {"input", "BPM"}}); assert(rejected(bad));
    bad = plan; bad["connections"][0].erase("output"); assert(rejected(bad));
    bad = plan; bad["nodes"][0]["values"] = {{"BPM", 120}}; assert(rejected(bad));
    bad = plan; bad["nodes"] = json::array();
    for(int i = 0; i < 25; ++i) bad["nodes"].push_back({{"id", std::to_string(i)}, {"type", "Phasor"}});
    assert(rejected(bad));
    for(const auto& invalid : {json(), json::array(), json("text"), json::object()}) assert(rejected(invalid));
    bad = answer; bad["nodes"].push_back(nullptr); assert(rejected(bad));
    bad = answer; bad["answer"] = 42; assert(rejected(bad));
    const auto& catalog = ofxOceanodeNodeHelpCatalog();
    assert(catalog.size() >= 500);
    for(const auto& item : catalog.items()) {
        assert(!item.value().at("description").get<std::string>().empty());
        assert(item.value().at("parameters").is_array());
    }
    assert(!ofxOceanodeNodeHelpDescription("Preset Load Trigger").empty());
    assert(!ofxOceanodeNodeHelpDescription("SC ArpeggiatorBox").empty());
    assert(!ofxOceanodeNodeHelpDescription("Router f").empty());
    assert(!ofxOceanodeNodeHelpDescription("Buffer v_f").empty());
    assert(!ofxOceanodeNodeHelpDescription("Fixture Custom").empty());
    assert(ofxOceanodeNodeHelpDescription("Imaginary").empty());
    std::cout << "Help validation and " << catalog.size() << " catalog entries passed\n";
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix='oceanode-help-test-') as temp:
        work = Path(temp)
        # Catalog generation only needs OF's JSON alias, independent of its GUI runtime.
        (work / 'ofMain.h').write_text('#pragma once\n#include <nlohmann/json.hpp>\nusing ofJson = nlohmann::json;\nusing namespace std;\n')
        (work / 'ofJson.h').write_text('#pragma once\n#include "ofMain.h"\n')
        (work / 'ofxOceanodeNodeModel.h').write_text('#pragma once\n#include "ofMain.h"\n'
            'class ofxOceanodeNodeModel { public: virtual ~ofxOceanodeNodeModel() = default; '
            'virtual string macroMixerBlocker() const { return {}; } string nodeName() { return "SC Imported"; } string getDescription() { return "Node description"; } };\n')
        source = work / 'test.cpp'
        source.write_text(CASES)
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra',
                   '-I' + str(work), '-I' + str(ROOT / 'src'),
                   '-I' + str(of_root() / 'libs/json/include'), str(source), '-o', str(work / 'test')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'test')], check=True)

if __name__ == '__main__':
    main()
