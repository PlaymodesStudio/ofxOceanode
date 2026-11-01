//
//  ofxOceanodeContainerClipboard.cpp
//  ofxOceanode
//
//  Copy, cut and paste of nodes with their connections.
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


bool ofxOceanodeContainer::loadClipboardModulesAndConnections(glm::vec2 referencePosition, bool allowOutsideInputs){
    of::filesystem::path presetFolderPath = of::filesystem::current_path() / "clipboardPreset";
    of::filesystem::path tempLoadFolderPath = presetFolderPath / "tempLoad";
    ofLog()<<"Load Clipboard Preset";
    
    map<string, map<int, int>> moduleConverter;
    vector<ofxOceanodeNode*> newCreatedNodes;
    
    ofJson json = ofLoadJson(presetFolderPath / "modules.json");
    for (ofJson::iterator nodeType = json.begin(); nodeType != json.end(); ++nodeType) {
        for(ofJson::iterator nodeId = nodeType.value().begin(); nodeId != nodeType.value().end(); ++nodeId){
            ofLog() << nodeType.key() << " " << nodeId.key();
            string escapedNodeName = nodeType.key();
            ofStringReplace(escapedNodeName, " ", "_");
            ofJson tempJson = ofLoadJson(presetFolderPath / (escapedNodeName + "_" + ofToString(ofToInt(nodeId.key())) + ".json"));
            if(!tempJson.is_object() || tempJson.empty()) return false;
            auto model = registry->create(nodeType.key());
            if(!model){
                ofLogError("Clipboard") << "Cannot create " << nodeType.key();
                return false;
            }
            if(auto* router = dynamic_cast<abstractRouter*>(model.get()))
                router->abstractRouter::loadBeforeConnections(tempJson);
            auto* node = &createNode(std::move(model));
            if(auto* router = dynamic_cast<abstractRouter*>(&node->getNodeModel()))
                tempJson["Name"] = router->getNameParam().get();
            node->getNodeGui().setPosition(glm::vec2(nodeId.value()[0], nodeId.value()[1]) + referencePosition);
            node->getNodeGui().setSelected(true);
            newCreatedNodes.push_back(node);
            int newNodeId = node->getNodeModel().getNumIdentifier();
            moduleConverter[escapedNodeName][ofToInt(nodeId.key())] = newNodeId;
            if(!ofSaveJson(tempLoadFolderPath / (escapedNodeName + "_" + ofToString(newNodeId) + ".json"), tempJson)) return false;
        }
    }
	
	//Change Local macro names to match duplicated
	ofDirectory dir;
	dir.open(presetFolderPath);
	for(auto &f : dir.getFiles()){
		if(ofStringTimesInString(f.getFileName(), "Macro") && f.isDirectory()){
			string sourceMappedModuleId = ofSplitString(f.getFileName(), "_").back();
            ofDirectory dir2(presetFolderPath / f.getFileName());
            dir2.copyTo(tempLoadFolderPath / ("Macro_" + ofToString(moduleConverter["Macro"][ofToInt(sourceMappedModuleId)])), true);
		}
	}


    for(auto node : newCreatedNodes){
        node->loadPresetBeforeConnections(tempLoadFolderPath);
    }
    
    
    json.clear();
    json = ofLoadJson(presetFolderPath / "connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    if(allowOutsideInputs || sinkParameter.value()){
                        string sourceMappedModule = sourceModule.key();
                        if(sinkParameter.value()){
                            string sourceMappedModuleId = ofSplitString(sourceMappedModule, "_").back();
                            sourceMappedModule.erase(sourceMappedModule.rfind(sourceMappedModuleId)-1);
                            sourceMappedModule += "_" + ofToString(moduleConverter[sourceMappedModule][ofToInt(sourceMappedModuleId)]);
                        }
                        
                        string sinkMappedModule = sinkModule.key();
                        string sinkMappedModuleId = ofSplitString(sinkMappedModule, "_").back();
                        sinkMappedModule.erase(sinkMappedModule.rfind(sinkMappedModuleId)-1);
                        sinkMappedModule += "_" + ofToString(moduleConverter[sinkMappedModule][ofToInt(sinkMappedModuleId)]);
                        
                        createConnectionFromInfo(sourceMappedModule, sourceParameter.key(), sinkMappedModule, sinkParameter.key());
                    }
                }
            }
        }
    }
//
    for(auto node : newCreatedNodes){
        node->loadPreset(tempLoadFolderPath);
    }
    
    for(auto node : newCreatedNodes){
        node->presetHasLoaded();
    }
    
    // Load comments
    if(of::filesystem::exists(presetFolderPath / "comments.json")){
        json.clear();
        json = ofLoadJson(presetFolderPath / "comments.json");
        int numComments = json["NumComments"];
        for(int i = 0; i < numComments; i++){
            auto &commentJson = json[ofToString(i)];
            ofxOceanodeComment comment;
            comment.text = commentJson["text"];
            comment.position = glm::vec2(commentJson["position"][0], commentJson["position"][1]) + referencePosition;
            comment.size = glm::vec2(commentJson["size"][0], commentJson["size"][1]);
            comment.color = ofFloatColor(commentJson["color"][0], commentJson["color"][1], commentJson["color"][2], commentJson["color"][3]);
            comment.textColor = ofFloatColor(commentJson["textColor"][0], commentJson["textColor"][1], commentJson["textColor"][2], commentJson["textColor"][3]);
            comment.selected = true; // Select newly pasted comments
            comments.push_back(comment);
        }
    }
    
    ofDirectory::removeDirectory(tempLoadFolderPath, true);
	
    return true;
}

