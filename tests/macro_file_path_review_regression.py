#!/usr/bin/env python3
"""A file saved by a relative path and held by its absolute one is not an edit.

A node that embeds a sample saves its path relative (to the data folder, or to
the saved folder's samples) and keeps it absolute once loaded. The Macro
review compared the two texts and reported a change nobody made, so "Save
changes?" appeared on an untouched Macro. The review now compares what the
paths point at: the same file (present or missing) is one value; another
file is still a difference.

The same goes for a path inside a data folder shared between machines: saved
absolute on another machine, it is relative (or under this machine's home
folder) once loaded here, and is the same place.

Uses the headless OF fixture; does not launch the host application.
"""
import macro_mixer_regression as native
from new_preset_regression import block

CASES = r'''
int main(int argc, char** argv) {
    assert(argc == 3);
    namespace fs = std::filesystem;
    fs::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    const std::string folder = "Macros/Sonif/Macro_2";
    fs::create_directories(folder + "/samples");
    std::ofstream(folder + "/samples/a.wav") << "a";
    std::ofstream(folder + "/samples/b.wav") << "b";
    const std::string relative = folder + "/samples/a.wav";
    const std::string absolute = fs::absolute(relative).string();
    const std::string other = fs::absolute(folder + "/samples/b.wav").string();

    // The same file, written relative to the data folder and absolute.
    assert(sameSavedFile(relative, absolute, folder));
    assert(sameSavedFile(absolute, relative, folder));
    // ...and relative to the saved folder's samples, as an embedded sample is.
    assert(sameSavedFile("samples/a.wav", absolute, folder));
    // Another file is a change.
    assert(!sameSavedFile(relative, other, folder));
    assert(!sameSavedFile(relative, "", folder));
    // A file that is missing is still the same reference by either spelling,
    // and a different one from another missing file.
    assert(sameSavedFile(folder + "/samples/missing.wav", fs::absolute(folder + "/samples/missing.wav").string(), folder));
    assert(!sameSavedFile(folder + "/samples/missing.wav", fs::absolute(folder + "/samples/gone.wav").string(), folder));
    // Text that is no path is compared as text.
    assert(sameSavedFile("hello", "hello", folder));
    assert(!sameSavedFile("hello", "world", folder));
    assert(!sameSavedFile("a/b", "a/c", folder));

    auto changesBetween = [&](const ofJson& saved, const ofJson& current) {
        std::vector<GlobalMacroSaveChange> changes;
        compareJson(changes, "Macro", "SC GrainBox 1", "samplePath", withCurrentFileSpelling(saved, current, folder), current);
        return changes.size();
    };
    // The reported case: one field, the same sample.
    assert(changesBetween(relative, absolute) == 0);
    assert(changesBetween(relative, other) == 1); // a real change of sample still shows
    // Paths inside lists and objects (a node with several samples).
    assert(changesBetween(ofJson::array({relative, ""}), ofJson::array({absolute, ""})) == 0);
    assert(changesBetween(ofJson::array({relative, ""}), ofJson::array({other, ""})) == 1);
    assert(changesBetween({{"main", relative}, {"gain", 0.5}}, {{"main", absolute}, {"gain", 0.5}}) == 0);
    assert(changesBetween({{"main", relative}, {"gain", 0.5}}, {{"main", absolute}, {"gain", 0.7}}) == 1);
    // A folder in a data folder shared between machines: saved absolute on
    // another machine (missing here), it is relative or absolute once loaded.
    // (The cases above run without a data folder; these need one.)
    fs::create_directories("data/Supercollider/Samples/Kicks");
    ofEnableDataPath(); ofSetDataPathRoot(fs::absolute("data"));
    const std::string elsewhere = "/Users/nobody-here/Shared/data/Supercollider/Samples";
    const std::string here = ofToDataPath("Supercollider/Samples", true);
    assert(sameDataFolderPlace(elsewhere, "Supercollider/Samples"));
    assert(sameDataFolderPlace(elsewhere, here));
    assert(sameDataFolderPlace(here, "Supercollider/Samples")); // saved here before paths were relative
    assert(!sameDataFolderPlace(elsewhere, "Supercollider/Samples/Kicks")); // another folder is a change
    assert(!sameDataFolderPlace(elsewhere, ofToDataPath("Supercollider/Samples/Kicks", true)));
    assert(!sameDataFolderPlace("/Users/nobody-here/Documents/Samples", "Supercollider/Samples")); // outside any data folder
    assert(!sameDataFolderPlace("Supercollider", "Samples"));
    assert(changesBetween(elsewhere, "Supercollider/Samples") == 0);
    assert(changesBetween(elsewhere, "Supercollider/Samples/Kicks") == 1);
    ofDisableDataPath();
    // A NaN (a formula dividing by zero) is saved as null and is never equal
    // to itself: neither is a change, a number in its place is.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(sameSavedValue(ofJson::array({nan, 1.0}), ofJson::array({nan, 1.0})));
    assert(sameSavedValue(ofJson::array({nullptr, 1.0}), ofJson::array({nan, 1.0})));
    assert(!sameSavedValue(ofJson::array({nan, 1.0}), ofJson::array({0.0, 1.0})));
    assert(sameSavedValue({{"Output", ofJson::array({nan})}}, {{"Output", ofJson::array({nan})}}));
    assert(!sameSavedValue({{"Output", ofJson::array({nan})}}, {{"Output", ofJson::array({nan, nan})}}));
    assert(changesBetween(ofJson::array({nullptr, nullptr}), ofJson::array({nan, nan})) == 0);
    assert(changesBetween(ofJson::array({nullptr, nullptr}), ofJson::array({nan, 2.0})) == 1);
    // Values that are not strings are left alone.
    assert(changesBetween(1, 2) == 1);
    assert(changesBetween(nullptr, absolute) == 1);
    assert(changesBetween(nullptr, 1.5) == 1);
    std::cout << "PASS: the same file by two spellings is no change, present or missing; another file and other values still are\n";
}
'''


def main():
    source = (native.ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    helpers = '#include "GlobalMacroSaveReview.h"\n#include <fstream>\n#include <limits>\n'
    helpers += "\n".join(block(source, signature) for signature in (
        "bool sameSavedValue(", "ofJson readJson(", "ofJson field(", "std::string preview(",
        "std::string fullValue(", "void addChange(", "void compareJson(",
        "std::vector<std::filesystem::path> fileCandidates(", "bool sameSavedFile(",
        "std::vector<std::string> placesInDataFolder(", "bool sameDataFolderPlace(",
        "ofJson withCurrentFileSpelling(",
    ))
    native.CASES = helpers + CASES
    native.main()


if __name__ == "__main__":
    main()
