//
//  ofxOceanodeContainerPaths.cpp
//  ofxOceanode
//
//  Parameter and timeline paths, canvas lookup and showing nodes in a canvas.
//  Split out of ofxOceanodeContainer.cpp; bodies are unchanged.
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeAutomationExtension.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "Nodes/MacroSnapshotSystem.h"
#include "Nodes/MacroRouterValueDispatch.h"
#include "portal.h"
#include "portalSelector.h"
#include "imgui.h"
#include <cmath>
#include <cfloat>
#include <deque>
#include <filesystem>
#include <stdexcept>
#include "PresetSaveCleanup.h"

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiClock.h"
#include "ofxOceanodeMidiBinding.h"
#include "ofxOceanodeMidiKeyboardBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


namespace {
const char* const kTimelineCanvasSeparator = "::";
bool isRootCanvasID(const std::string& canvasID){
    return canvasID.empty() || canvasID == "Canvas" || canvasID == "0";
}
}


std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter) const
{
    auto* model = parameter.getNodeModel();
    const std::string local = (model == nullptr)
        ? parameter.getEscapedName()
        : model->getParameterGroup().getEscapedName() + "/" + parameter.getEscapedName();
    const std::string canvasID = model == nullptr ? std::string() : model->getParents();
    if(isRootCanvasID(canvasID)) return local;
    return canvasID + kTimelineCanvasSeparator + local;
}

void ofxOceanodeContainer::collectMovedTimelinePaths(ofxOceanodeNode& node, const std::string& oldNodePath,
                                                     const std::string& oldMacroCanvas,
                                                     std::map<std::string, std::string>& oldToNew) const
{
    for(auto& abstract : node.getParameters())
        if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get()))
            oldToNew[oldNodePath + parameter->getEscapedName()] = getTimelineParameterPath(*parameter);
    auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel());
    if(macro == nullptr || macro->getContainer() == nullptr || oldMacroCanvas.empty()) return;
    // Everything inside a moved macro moved with it: swap its canvas id back.
    const std::string newMacroCanvas = macro->getContainer()->getCanvasID();
    std::function<void(ofxOceanodeContainer&)> collect = [&](ofxOceanodeContainer& inner) {
        for(auto* innerNode : inner.getAllModules()) {
            for(auto& abstract : innerNode->getParameters()) {
                auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
                if(parameter == nullptr) continue;
                const std::string now = getTimelineParameterPath(*parameter);
                if(now.compare(0, newMacroCanvas.size(), newMacroCanvas) == 0)
                    oldToNew[oldMacroCanvas + now.substr(newMacroCanvas.size())] = now;
            }
            if(auto* nested = dynamic_cast<ofxOceanodeNodeMacro*>(&innerNode->getNodeModel()))
                if(nested->getContainer()) collect(*nested->getContainer());
        }
    };
    collect(*macro->getContainer());
}

std::string ofxOceanodeContainer::timelineCanvasPrefix(const std::string& canvasID)
{
    return isRootCanvasID(canvasID) ? std::string() : canvasID + kTimelineCanvasSeparator;
}

std::string ofxOceanodeContainer::timelineMacroCanvas(const std::string& parentCanvasID, int macroNumber)
{
    // A macro's canvas id is "Macro <its number>" under its parent's (see ofxOceanodeNodeMacro::setup).
    return (isRootCanvasID(parentCanvasID) ? std::string() : parentCanvasID + " / ") + "Macro " + ofToString(macroNumber);
}

std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter,
                                                       const ofxOceanodeContainer& base) const
{
    const std::string absolute = getTimelineParameterPath(parameter);
    if(isRootCanvasID(base.getCanvasID())) return absolute;
    // Canvas ids nest as "Macro 1 / Macro 2": strip the base's own id.
    const std::string& prefix = base.getCanvasID();
    const size_t separator = absolute.find(kTimelineCanvasSeparator);
    if(separator == std::string::npos) return std::string(); // at the root: outside base
    const std::string canvasID = absolute.substr(0, separator);
    const std::string local = absolute.substr(separator + std::char_traits<char>::length(kTimelineCanvasSeparator));
    if(canvasID == prefix) return local;
    const std::string nested = prefix + " / ";
    if(canvasID.compare(0, nested.size(), nested) == 0)
        return canvasID.substr(nested.size()) + kTimelineCanvasSeparator + local;
    return std::string();
}

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findTimelineParameter(const std::string& parameterPath) const
{
    const size_t separator = parameterPath.find(kTimelineCanvasSeparator);
    if(separator == std::string::npos) return findParameter(parameterPath);
    const std::string canvasID = parameterPath.substr(0, separator);
    const std::string local = parameterPath.substr(separator + std::char_traits<char>::length(kTimelineCanvasSeparator));
    if(isRootCanvasID(canvasID)) return findParameter(local);
    // Walk down the macro levels. Unlike getContainerForCanvasID this neither
    // falls back to the root nor logs: a binding to a deleted macro is simply
    // missing (and it is looked up every frame).
    // Paths are relative to this container: its own canvas id prefixes
    // every macro below it (nothing for the root).
    const std::vector<std::string> levels = ofSplitString(canvasID, " / ");
    ofxOceanodeContainer* current = const_cast<ofxOceanodeContainer*>(this);
    std::string accumulated = isRootCanvasID(getCanvasID()) ? std::string() : getCanvasID();
    for(size_t i = 0; i < levels.size(); ++i) {
        accumulated = accumulated.empty() ? levels[i] : accumulated + " / " + levels[i];
        ofxOceanodeContainer* next = nullptr;
        for(auto* node : current->getAllModules()) {
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                auto macroContainer = macro->getContainer();
                if(macroContainer != nullptr && macroContainer->getCanvasID() == accumulated) {
                    next = macroContainer.get();
                    break;
                }
            }
        }
        if(next == nullptr) return nullptr;
        current = next;
    }
    return current->findParameter(local);
}

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findParameter(const std::string& parameterPath) const
{
    auto resolved = const_cast<ofxOceanodeContainer*>(this)->resolveParameterFromPath(parameterPath, "");
    return resolved.parameter;
}

