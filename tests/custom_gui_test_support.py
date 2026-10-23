"""Link the real optional Custom GUI addon into the native headless fixture."""
from pathlib import Path
import os
from new_preset_regression import block


def enable_custom_gui(native, graph_helpers=True):
    addon = Path(os.environ.get("OCEANODE_CUSTOMGUI_ROOT", native.ROOT.parent / "ofxOceanodeCustomGui"))
    if not (addon / "src/ofxOceanodeCustomGui.h").exists():
        raise RuntimeError("Install ofxOceanodeCustomGui beside ofxOceanode, or set OCEANODE_CUSTOMGUI_ROOT")
    native.PRELUDE += '#include "ofxOceanodeCustomGui.h"\n'
    native.PRELUDE = native.PRELUDE.replace(
        'bool ofxOceanodeContainer::customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter&) const { return publishedInCustomGui; }',
        'bool ofxOceanodeContainer::customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter& p) const { return getCustomGuiExtension() && getCustomGuiExtension()->containsParameter(p); }')
    # Navigation and the root Scope renderer are outside this headless fixture.
    native.SUPPORT += r'''
bool ofxOceanodeContainer::showParameterInCanvas(ofxOceanodeAbstractParameter&) { assert(false); return false; }
bool ofxOceanodeScope::canScope(const ofxOceanodeAbstractParameter*) const { return false; }
bool ofxOceanodeScope::drawParameter(ofxOceanodeAbstractParameter*, ImVec2) const { assert(false); return false; }
'''
    source = (native.ROOT/'src/Managers/ofxOceanodeContainer.cpp').read_text()
    signatures = [
        'ofxOceanodeAbstractParameter* ofxOceanodeContainer::findParameter(',
        'ofxOceanodeContainer::ResolvedParameter ofxOceanodeContainer::resolveParameterFromPath(',
        'ofxOceanodeContainer::ParsedParameterPath ofxOceanodeContainer::parseParameterPath(',
        'ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(',
        'const ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(',
        'void ofxOceanodeContainer::recordUserEditedValues(']
    if graph_helpers:
        signatures.append('ofxOceanodeNode* ofxOceanodeContainer::getNodeFromParameter(')
    native.SUPPORT += '\nnamespace {\n' + block(source, 'std::string userEditedValueKey(') + '\n}\n'
    for signature in signatures:
        native.SUPPORT += block(source, signature) + '\n'
    return [str(p) for p in (addon/'src').rglob('*.cpp')], [addon/'src']
