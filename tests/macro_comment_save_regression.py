#!/usr/bin/env python3
"""Check comment save/review round trips using native persistence and JSON comparison.

Uses the headless OF fixture; does not launch the host application.
"""
import macro_mixer_regression as native
from new_preset_regression import block

CASES = r'''
int main(int argc, char** argv) {
    assert(argc == 3);
    std::filesystem::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    auto window = std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = std::make_shared<ofxOceanodeTypesRegistry>();
    auto transport = std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer container(registry, types, transport);
    container.setCanvasID("CommentReview");
    const std::string savedFolder = "Macros/CommentReview", location = "CommentReview";
    auto review = [&]() {
        std::vector<GlobalMacroSaveChange> changes;
        COMPARE_COMMENTS
        return changes;
    };
    assert(container.savePreset(savedFolder));
    assert(review().empty());
    auto& comment = container.getComments().emplace_back();
    comment.text = "INPUT";
    comment.color = ofFloatColor(.2f, .45f, .13f, 1.f);
    comment.textColor = ofFloatColor(1.f, 1.f, 1.f, 1.f);
    assert(!review().empty());
    assert(container.savePreset(savedFolder));
    assert(review().empty()); // The reported second-save failure.
    assert(container.savePreset(savedFolder));
    assert(review().empty());
    for(auto* alpha : {&comment.color.a, &comment.textColor.a}) {
        *alpha = .25f;
        assert(!review().empty()); // Actual transparency edits must still prompt.
        *alpha = 1.f;
        assert(review().empty()); // Undoing an edit is clean again.
        *alpha = .5f;
        assert(container.savePreset(savedFolder));
        assert(review().empty());
    }
    container.loadPreset_loadComments(savedFolder);
    assert(review().empty());
    // Legacy RGB-only files load as opaque, without an upgrade-only prompt.
    auto legacy = ofLoadJson(savedFolder + "/comments.json");
    legacy["Comments"][0]["Color"].erase("A");
    legacy["Comments"][0]["TextColor"].erase("A");
    ofSaveJson(savedFolder + "/comments.json", legacy);
    container.loadPreset_loadComments(savedFolder);
    assert(container.getComments()[0].color.a == 1.f);
    assert(container.getComments()[0].textColor.a == 1.f);
    assert(review().empty());
    container.getComments()[0].textColor.a = .3f;
    assert(!review().empty());
    assert(container.savePreset(savedFolder));
    assert(review().empty());
    container.getComments()[0].text += " CHANGED";
    assert(!review().empty());
    container.getComments().clear();
    assert(!review().empty());
    assert(container.savePreset(savedFolder));
    assert(review().empty());
    std::cout << "PASS: repeated comment saves, alpha edits/undo, reload, legacy RGB, text edits and deletion\n";
}
'''


def main():
    source = (native.ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    helpers = '#include "GlobalMacroSaveReview.h"\n'
    helpers += "\n".join(block(source, signature) for signature in (
        "bool sameSavedValue(", "ofJson readJson(", "ofJson field(", "std::string preview(",
        "std::string fullValue(", "void addChange(", "void compareJson(",
        "ofJson currentComments(", "ofJson normalizedSavedComments(",
    ))
    start = source.index('    compareJson(changes, location, "Comments", "Comments",')
    end = source.index('    compareJson(changes, location, "Custom GUI",', start)
    native.CASES = helpers + CASES.replace("COMPARE_COMMENTS", source[start:end])
    native.main()


if __name__ == "__main__":
    main()