ofxOceanodeContainer::ParsedParameterPath ofxOceanodeContainer::parseParameterPath(
    const std::string& path) {
    
    ParsedParameterPath result;
    result.isValid = false;
    
    size_t slashPos = path.find('/');
    if(slashPos == std::string::npos) {
        return result;
    }
    
    result.groupName = path.substr(0, slashPos);
    result.paramName = path.substr(slashPos + 1);
    result.isValid = true;
    
    return result;
}

ofxOceanodeContainer::ResolvedParameter ofxOceanodeContainer::resolveParameterFromPath(
    const std::string& paramPath,
    const std::string& canvasID) {
    
    // Parse the path
    auto parsed = parseParameterPath(paramPath);
    if(!parsed.isValid) {
        return ResolvedParameter();
    }
    
    ofxOceanodeContainer* targetContainer = getContainerForCanvasID(canvasID);
    if(targetContainer == nullptr) targetContainer = this;
    
    // Get all nodes from the target container
    vector<ofxOceanodeNode*> allNodes = targetContainer->getAllModules();
    
    // Search for matching parameter
    for(auto* node : allNodes) {
        ofParameterGroup& params = node->getParameters();
        const std::string groupEscapedName = params.getEscapedName();
        
        // Check each parameter in the node
        for(int i = 0; i < params.size(); i++) {
            ofAbstractParameter& absParam = params.get(i);
            
            // Try to cast to oceanode parameter
            auto* oceanodeParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&absParam);
            if(oceanodeParam != nullptr) {
                const bool groupMatchesDirectly = (groupEscapedName == parsed.groupName);

                bool groupMatchesHierarchy = false;
                vector<string> hierarchyNames = oceanodeParam->getGroupHierarchyNames();
                if(!hierarchyNames.empty() && hierarchyNames.front() == parsed.groupName) {
                    groupMatchesHierarchy = true;
                }

                if(groupMatchesDirectly || groupMatchesHierarchy) {
                    if(absParam.getName() == parsed.paramName || absParam.getEscapedName() == parsed.paramName) {
                        return ResolvedParameter(oceanodeParam, node);
                    }
                }
            }
        }
    }
    
    return ResolvedParameter();
}

ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(const std::string& canvasID)
{
    return const_cast<ofxOceanodeContainer*>(static_cast<const ofxOceanodeContainer*>(this)->getContainerForCanvasID(canvasID));
}

const ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(const std::string& canvasID) const
{
    if(canvasID.empty() || canvasID == "Canvas" || canvasID == "0") return this;

    vector<string> canvasLevels = ofSplitString(canvasID, " / ");
    const ofxOceanodeContainer* currentContainer = this;
    string accumulatedPath;

    for(int levelIndex = 0; levelIndex < (int)canvasLevels.size(); levelIndex++) {
        if(levelIndex == 0) accumulatedPath = canvasLevels[levelIndex];
        else accumulatedPath += " / " + canvasLevels[levelIndex];

        vector<ofxOceanodeNode*> nodesAtLevel = const_cast<ofxOceanodeContainer*>(currentContainer)->getAllModules();
        bool levelFound = false;
        for(auto* node : nodesAtLevel) {
            if(ofxOceanodeNodeMacro* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                auto macroContainer = macro->getContainer();
                if(macroContainer != nullptr && macroContainer->getCanvasID() == accumulatedPath) {
                    currentContainer = macroContainer.get();
                    levelFound = true;
                    break;
                }
            }
        }

        if(!levelFound) {
            ofLogWarning("ofxOceanodeContainer") << "Could not find macro with path: " << accumulatedPath
                                                 << " in hierarchy: " << canvasID;
            return this;
        }
    }

    return currentContainer;
}