void ofxOceanodeContainer::saveClipboardModulesAndConnections(vector<ofxOceanodeNode*> nodes, glm::vec2 referencePosition){
    of::filesystem::path presetFolderPath = of::filesystem::current_path() / "clipboardPreset";
    ofDirectory::removeDirectory(presetFolderPath, true, false);
    ofLog()<< "Save Clipboard Preset";
    
    vector<string> nodeAsParentNames;
    
    ofJson json;
    for(auto &node : nodes){
        nodeAsParentNames.push_back(node->getParameters().getEscapedName());
        glm::vec2 pos(0,0);
        pos = node->getNodeGui().getPosition();
        json[node->getNodeModel().nodeName()][ofToString(node->getNodeModel().getNumIdentifier(), 2, '0')] = {pos.x - referencePosition.x, pos.y - referencePosition.y};
    }
    ofSavePrettyJson(presetFolderPath / "modules.json", json);
    
    //TODO: search for connections in each parameter of the node? instead of searching all connections? try performance
    json.clear();
    for(auto &connection : connections){
        if(!connection->getIsPersistent()){
            string sourceName = connection->getSourceParameter().getName();
            string sourceParentName = connection->getSourceParameter().getGroupHierarchyNames()[0];
            string sinkName = connection->getSinkParameter().getName();
            string sinkParentName = connection->getSinkParameter().getGroupHierarchyNames()[0];
            bool sourceIsInModuleList = std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sourceParentName) != nodeAsParentNames.end();
            bool sinkIsInModuleList = std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sinkParentName) != nodeAsParentNames.end();
            if(sinkIsInModuleList){
                json[sourceParentName][sourceName][sinkParentName][sinkName] = sourceIsInModuleList;
            }
        }
    }
    
    ofSavePrettyJson(presetFolderPath / "connections.json", json);
    
    for(auto &node : nodes){
        auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
        if(macro && pendingCanvasEdit && !pendingCanvasEdit->beforeAux.is_null()){
            // Structural moves need the live contents of library macros too,
            // including edits that have not been saved to the library definition.
            auto state = node->saveParametersToJson(false, true);
            node->saveInspectorParametersToJson(state);
            node->getNodeModel().presetSave(state);
            macro->saveCanvasUndoState(state, presetFolderPath.string());
            node->saveNodeSettings(state);
            state["expanded"] = node->getNodeGui().getExpanded();
            string type = node->getNodeModel().nodeName();
            ofStringReplace(type, " ", "_");
            if(!ofSaveJson(presetFolderPath / (type + "_" + ofToString(node->getNodeModel().getNumIdentifier()) + ".json"), state))
                throw std::runtime_error("Cannot stage current macro contents");
        }else node->savePreset(presetFolderPath, true);
    }
    
    // Save selected comments
    json.clear();
    json["NumComments"] = 0;
    int commentIndex = 0;
    for(auto &c : comments){
        if(c.selected){
            auto &commentJson = json[ofToString(commentIndex)];
            commentJson["text"] = c.text;
            commentJson["position"] = {c.position.x - referencePosition.x, c.position.y - referencePosition.y};
            commentJson["size"] = {c.size.x, c.size.y};
            commentJson["color"] = {c.color.r, c.color.g, c.color.b, c.color.a};
            commentJson["textColor"] = {c.textColor.r, c.textColor.g, c.textColor.b, c.textColor.a};
            commentIndex++;
        }
    }
    json["NumComments"] = commentIndex;
    ofSavePrettyJson(presetFolderPath / "comments.json", json);
}

bool ofxOceanodeContainer::copySelectedModulesWithConnections(){
    vector<ofxOceanodeNode*> modulesToCopy = getSelectedModules();
    glm::vec2 minPosition(FLT_MAX, FLT_MAX);
    for(auto &node : modulesToCopy){
        minPosition = glm::vec2(min(node->getNodeGui().getPosition().x, minPosition.x), min(node->getNodeGui().getPosition().y, minPosition.y));
    }
    if(modulesToCopy.size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCopy, minPosition);
    return true;
}

bool ofxOceanodeContainer::cutSelectedModulesWithConnections(){
    vector<ofxOceanodeNode*> modulesToCut = getSelectedModules();
    glm::vec2 minPosition(FLT_MAX, FLT_MAX);
    for(auto &node : modulesToCut){
        minPosition = glm::vec2(min(node->getNodeGui().getPosition().x, minPosition.x), min(node->getNodeGui().getPosition().y, minPosition.y));
    }
    
    // Also consider selected comments for minimum position
    for(auto &c : comments){
        if(c.selected){
            minPosition = glm::vec2(min(c.position.x, minPosition.x), min(c.position.y, minPosition.y));
        }
    }
    
    if(modulesToCut.size() == 0 && getSelectedCommentIndices().size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCut, minPosition);
    for(auto &m : modulesToCut) m->deleteSelf();
    
    // Also delete selected comments
    auto selectedCommentIndices = getSelectedCommentIndices();
    // Delete in reverse order to maintain correct indices
    for(int i = selectedCommentIndices.size() - 1; i >= 0; i--){
        comments.erase(comments.begin() + selectedCommentIndices[i]);
    }
    return true;
}

bool ofxOceanodeContainer::pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs){
    ofxOceanodeShared::startedLoadingPreset(ofxOceanodePresetLoadType_ClipboardPaste);
    bool b_sucess = loadClipboardModulesAndConnections(position, allowOutsideInputs);
    ofxOceanodeShared::finishedLoadingPreset();
    return b_sucess;
}