bool ofxOceanodeContainer::showParameterInCanvas(ofxOceanodeAbstractParameter& parameter)
{
    auto* node = getNodeFromParameter(parameter);
    if(node == nullptr) return false;
    return showNodeInCanvas(*node);
}

bool ofxOceanodeContainer::showNodeInCanvas(ofxOceanodeNode& targetNode, const vector<ofxOceanodeNode*>& alsoSelect, bool takeFocus, bool fitView)
{
    auto* rootContainer = ofxOceanodeShared::getRootContainer();
    auto* rootCanvas = ofxOceanodeShared::getRootCanvas();
    if(rootContainer == nullptr || rootCanvas == nullptr) return false;

    struct NodeLocation {
        ofxOceanodeNode* node = nullptr;
        ofxOceanodeCanvas* canvas = nullptr;
        ofxOceanodeNodeMacro* macro = nullptr;
        ofxOceanodeContainer* container = nullptr;
    };

    NodeLocation location;
    std::function<void(ofxOceanodeContainer*, ofxOceanodeCanvas*, ofxOceanodeNodeMacro*)> findNode =
        [&](ofxOceanodeContainer* currentContainer, ofxOceanodeCanvas* currentCanvas, ofxOceanodeNodeMacro* currentMacro){
            if(currentContainer == nullptr || currentCanvas == nullptr || location.node != nullptr) return;

            for(auto* node : currentContainer->getAllModules()){
                if(node == nullptr || location.node != nullptr) continue;

                if(node == &targetNode){
                    location.node = node;
                    location.canvas = currentCanvas;
                    location.macro = currentMacro;
                    location.container = currentContainer;
                    return;
                }

                if(auto* nestedMacro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
                    findNode(nestedMacro->getContainer().get(), nestedMacro->getCanvas(), nestedMacro);
                }
            }
        };

    findNode(rootContainer, rootCanvas, nullptr);
    if(location.node == nullptr || location.canvas == nullptr || location.container == nullptr) return false;

    auto* originalMacro = location.macro;
    if(location.macro != nullptr){
        // Instances of a global Macro share one tab: go to the matching node there.
        const auto shown = location.macro->windowTargetFor(location.node);
        location.macro = shown.macro;
        location.container = shown.macro->getContainer().get();
        if(shown.node == nullptr){
            shown.macro->activateWindow(takeFocus);
            return true;
        }
        location.node = shown.node;
    }

    for(auto& pair : location.container->getParameterGroupNodesMap()){
        pair.second->getNodeGui().setSelected(false);
    }
    location.container->deselectAllComments();
    location.node->getNodeGui().setSelected(true);
    vector<ofxOceanodeNode*> selectedNodes{location.node};
    for(auto* other : alsoSelect){
        // The extra nodes live beside the target, so they map to the same shown Macro.
        auto* shownNode = originalMacro != nullptr ? originalMacro->windowTargetFor(other).node : other;
        if(shownNode == nullptr) continue;
        shownNode->getNodeGui().setSelected(true);
        selectedNodes.push_back(shownNode);
    }

    if(location.macro != nullptr){
        location.macro->activateWindow(takeFocus);
        location.canvas = location.macro->getCanvas();
    }else if(takeFocus){
        location.canvas->requestFocus();
    }

    if(takeFocus) location.canvas->bringOnTop();
    ofxOceanodeShared::setActiveCanvasUniqueID(location.canvas->getUniqueID());
    ofxOceanodeShared::nodeSelectedInCanvas(location.node);
    const int centerDelay = location.macro != nullptr ? 2 : 1;
    if(selectedNodes.size() == 1 && !fitView){
        location.canvas->requestCenterOnNode(location.node, centerDelay);
    }else{
        // Several nodes: the middle of the box that holds all of them.
        float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
        for(auto* member : selectedNodes){
            const auto position = member->getNodeGui().getPosition();
            auto rectangle = member->getNodeGui().getRectangle();
            left = std::min(left, position.x);
            top = std::min(top, position.y);
            right = std::max(right, position.x + rectangle.getWidth());
            bottom = std::max(bottom, position.y + rectangle.getHeight());
        }
        location.canvas->requestCenterOnPoint(glm::vec2((left + right) / 2.0f, (top + bottom) / 2.0f), centerDelay,
                                              glm::vec2(right - left, bottom - top));
    }
    return true;
}

bool ofxOceanodeContainer::showNodesInCanvas(const vector<ofxOceanodeNode*>& nodes, bool takeFocus)
{
    if(nodes.empty()) return false;
    const vector<ofxOceanodeNode*> others(nodes.begin() + 1, nodes.end());
    // A group always fits from 100%, even when it holds a single node.
    return showNodeInCanvas(*nodes.front(), others, takeFocus, true);
}
