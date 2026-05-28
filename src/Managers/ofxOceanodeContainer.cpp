//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "CustomGui/ofxOceanodeCustomGuiPanel.h"
#include "Nodes/MacroSnapshotSystem.h"
#include "Nodes/MacroRouterValueDispatch.h"
#include "imgui.h"

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry, shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry, shared_ptr<ofxOceanodeTransport> _transport) : registry(_registry), typesRegistry(_typesRegistry), transport(_transport){
    if(registry == nullptr) registry = make_shared<ofxOceanodeNodeRegistry>();
    if(typesRegistry == nullptr) typesRegistry = make_shared<ofxOceanodeTypesRegistry>();
    if(transport == nullptr) transport = make_shared<ofxOceanodeTransport>();
    transformationMatrix = glm::mat4(1.0);
    bpm = 120;
    phase = 0;
    transport->setBpm(bpm);
    
#ifdef OFXOCEANODE_USE_MIDI
    ofxMidiIn* midiIn = new ofxMidiIn();
    midiInPortList = midiIn->getInPortList();
    delete midiIn;
    for(auto port : midiInPortList){
        midiIns[port].openPort(port);
    }
    
    
    ofxMidiOut* midiOut = new ofxMidiOut();
    midiOutPortList = midiOut->getOutPortList();
    delete midiOut;
    for(auto port : midiOutPortList){
        midiOuts[port].openPort(port);
    }
    isListeningMidi = false;
#endif
    
    // Note: Scope change callback is now set per-preset in loadPreset()
    // to ensure correct preset path is captured
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    clearContainer();
}

void ofxOceanodeContainer::clearContainer(){
    // Clear scope callback first to prevent auto-saves triggered by parameter
    // destructors during teardown (app exit or preset switching)
    ofxOceanodeScope::getInstance()->setScopeChangedCallback(nullptr);
    
    connections.clear();
    customGuiPanels.clear();
    customGuiPanelsData.clear();
    customGuiSnapshotBanks.clear();
    
    std::vector<shared_ptr<ofxOceanodeNode>> toDelete;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            toDelete.push_back(node.second);
        }
    }
    for(auto td : toDelete) td->deleteSelf();
    
    dynamicNodes.clear();
    persistentNodes.clear();
}

void ofxOceanodeContainer::update(){
    ofEventArgs args;
#ifdef OFXOCEANODE_USE_MIDI
    for(auto &paramBinds : midiBindings){
        for(auto &bind : paramBinds.second){
            bind->update();
        }
    }
    for(auto &paramBinds : persistentMidiBindings){
        for(auto &bind : paramBinds.second){
            bind->update();
        }
    }
#endif
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->update(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->update(args);
        }
    }
}

void ofxOceanodeContainer::draw(){
    ofEventArgs args;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->draw(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->draw(args);
        }
    }

    for(auto& panel : customGuiPanels){
        if(panel) panel->draw();
    }
    if(!pendingDeletedCustomGuiPanelId.empty()){
        std::string panelId = pendingDeletedCustomGuiPanelId;
        pendingDeletedCustomGuiPanelId.clear();
        deleteCustomGuiPanel(panelId);
    }
}

void ofxOceanodeContainer::activate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(true);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(true);
        }
    }
}

void ofxOceanodeContainer::deactivate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(false);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(false);
        }
    }
}

ofxOceanodeNode* ofxOceanodeContainer::createNodeFromName(string name, int identifier, bool isPersistent){
    unique_ptr<ofxOceanodeNodeModel> type = registry->create(name);
    
    if (type)
    {
        auto &node =  createNode(std::move(type), identifier, isPersistent);
        return &node;
    }
    return nullptr;
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier, bool isPersistent, string additionalInfo){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    int toBeCreatedId = identifier;
    string nodeToBeCreatedName = nodeModel->nodeName();
    if(identifier == -1){
        int lastId = 1;
        while (dynamicNodes[nodeToBeCreatedName].count(lastId) != 0 || persistentNodes[nodeToBeCreatedName].count(lastId) != 0) lastId++;
        toBeCreatedId = lastId;
    }
    nodeModel->setNumIdentifier(toBeCreatedId);
    nodeModel->setContainer(this);
    auto node = make_shared<ofxOceanodeNode>(move(nodeModel));
	node->setup(additionalInfo);
	auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
#ifdef OFXOCEANODE_USE_MIDI
        nodeGui->setIsListeningMidi(isListeningMidi);
#endif
	node->setGui(std::move(nodeGui));
    node->setBpm(bpm);
    node->setIsPersistent(isPersistent);
    
    auto nodePtr = node.get();
    collection[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    parameterGroupNodesMap[nodePtr->getParameters().getEscapedName()] = nodePtr;
    
    //Interaction listeners
    destroyNodeListeners.push(nodePtr->deleteModule.newListener([this, nodeToBeCreatedName, toBeCreatedId, isPersistent](){
#ifdef OFXOCEANODE_USE_MIDI
        if(!isPersistent){
			for(int i = 0 ; i < dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().size(); i++){
				auto &p = dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().get(i);
                while(removeLastMidiBinding(static_cast<ofxOceanodeAbstractParameter &>(p)));
            }
        }
#endif
        
        if(!isPersistent){
            //Delete Map
            parameterGroupNodesMap.erase(dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().getEscapedName());
            dynamicNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }else{
            //Delete Map
            parameterGroupNodesMap.erase(persistentNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().getEscapedName());
            persistentNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }
    }));
    
    //Used in Macro
    newNodeCreated.notify(this, nodePtr);
    return *nodePtr;
}

bool ofxOceanodeContainer::loadPreset(string presetFolderPath){
    ofLog()<<"Load Preset " << presetFolderPath;
    customGuiStoragePath = presetFolderPath;
    
    // Disable scope auto-save during preset loading to prevent saving empty scope
    // when nodes are deleted
    ofxOceanodeScope::getInstance()->setScopeChangedCallback(nullptr);
    
    loadPreset_presetWillBeLoaded();

    loadPreset_loadNodes(presetFolderPath);
    
    loadPreset_loadBeforeConnections(presetFolderPath);
    
    loadPreset_deactivateConnections();

    loadPreset_loadConnections(presetFolderPath);
    
    loadPreset_midiBindings(presetFolderPath);
    
    loadPreset_loadNodePreset(presetFolderPath);
    
    loadPreset_activateConnections();
    
    loadPreset_loadComments(presetFolderPath);
    
    loadPreset_presetHasLoaded();
    
	loadScope(presetFolderPath);
	loadCustomGuis(presetFolderPath);
    loadCustomGuiSnapshots(presetFolderPath);
	
	resetPhase();
	
	// Re-enable scope auto-save callback AFTER all loading is complete
	// This must be the LAST step to avoid saving during any deferred cleanup
	// Capture presetFolderPath by value to ensure we save to the correct location
	ofxOceanodeScope::getInstance()->setScopeChangedCallback([this, presetFolderPath]()
	{
		if (!(!getCanvasID().empty() && getCanvasID() != "Canvas" && getCanvasID() != "0"))
		{
			saveScope(presetFolderPath);
		}
	});

    return true;
}

void ofxOceanodeContainer::saveCustomGuis(const std::string& presetPath)
{
    customGuiStoragePath = presetPath;
    ofSavePrettyJson(getCustomGuiFilePath(presetPath), customGuiPanelsToJson(customGuiPanelsData));
    customGuisDirty = false;
}

void ofxOceanodeContainer::saveCustomGuiSnapshots(const std::string& presetPath)
{
    customGuiStoragePath = presetPath;
    std::string filePath = getCustomGuiSnapshotsFilePath(presetPath);

    std::vector<CustomGuiSnapshotBank> sanitizedBanks;
    sanitizedBanks.reserve(customGuiSnapshotBanks.size());
    for(auto bank : customGuiSnapshotBanks){
        if(bank.customGuiId.empty()) continue;
        const CustomGuiPanelData* panel = getCustomGuiPanelData(bank.customGuiId);
        if(panel == nullptr) continue;

        bank.customGuiName = panel->name;
        bank.snapshots.erase(std::remove_if(bank.snapshots.begin(), bank.snapshots.end(), [](const CustomGuiSnapshotData& snapshot){
            return snapshot.id.empty() || snapshot.parameterValues.empty();
        }), bank.snapshots.end());
        for(size_t i = 0; i < bank.snapshots.size(); i++){
            if(bank.snapshots[i].slot < 0) bank.snapshots[i].slot = (int)i;
        }
        std::sort(bank.snapshots.begin(), bank.snapshots.end(), [](const CustomGuiSnapshotData& a, const CustomGuiSnapshotData& b){
            return a.slot < b.slot;
        });

        if(bank.snapshots.empty()) continue;
        sanitizedBanks.push_back(std::move(bank));
    }
    customGuiSnapshotBanks = sanitizedBanks;

    if(customGuiSnapshotBanks.empty()){
        if(ofFile::doesFileExist(filePath)) ofFile::removeFile(filePath);
        customGuiSnapshotsDirty = false;
        return;
    }

    ofSavePrettyJson(filePath, customGuiSnapshotBanksToJson(customGuiSnapshotBanks));
    customGuiSnapshotsDirty = false;
}

void ofxOceanodeContainer::saveCustomGuis()
{
    if(customGuiStoragePath.empty()) return;
    saveCustomGuis(customGuiStoragePath);
}

void ofxOceanodeContainer::saveCustomGuiSnapshots()
{
    if(customGuiStoragePath.empty()) return;
    saveCustomGuiSnapshots(customGuiStoragePath);
}

void ofxOceanodeContainer::markCustomGuisDirty()
{
    customGuisDirty = true;
}

void ofxOceanodeContainer::markCustomGuiSnapshotsDirty()
{
    customGuiSnapshotsDirty = true;
}

void ofxOceanodeContainer::loadCustomGuis(const std::string& presetPath)
{
    customGuiStoragePath = presetPath;
    std::string filepath = getCustomGuiFilePath(presetPath);
    ofFile file(filepath);
    customGuiPanelsData.clear();
    if(!file.exists()){
        rebuildCustomGuiPanels();
        customGuisDirty = false;
        return;
    }

    ofJson json;
    try {
        json = ofLoadJson(filepath);
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error loading custom GUIs: " << e.what();
        rebuildCustomGuiPanels();
        return;
    }

    try {
        customGuiPanelsData = customGuiPanelsFromJson(json);
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error parsing custom GUIs: " << e.what();
        customGuiPanelsData.clear();
    }

    auto isStaticWidgetType = [](CustomGuiWidgetType type){
        return type == CustomGuiWidgetType::Label ||
               type == CustomGuiWidgetType::BackgroundPanel ||
               type == CustomGuiWidgetType::Text ||
               type == CustomGuiWidgetType::Image;
    };

    std::vector<CustomGuiPanelData> sanitizedPanels;
    sanitizedPanels.reserve(customGuiPanelsData.size());
    for(auto panel : customGuiPanelsData){
        std::set<std::string> widgetNodeNames;
        int parameterWidgetCount = 0;
        int staticWidgetCount = 0;
        std::vector<CustomGuiWidget> sanitizedWidgets;
        sanitizedWidgets.reserve(panel.layout.widgets.size());

        for(auto widget : panel.layout.widgets){
            if(isStaticWidgetType(widget.type)){
                staticWidgetCount++;
                sanitizedWidgets.push_back(std::move(widget));
                continue;
            }

            if(widget.parameterRef.parameterPath.empty()) continue;
            ofxOceanodeAbstractParameter* parameter = findCustomGuiParameter(widget.parameterRef.parameterPath);
            if(parameter == nullptr) continue;

            parameterWidgetCount++;
            if(!widget.parameterRef.nodeDisplayName.empty()){
                widgetNodeNames.insert(widget.parameterRef.nodeDisplayName);
            }else{
                auto* node = getNodeFromParameter(*parameter);
                if(node != nullptr){
                    widget.parameterRef.nodeDisplayName = node->getParameters().getName();
                    widgetNodeNames.insert(widget.parameterRef.nodeDisplayName);
                }
            }
            widget.parameterRef.parameterDisplayName = parameter->getName();
            sanitizedWidgets.push_back(std::move(widget));
        }

        panel.layout.widgets = std::move(sanitizedWidgets);
        if(panel.layout.widgets.empty()) continue;

        const bool looksLikeLegacySingleNodePanel =
            parameterWidgetCount > 0 &&
            staticWidgetCount == 0 &&
            widgetNodeNames.size() == 1 &&
            panel.name == *widgetNodeNames.begin();

        if(looksLikeLegacySingleNodePanel) continue;

        panel.designMode = false;
        panel.windowState.isOpen = true;
        sanitizedPanels.push_back(std::move(panel));
    }

    customGuiPanelsData = std::move(sanitizedPanels);
    rebuildCustomGuiPanels();
    customGuisDirty = false;
}

void ofxOceanodeContainer::loadCustomGuiSnapshots(const std::string& presetPath)
{
    customGuiStoragePath = presetPath;
    customGuiSnapshotBanks.clear();

    std::string filepath = getCustomGuiSnapshotsFilePath(presetPath);
    ofFile file(filepath);
    if(!file.exists()){
        customGuiSnapshotsDirty = false;
        return;
    }

    ofJson json;
    try {
        json = ofLoadJson(filepath);
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error loading custom GUI snapshots: " << e.what();
        return;
    }

    try {
        customGuiSnapshotBanks = customGuiSnapshotBanksFromJson(json);
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error parsing custom GUI snapshots: " << e.what();
        customGuiSnapshotBanks.clear();
    }

    std::vector<CustomGuiSnapshotBank> sanitizedBanks;
    sanitizedBanks.reserve(customGuiSnapshotBanks.size());
    for(auto bank : customGuiSnapshotBanks){
        if(bank.customGuiId.empty()) continue;
        const CustomGuiPanelData* panel = getCustomGuiPanelData(bank.customGuiId);
        if(panel == nullptr) continue;

        bank.customGuiName = panel->name;
        bank.snapshots.erase(std::remove_if(bank.snapshots.begin(), bank.snapshots.end(), [](const CustomGuiSnapshotData& snapshot){
            return snapshot.id.empty() || snapshot.parameterValues.empty();
        }), bank.snapshots.end());
        for(size_t i = 0; i < bank.snapshots.size(); i++){
            if(bank.snapshots[i].slot < 0) bank.snapshots[i].slot = (int)i;
        }
        std::sort(bank.snapshots.begin(), bank.snapshots.end(), [](const CustomGuiSnapshotData& a, const CustomGuiSnapshotData& b){
            return a.slot < b.slot;
        });
        if(bank.snapshots.empty()) continue;

        bool currentExists = false;
        for(const auto& snapshot : bank.snapshots){
            if(snapshot.id == bank.currentSnapshotId){
                currentExists = true;
                break;
            }
        }
        if(!currentExists){
            bank.currentSnapshotId = bank.snapshots.front().id;
        }

        sanitizedBanks.push_back(std::move(bank));
    }

    customGuiSnapshotBanks = std::move(sanitizedBanks);
    customGuiSnapshotsDirty = false;
}

void ofxOceanodeContainer::saveScope(const std::string& presetPath)
{
	// Get scope state from scope
	auto scopeState = ofxOceanodeScope::getInstance()->getScopeState();
	
	// Always save – even an empty scope (no parameters) must overwrite the file
	// so that removing all scoped parameters and hitting Save Preset correctly
	// reflects zero scopes.  The callback is cleared in clearContainer() to
	// prevent spurious saves during app exit or preset switching.
	ofJson json = scopeState.toJson();
	
	// Save to file
	std::string filepath = presetPath + "/scope_config.json";
	ofSavePrettyJson(filepath, json);
}

void ofxOceanodeContainer::loadScope(const std::string& presetPath) {
    // Only load scope for root canvas - skip if path contains Macro_ (same logic as saveScope)
	if (!getCanvasID().empty() && getCanvasID() != "Canvas" && getCanvasID() != "0")
	{
		// Skip loading for macro containers
		return;
	}

    std::string filepath = presetPath + "/scope_config.json";
    
    // Check if file exists
    ofFile file(filepath);
    if(!file.exists()) {
        ofLogNotice("ofxOceanodeContainer") << "No scope config file found at: " << filepath;
        ofxOceanodeScope::getInstance()->clearScopedParameters();
        return;
    }
    
    // Load JSON
    ofJson json;
    try {
        json = ofLoadJson(filepath);
    } catch (const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error loading scope file: " << e.what();
        return;
    }
    
    // Parse to scope state
    ofxOceanodeScopeState scopeState = ofxOceanodeScopeState::fromJson(json);
    
    // Clear existing scope
    ofxOceanodeScope::getInstance()->clearScopedParameters();
    
    // Set window configuration
    ofxOceanodeScope::getInstance()->setWindowConfig(scopeState.windowConfig);
    
    // Resolve and add parameters
    int successCount = 0;
    int failureCount = 0;
    
    // Tell the scope we're loading from preset so it doesn't rebuild the dock tree
    ofxOceanodeScope::getInstance()->setLoadingFromPreset(true);
    
    for(const auto& paramData : scopeState.parameters) {
        // Resolve parameter with canvasID awareness
        auto resolved = resolveParameterFromPath(paramData.parameterPath, paramData.canvasID);
        
        if(resolved.parameter != nullptr) {
            // Get color from the resolved node
            ofColor nodeColor = ofColor::white; // default
            if(resolved.node != nullptr) {
                nodeColor = resolved.node->getColor();
            }
            
            // Add to scope
            ofxOceanodeScope::getInstance()->addParameter(resolved.parameter, nodeColor);
            
            // Set size relative (access the last added item)
            auto& scopedParams = ofxOceanodeScope::getInstance()->getScopedParameters();
            if(!scopedParams.empty()) {
                scopedParams.back().sizeRelative = paramData.sizeRelative;
            }
            
            successCount++;
        } else {
            ofLogWarning("ofxOceanodeContainer") << "Could not resolve parameter: " 
                                                  << paramData.parameterPath;
            failureCount++;
        }
    }
    
    ofxOceanodeScope::getInstance()->setLoadingFromPreset(false);
        
    if (successCount > 0) {
        for (const auto& data : scopeState.parameters) {
            std::string fullPath = data.canvasID;
            if (!fullPath.empty() && fullPath != "Canvas" && fullPath != "0") {
                fullPath += " > ";
            } else {
                fullPath = "";
            }
            fullPath += data.nodeName + " / " + data.paramName;
        }
    }
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



void ofxOceanodeContainer::loadPreset_presetWillBeLoaded(){
    for(auto &nodeTypeMap : dynamicNodes){
            for(auto &node : nodeTypeMap.second){
                node.second->presetWillBeLoaded();
            }
        }
    
        for(auto &nodeTypeMap : persistentNodes){
            for(auto &node : nodeTypeMap.second){
                node.second->presetWillBeLoaded();
            }
        }
}

void ofxOceanodeContainer::loadPreset_loadNodes(string presetFolderPath){
    const std::string modulesPath = presetFolderPath + "/modules.json";
    if(!ofFile::doesFileExist(modulesPath)){
        ofLogWarning("ofxOceanodeContainer") << "Preset modules file not found: " << modulesPath;
        return;
    }

    ofJson json = ofLoadJson(modulesPath);
    if(!json.empty()){;
        for(auto &models : registry->getRegisteredModels()){
            string moduleName = models.first;
            vector<int>  vector_of_dynamic_identifiers;
            vector<int>  vector_of_persistent_identifiers;
            if(dynamicNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : dynamicNodes[moduleName]){
                    vector_of_dynamic_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            if(persistentNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : persistentNodes[moduleName]){
                    vector_of_persistent_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            
            for(auto identifier : vector_of_dynamic_identifiers){
                string stringIdentifier = ofToString(identifier, 2, '0');
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
                    glm::vec2 position(readArray[0], readArray[1]);
                    dynamicNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }else{
                    for(int i = 0; i < connections.size();){
                        auto &connection = connections[i];
                        string sourceName = connection->getSourceParameter().getGroupHierarchyNames()[0];;
                        string sourceModuleId = ofSplitString(sourceName, "_").back();
                        sourceName.erase(sourceName.rfind(sourceModuleId)-1);
                        ofStringReplace(sourceName, "_", " ");
                        string sinkName = connection->getSinkParameter().getGroupHierarchyNames()[0];;
                        string sinkModuleId = ofSplitString(sinkName, "_").back();
                        sinkName.erase(sinkName.rfind(sinkModuleId)-1);
                        ofStringReplace(sinkName, "_", " ");
                        if((sourceName == moduleName && ofToInt(sourceModuleId) == identifier) || (sinkName == moduleName && ofToInt(sinkModuleId) == identifier)){
                            connections.erase(connections.begin()+i);
                        }else{
                            i++;
                        }
                    }
                    dynamicNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for(auto identifier : vector_of_persistent_identifiers){
                string stringIdentifier = ofToString(identifier);
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
                    glm::vec2 position(readArray[0], readArray[1]);
                    persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }
            }
            
            
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(dynamicNodes[moduleName].count(identifier) == 0){
                    vector<float> readArray = it.value();
                    if(readArray.size() == 2){ //Size 3 means it is only saved as persistent, we only want to move it, if it does not exist we don't create it
                        auto node = createNodeFromName(moduleName, identifier);
                        node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
                    }
                }
            }
        }
    }else{
        for(auto &pair : dynamicNodes){
            for(auto &nodes : pair.second){
                for(int i = 0; i < connections.size();){
                    auto &connection = connections[i];
                    string sourceName = connection->getSourceParameter().getGroupHierarchyNames()[0];;
                    string sourceModuleId = ofSplitString(sourceName, "_").back();
                    sourceName.erase(sourceName.rfind(sourceModuleId)-1);
                    ofStringReplace(sourceName, "_", " ");
                    string sinkName = connection->getSinkParameter().getGroupHierarchyNames()[0];;
                    string sinkModuleId = ofSplitString(sinkName, "_").back();
                    sinkName.erase(sinkName.rfind(sinkModuleId)-1);
                    ofStringReplace(sinkName, "_", " ");
                    if((sourceName == pair.first && ofToInt(sourceModuleId) == (nodes.first)) || (sinkName == pair.first && ofToInt(sinkModuleId) == (nodes.first))){
                        connections.erase(connections.begin()+i);
                    }else{
                        i++;
                    }
                }
            }
        }
        //TODO: Only delete not persistent in the map
        parameterGroupNodesMap.clear();
        std::vector<std::shared_ptr<ofxOceanodeNode>> allNodes;
        for(auto &nodeTypeMap : dynamicNodes){
            for(auto &node : nodeTypeMap.second){
                allNodes.push_back(node.second);
            }
        }
        for(auto n : allNodes) n->deleteSelf();
    }
    allNodesCreated.notify(this);
}

void ofxOceanodeContainer::loadPreset_deactivateConnections(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->deactivateConnections();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->deactivateConnections();
        }
    }
    for(auto &connection : connections){
        connection->setActive(false);
    }
}

void ofxOceanodeContainer::loadPreset_loadBeforeConnections(string presetFolderPath){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPresetBeforeConnections(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPresetBeforeConnections(presetFolderPath);
        }
    }
}

void ofxOceanodeContainer::loadPreset_loadConnections(string presetFolderPath){
    ofJson json = ofLoadJson(presetFolderPath + "/connections.json");
    for(int i = 0; i < connections.size();){
        string sourceParameter = connections[i]->getSourceParameter().getName();
        string sourceModule = connections[i]->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkParameter = connections[i]->getSinkParameter().getName();
        string sinkModule = connections[i]->getSinkParameter().getGroupHierarchyNames()[0];

        bool foundConnection = false;
        if(json.find(sourceModule) != json.end()){
            if(json[sourceModule].find(sourceParameter) != json[sourceModule].end()){
                if(json[sourceModule][sourceParameter].find(sinkModule) != json[sourceModule][sourceParameter].end()){
                    if(json[sourceModule][sourceParameter][sinkModule].find(sinkParameter) != json[sourceModule][sourceParameter][sinkModule].end()){
                        json[sourceModule][sourceParameter][sinkModule].erase(sinkParameter);
                        foundConnection = true;
                        if(json[sourceModule][sourceParameter][sinkModule].size() == 0){
                            json[sourceModule][sourceParameter].erase(sinkModule);
                            if(json[sourceModule][sourceParameter].size() == 0){
                                json[sourceModule].erase(sourceParameter);
                                if(json[sourceModule].size() == 0){
                                    json.erase(sourceModule);
                                }
                            }
                        }
                        i++;
                    }
                }
            }
        }
        if(!foundConnection){
            if(!connections[i]->getIsPersistent()){
                connections.erase(connections.begin()+i);
            }else{
                i++;
            }
        }
    }
        
    vector<vector<string>> oldConnectionsInfo(connections.size(), vector<string>(4));
    for(int i = 0; i < connections.size(); i++){
        oldConnectionsInfo[i][1] = connections[i]->getSourceParameter().getName();
        oldConnectionsInfo[i][0] = connections[i]->getSourceParameter().getGroupHierarchyNames()[0];
        oldConnectionsInfo[i][3] = connections[i]->getSinkParameter().getName();
        oldConnectionsInfo[i][2] = connections[i]->getSinkParameter().getGroupHierarchyNames()[0];

    }
    std::vector<string> notCreatedConnectionInfo;
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    bool connectionExist = false;
                    for(int i = 0; i < oldConnectionsInfo.size(); i++){
                        if(!(oldConnectionsInfo[i][0] != sourceModule.key()
                             || oldConnectionsInfo[i][1] != sourceParameter.key()
                             || oldConnectionsInfo[i][2] != sinkModule.key()
                             || oldConnectionsInfo[i][3] != sinkParameter.key())){
                            oldConnectionsInfo.erase(oldConnectionsInfo.begin()+i);
                            connectionExist = true;
                            break;
                        }
                    }
                    if(!connectionExist){
                        auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key(), false);
                        if(connection == nullptr){ //Connection could not be made
                            notCreatedConnectionInfo.push_back(sourceModule.key() + "/" + sourceParameter.key() + " -> " + sinkModule.key() + "/" + sinkParameter.key());
                        }
                    }
                }
            }
        }
    }
    if(notCreatedConnectionInfo.size() != 0){
        std::string message = "ERROR\nCOULD NOT CREATE CONNECTIONS\nIN " + getCanvasID();
        for(auto l : notCreatedConnectionInfo) message += "\n" + l;
        ofSystemAlertDialog(message);
    }
}

void ofxOceanodeContainer::loadPreset_midiBindings(string presetFolderPath){
#ifdef OFXOCEANODE_USE_MIDI
    //TODO: No remove old connections
    ofJson json;
    for(auto &bindingVec : midiBindings){
        for(auto &binding : bindingVec.second){
            for(auto &midiInPair : midiIns){
                midiInPair.second.removeListener(binding.get());
            }
        }
    }
    midiBindings.clear();
    json = ofLoadJson(presetFolderPath + "/midi.json");
    for (ofJson::iterator module = json.begin(); module != json.end(); ++module) {
        for (ofJson::iterator parameter = module.value().begin(); parameter != module.value().end(); ++parameter) {
            if(parameter->is_array()){ //New MultiMidi Method (Setp 19)
                for(int i = 0; i < parameter->size(); i++){
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, i);
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, ofToInt(binding.key()));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key());
                if(midiBinding != nullptr){
                    midiBinding->loadPreset(json[module.key()][parameter.key()]);
                }
            }
        }
    }
#endif
}

void ofxOceanodeContainer::loadPreset_loadNodePreset(string presetFolderPath){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
}

void ofxOceanodeContainer::loadPreset_activateConnections(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->activateConnections();
        }
    }
    
    for(auto &connection : connections){
        connection->setActive(true);
    }
}

void ofxOceanodeContainer::loadPreset_presetHasLoaded(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
}

void ofxOceanodeContainer::loadPreset_loadComments(string presetFolderPath){
    ofJson json = ofLoadJson(presetFolderPath + "/comments.json");
    if(!json.empty()){
        comments.resize(json["NumComments"]);
        for (int i = 0; i < comments.size(); i++) {
            auto &c = comments[i];
            c.text = json["Comments"][i]["Text"];
            c.size.x = json["Comments"][i]["Size"]["X"];
            c.size.y = json["Comments"][i]["Size"]["Y"];
            c.position.x = json["Comments"][i]["Pos"]["X"];
            c.position.y = json["Comments"][i]["Pos"]["Y"];
            c.color.r = json["Comments"][i]["Color"]["R"];
            c.color.g = json["Comments"][i]["Color"]["G"];
            c.color.b = json["Comments"][i]["Color"]["B"];
            c.textColor.r = json["Comments"][i]["TextColor"]["R"];
            c.textColor.g = json["Comments"][i]["TextColor"]["G"];
            c.textColor.b = json["Comments"][i]["TextColor"]["B"];
        }
    }
}

void ofxOceanodeContainer::savePreset(string presetFolderPath){
    ofLog()<<"Save Preset " << presetFolderPath << " Canvas ID : " << getCanvasID() << endl;
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first, 2, '0')] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
            pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y, 0}; //We add an element to know is persistent
        }
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        if(!connection->getIsPersistent()){
            string sourceName = connection->getSourceParameter().getName();
            string sourceParentName = connection->getSourceParameter().getNodeModel()->getParameterGroup().getEscapedName();
            string sinkName = connection->getSinkParameter().getName();
            string sinkParentName = connection->getSinkParameter().getNodeModel()->getParameterGroup().getEscapedName();
            json[sourceParentName][sourceName][sinkParentName][sinkName];
        }
    }
    
    ofSavePrettyJson(presetFolderPath + "/connections.json", json);
    
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePreset(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePreset(presetFolderPath);
        }
    }
    
#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingsPair : midiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    ofSavePrettyJson(presetFolderPath + "/midi.json", json);
#endif
	
	json.clear();
	json["NumComments"] = comments.size();
	for(int i = 0; i < comments.size(); i++){
		auto &c = comments[i];
		json["Comments"][i]["Text"] = c.text;
		json["Comments"][i]["Size"]["X"] = c.size.x;
		json["Comments"][i]["Size"]["Y"] = c.size.y;
		json["Comments"][i]["Pos"]["X"] = c.position.x;
		json["Comments"][i]["Pos"]["Y"] = c.position.y;
		json["Comments"][i]["Color"]["R"] = c.color.r;
		json["Comments"][i]["Color"]["G"] = c.color.g;
		json["Comments"][i]["Color"]["B"] = c.color.b;
		json["Comments"][i]["TextColor"]["R"] = c.textColor.r;
		json["Comments"][i]["TextColor"]["G"] = c.textColor.g;
		json["Comments"][i]["TextColor"]["B"] = c.textColor.b;
	}
	ofSavePrettyJson(presetFolderPath + "/comments.json", json);
	
	saveScope(presetFolderPath);
	saveCustomGuis(presetFolderPath);
    saveCustomGuiSnapshots(presetFolderPath);
	
}

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
            auto node = createNodeFromName(nodeType.key());
            node->getNodeGui().setPosition(glm::vec2(nodeId.value()[0], nodeId.value()[1]) + referencePosition);
            node->getNodeGui().setSelected(true);
            newCreatedNodes.push_back(node);
            int newNodeId = node->getNodeModel().getNumIdentifier();
            string escapedNodeName = nodeType.key();
            ofStringReplace(escapedNodeName, " ", "_");
            moduleConverter[escapedNodeName][ofToInt(nodeId.key())] = newNodeId;
            ofJson tempJson = ofLoadJson(presetFolderPath / (escapedNodeName + "_" + ofToString(ofToInt(nodeId.key())) + ".json"));
            ofSaveJson(tempLoadFolderPath / (escapedNodeName + "_" + ofToString(newNodeId) + ".json"), tempJson);
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
        node->savePreset(presetFolderPath);
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

void ofxOceanodeContainer::savePersistent(){
    ofLog()<<"Save Persistent";
    string persistentFolderPath = "Persistent";
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    ofSavePrettyJson(persistentFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        string sourceName = connection->getSourceParameter().getName();
        string sourceParentName = connection->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkName = connection->getSinkParameter().getName();
        string sinkParentName = connection->getSinkParameter().getGroupHierarchyNames()[0];
        json[sourceParentName][sourceName][sinkParentName][sinkName];
    }
    
    ofSavePrettyJson(persistentFolderPath + "/connections.json", json);
    
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }

#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingsPair : midiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    for(auto &bindingsPair : persistentMidiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    ofSavePrettyJson(persistentFolderPath + "/midi.json", json);
#endif
}

void ofxOceanodeContainer::loadPersistent(){
    ofLog()<<"Load Persistent";
    string persistentFolderPath = "Persistent";
    
    //Read new nodes in preset
    //Check if the nodes exists and update them, (or update all at the end)
    //Create new modules and update them (or update at end)
    ofJson json = ofLoadJson(persistentFolderPath + "/modules.json");
    if(!json.empty()){;
        for(auto &models : registry->getRegisteredModels()){
            string moduleName = models.first;
            vector<int>  vector_of_identifiers;
            if(persistentNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : persistentNodes[moduleName]){
                    vector_of_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            for(auto identifier : vector_of_identifiers){
                string stringIdentifier = ofToString(identifier);
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
					glm::vec2 position(readArray[0], readArray[1]);
					persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }else{
                    persistentNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(persistentNodes[moduleName].count(identifier) == 0){
                    auto node = createNodeFromName(moduleName, identifier, true);
					node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
                }
            }
        }
    }else{
        persistentNodes.clear();
    }
    
    //connections.clear();
    for(int i = 0; i < connections.size();){
        if(!connections[i]->getIsPersistent()){
            connections.erase(connections.begin()+i);
        }else{
            i++;
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPersistentPreset(persistentFolderPath);
        }
    }
    
    json.clear();
    json = ofLoadJson(persistentFolderPath + "/connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key());
                    if(connection != nullptr) connection->setIsPersistent(true);
                }
            }
        }
    }
    
#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingVec : persistentMidiBindings){
        for(auto &binding : bindingVec.second){
            for(auto &midiInPair : midiIns){
                midiInPair.second.removeListener(binding.get());
            }
        }
    }
    persistentMidiBindings.clear();
    json = ofLoadJson(persistentFolderPath + "/midi.json");
    for (ofJson::iterator module = json.begin(); module != json.end(); ++module) {
        for (ofJson::iterator parameter = module.value().begin(); parameter != module.value().end(); ++parameter) {
            if(parameter->is_array()){ //New MultiMidi Method (Setp 19)
                for(int i = 0; i < parameter->size(); i++){
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, i);
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, ofToInt(binding.key()));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true);
                if(midiBinding != nullptr){
                    midiBinding->loadPreset(json[module.key()][parameter.key()]);
                }
            }
        }
    }
#endif
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
}

void ofxOceanodeContainer::updatePersistent(){
    string persistentFolderPath = "Persistent";
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }
}

void ofxOceanodeContainer::saveCurrentPreset(){
    saveCurrentPresetEvent.notify();
}

void ofxOceanodeContainer::setBpm(float _bpm){
    bpm = _bpm;
    if(transport != nullptr){
        transport->setBpm(bpm);
    }
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setBpm(bpm);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setBpm(bpm);
        }
    }
}

void ofxOceanodeContainer::resetPhase(bool notifyTransport){
    if(notifyTransport && transport != nullptr){
        transport->notifyReset();
    }
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->resetPhase();
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->resetPhase();
        }
    }
}

ofxOceanodeTransportState ofxOceanodeContainer::getTransportState() const{
    if(transport == nullptr){
        return {};
    }
    return transport->getState();
}

ofxOceanodeFrameTransportState ofxOceanodeContainer::getFrameTransportState() const{
    if(transport == nullptr){
        return {};
    }
    return transport->getFrameState();
}

#ifdef OFXOCEANODE_USE_OSC

void ofxOceanodeContainer::receiveOscMessage(ofxOscMessage &m){
	//Todo: Fix for ofxOceanodeParameter
    auto setParameterFromMidiMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
			castedParam = ofClamp(m.getArgAsFloat(0), castedParam.getMin(), castedParam.getMax());
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                castedParam = ofClamp(m.getArgAsInt(0), castedParam.getMin(), castedParam.getMax());
            }
            
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = m.getArgAsBool(0);
        }else if(absParam.valueType() == typeid(void).name()){
            absParam.cast<void>().getParameter().trigger();
        }else if(absParam.valueType() == typeid(string).name()){
            absParam.cast<string>().getParameter() = m.getArgAsString(0);
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<float> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                for(int i = 0; i < tempVec.size(); i++){
					tempVec[i] = ofClamp(m.getArgAsFloat(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                }
                castedParam = tempVec;
            }
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<int> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofMap(m.getArgAsFloat(i), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true);
                    }
                }
                else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofClamp(m.getArgAsInt(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                    }
                }
                castedParam = tempVec;
            }
        }
    };
    
    auto modulateParameterFromOscMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
            castedParam = castedParam + m.getArgAsFloat(0);
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax() - castedParam.getMin();
                castedParam += ofMap(m.getArgAsFloat(0), -1, 1, -range, range, false);
            }else{
                castedParam += (castedParam+m.getArgAsInt(0));
            }
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = !absParam.cast<bool>().getParameter();
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<float> tempVec;
            if(m.getNumArgs() == 1 && castedParam->size() != 1){
                tempVec = castedParam;
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(0);
                }
            }else{
                if(m.getNumArgs() == castedParam->size()){
                    tempVec = castedParam;
                }else if(m.getNumArgs() > castedParam->size()){
                    tempVec = vector<float>(m.getNumArgs(), castedParam.get()[0]);
                }
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(i);
                }
            }
            castedParam = tempVec;
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<int> tempVec;
            tempVec.resize(m.getNumArgs(), 0);
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax()[0] - castedParam.getMin()[0];
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] += ofMap(m.getArgAsFloat(i), -1, 1, -range, range, true);
                }
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                if(m.getNumArgs() == 1 && castedParam->size() != 1){
                    tempVec = castedParam;
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(0);
                    }
                }else{
                    if(m.getNumArgs() == castedParam->size()){
                        tempVec = castedParam;
                    }else if(m.getNumArgs() > castedParam->size()){
                        tempVec = vector<int>(m.getNumArgs(), castedParam.get()[0]);
                    }
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(i);
                    }
                }
            }
            castedParam = tempVec;
        }
    };
    
    
    vector<string> splitAddress = ofSplitString(m.getAddress(), "/");
    if(splitAddress[0].size() == 0) splitAddress.erase(splitAddress.begin());
    if(splitAddress.size() == 1){
        if(splitAddress[0] == "phaseReset"){
            resetPhase();
        }else if(splitAddress[0] == "bpm"){
            float newBpm = m.getArgAsFloat(0);
            ofNotifyEvent(changedBpmEvent, newBpm);
        }
    }else if(splitAddress.size() == 2){ //Load preset by name
        if(splitAddress[0] == "presetLoad"){
            auto toSendPair = make_pair(splitAddress[1], m.getArgAsString(0));
            loadPresetEvent.notify(toSendPair);
        }else if(splitAddress[0] == "presetLoadi"){ //Load preset by number
			if(m.getArgAsInt(0) != 0){
				auto toSendPair = make_pair(splitAddress[1], m.getArgAsInt(0));
				loadPresetNumEvent.notify(toSendPair);
			}
//        }else if(splitAddress[0] == "presetSave"){
//            savePreset("Presets/" + splitAddress[1] + "/" + m.getArgAsString(0));
        }else if(splitAddress[0] == "Global"){
            for(auto &nodeType  : dynamicNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
            for(auto &nodeType  : persistentNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
        }else{
            //TODO: check if is in the form of NAME_ID
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            if(moduleName.find("_") != std::string::npos){
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                bool validOSC = false;
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(!validOSC){
                    string moduleName = splitAddress[0];
                    string moduleId = ofSplitString(moduleName, "_").back();
                    moduleName.erase(moduleName.find(moduleId)-1);
                    ofStringReplace(moduleName, "_", " ");
                    if(dynamicNodes.count(moduleName) == 1){
                        if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                            string newAddress;
                            for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                            m.setAddress(newAddress);
                            dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                        }
                    }
                }
            }
        }
    }
    else if(splitAddress.size() == 3){
		if(splitAddress[0] == "relative"){
            if(splitAddress[1] == "Global"){
                for(auto &nodeType  : dynamicNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                for(auto &nodeType  : persistentNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }else{
                string moduleName = splitAddress[1];
                string moduleId = ofSplitString(moduleName, "_").back();
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }
        }else{
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            moduleName.erase(moduleName.find(moduleId)-1);
            ofStringReplace(moduleName, "_", " ");
            if(dynamicNodes.count(moduleName) == 1){
                if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                    string newAddress;
                    for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                    m.setAddress(newAddress);
                    dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                }
            }
        }
    }
}

#endif


namespace {
    bool isCustomGuiSnapshotSupportedType(const std::string& type)
    {
        return type == typeid(float).name() ||
               type == typeid(std::vector<float>).name() ||
               type == typeid(int).name() ||
               type == typeid(std::vector<int>).name() ||
               type == typeid(bool).name() ||
               type == typeid(std::vector<bool>).name() ||
               type == typeid(std::string).name() ||
               type == typeid(std::vector<std::string>).name() ||
               type == typeid(ofColor).name() ||
               type == typeid(ofFloatColor).name();
    }

    std::string trimCustomGuiSnapshotName(const std::string& name, const std::string& fallback)
    {
        std::string trimmed = name;
        auto begin = trimmed.find_first_not_of(" \t\n\r");
        auto end = trimmed.find_last_not_of(" \t\n\r");
        if(begin == std::string::npos) trimmed.clear();
        else trimmed = trimmed.substr(begin, end - begin + 1);
        return trimmed.empty() ? fallback : trimmed;
    }
}

std::string ofxOceanodeContainer::getCustomGuiFilePath(const std::string& presetPath) const
{
    return presetPath + "/custom_guis.json";
}

std::string ofxOceanodeContainer::getCustomGuiSnapshotsFilePath(const std::string& presetPath) const
{
    return presetPath + "/custom_gui_snapshots.json";
}

std::string ofxOceanodeContainer::makeUniqueCustomGuiName(const std::string& baseName) const
{
    std::string candidate = baseName.empty() ? "Custom GUI" : baseName;
    auto nameExists = [&](const std::string& name){
        return std::any_of(customGuiPanelsData.begin(), customGuiPanelsData.end(), [&](const CustomGuiPanelData& panel){
            return panel.name == name;
        });
    };

    if(!nameExists(candidate)) return candidate;
    for(int i = 2; ; i++){
        std::string numbered = candidate + " " + ofToString(i);
        if(!nameExists(numbered)) return numbered;
    }
}

std::string ofxOceanodeContainer::makeCustomGuiId() const
{
    return "custom_gui_" + ofGetTimestampString("%Y%m%d%H%M%S") + "_" + ofToString(ofGetElapsedTimeMicros()) + "_" + ofToString(customGuiPanelsData.size() + 1);
}

std::string ofxOceanodeContainer::makeCustomGuiSnapshotId() const
{
    return "custom_gui_snapshot_" + ofGetTimestampString("%Y%m%d%H%M%S") + "_" + ofToString(ofGetElapsedTimeMicros());
}

std::string ofxOceanodeContainer::makeUniqueCustomGuiSnapshotName(const CustomGuiSnapshotBank& bank, const std::string& baseName) const
{
    const std::string candidateBase = baseName.empty() ? "Snapshot" : baseName;
    auto nameExists = [&](const std::string& name){
        return std::any_of(bank.snapshots.begin(), bank.snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
            return snapshot.name == name;
        });
    };

    if(!nameExists(candidateBase)) return candidateBase;
    for(int i = 2; ; i++){
        const std::string numbered = candidateBase + " " + ofToString(i);
        if(!nameExists(numbered)) return numbered;
    }
}

int ofxOceanodeContainer::getNextAvailableCustomGuiSnapshotSlot(const CustomGuiSnapshotBank& bank) const
{
    int slot = 0;
    while(true){
        bool used = false;
        for(const auto& snapshot : bank.snapshots){
            if(snapshot.slot == slot){
                used = true;
                break;
            }
        }
        if(!used) return slot;
        slot++;
    }
}

void ofxOceanodeContainer::rebuildCustomGuiPanels()
{
    customGuiPanels.clear();
    for(const auto& panelData : customGuiPanelsData){
        customGuiPanels.push_back(std::make_unique<ofxOceanodeCustomGuiPanel>(*this, panelData.id));
    }
}

CustomGuiPanelData* ofxOceanodeContainer::getCustomGuiPanelData(const std::string& panelId)
{
    for(auto& panel : customGuiPanelsData){
        if(panel.id == panelId) return &panel;
    }
    return nullptr;
}

const CustomGuiPanelData* ofxOceanodeContainer::getCustomGuiPanelData(const std::string& panelId) const
{
    for(const auto& panel : customGuiPanelsData){
        if(panel.id == panelId) return &panel;
    }
    return nullptr;
}

CustomGuiSnapshotBank* ofxOceanodeContainer::getCustomGuiSnapshotBank(const std::string& panelId)
{
    for(auto& bank : customGuiSnapshotBanks){
        if(bank.customGuiId == panelId) return &bank;
    }
    return nullptr;
}

const CustomGuiSnapshotBank* ofxOceanodeContainer::getCustomGuiSnapshotBank(const std::string& panelId) const
{
    for(const auto& bank : customGuiSnapshotBanks){
        if(bank.customGuiId == panelId) return &bank;
    }
    return nullptr;
}

CustomGuiSnapshotBank* ofxOceanodeContainer::getOrCreateCustomGuiSnapshotBank(const std::string& panelId)
{
    if(panelId.empty()) return nullptr;
    if(auto* existing = getCustomGuiSnapshotBank(panelId)) return existing;

    const CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(panel == nullptr) return nullptr;

    CustomGuiSnapshotBank bank;
    bank.customGuiId = panelId;
    bank.customGuiName = panel->name;
    customGuiSnapshotBanks.push_back(std::move(bank));
    return &customGuiSnapshotBanks.back();
}

CustomGuiSnapshotData* ofxOceanodeContainer::getCustomGuiSnapshotBySlot(const std::string& panelId, int slot)
{
    if(slot < 0) return nullptr;
    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return nullptr;
    for(auto& snapshot : bank->snapshots){
        if(snapshot.slot == slot) return &snapshot;
    }
    return nullptr;
}

const CustomGuiSnapshotData* ofxOceanodeContainer::getCustomGuiSnapshotBySlot(const std::string& panelId, int slot) const
{
    if(slot < 0) return nullptr;
    const CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return nullptr;
    for(const auto& snapshot : bank->snapshots){
        if(snapshot.slot == slot) return &snapshot;
    }
    return nullptr;
}

CustomGuiPanelData& ofxOceanodeContainer::createCustomGuiPanel(const std::string& requestedName)
{
    CustomGuiPanelData panel;
    panel.id = makeCustomGuiId();
    panel.name = makeUniqueCustomGuiName(requestedName.empty() ? "Custom GUI" : requestedName);
    panel.windowState.isOpen = true;
    panel.windowState.hasConfig = false;
    customGuiPanelsData.push_back(panel);
    rebuildCustomGuiPanels();
    markCustomGuisDirty();
    return customGuiPanelsData.back();
}

bool ofxOceanodeContainer::deleteCustomGuiPanel(const std::string& panelId)
{
    auto it = std::remove_if(customGuiPanelsData.begin(), customGuiPanelsData.end(), [&](const CustomGuiPanelData& panel){
        return panel.id == panelId;
    });
    if(it == customGuiPanelsData.end()) return false;
    customGuiPanelsData.erase(it, customGuiPanelsData.end());
    customGuiSnapshotBanks.erase(std::remove_if(customGuiSnapshotBanks.begin(), customGuiSnapshotBanks.end(), [&](const CustomGuiSnapshotBank& bank){
        return bank.customGuiId == panelId;
    }), customGuiSnapshotBanks.end());
    rebuildCustomGuiPanels();
    markCustomGuisDirty();
    markCustomGuiSnapshotsDirty();
    return true;
}

void ofxOceanodeContainer::requestDeleteCustomGuiPanel(const std::string& panelId)
{
    pendingDeletedCustomGuiPanelId = panelId;
}

void ofxOceanodeContainer::openCustomGuiPanel(const std::string& panelId, bool designMode)
{
    CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(panel == nullptr) return;
    panel->designMode = designMode;
    panel->windowState.isOpen = true;
    markCustomGuisDirty();
}

bool ofxOceanodeContainer::renameCustomGuiPanel(const std::string& panelId, const std::string& requestedName)
{
    CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(panel == nullptr) return false;

    std::string trimmedName = requestedName;
    auto begin = trimmedName.find_first_not_of(" \t\n\r");
    auto end = trimmedName.find_last_not_of(" \t\n\r");
    if(begin == std::string::npos) trimmedName.clear();
    else trimmedName = trimmedName.substr(begin, end - begin + 1);
    if(trimmedName.empty()) trimmedName = "Custom GUI";

    if(panel->name == trimmedName) return true;
    panel->name = makeUniqueCustomGuiName(trimmedName);
    if(auto* bank = getCustomGuiSnapshotBank(panelId)){
        bank->customGuiName = panel->name;
        markCustomGuiSnapshotsDirty();
    }
    markCustomGuisDirty();
    return true;
}

bool ofxOceanodeContainer::customGuiPanelHasSnapshotEligibleParameters(const std::string& panelId) const
{
    const CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(panel == nullptr) return false;

    for(const auto& widget : panel->layout.widgets){
        if(widget.parameterRef.parameterPath.empty()) continue;
        auto* parameter = findCustomGuiParameter(widget.parameterRef.parameterPath);
        if(parameter == nullptr) continue;
        if(isCustomGuiSnapshotSupportedType(parameter->valueType())) return true;
    }
    return false;
}

std::vector<CustomGuiWidgetType> ofxOceanodeContainer::getCompatibleCustomGuiWidgetTypes(ofxOceanodeAbstractParameter& parameter) const
{
    ofxOceanodeCustomGuiPanel tempPanel(const_cast<ofxOceanodeContainer&>(*this), "");
    return tempPanel.getCompatibleWidgetTypes(parameter);
}

CustomGuiWidgetType ofxOceanodeContainer::getDefaultCustomGuiWidgetType(ofxOceanodeAbstractParameter& parameter) const
{
    ofxOceanodeCustomGuiPanel tempPanel(const_cast<ofxOceanodeContainer&>(*this), "");
    return tempPanel.getDefaultWidgetType(parameter);
}

std::string ofxOceanodeContainer::getCustomGuiParameterPath(ofxOceanodeAbstractParameter& parameter) const
{
    auto* model = parameter.getNodeModel();
    if(model == nullptr) return parameter.getEscapedName();
    return model->getParameterGroup().getEscapedName() + "/" + parameter.getEscapedName();
}

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findCustomGuiParameter(const std::string& parameterPath) const
{
    auto resolved = const_cast<ofxOceanodeContainer*>(this)->resolveParameterFromPath(parameterPath, "");
    return resolved.parameter;
}

bool ofxOceanodeContainer::showParameterInCanvas(ofxOceanodeAbstractParameter& parameter)
{
    auto* node = getNodeFromParameter(parameter);
    if(node == nullptr) return false;
    return showNodeInCanvas(*node);
}

bool ofxOceanodeContainer::showNodeInCanvas(ofxOceanodeNode& targetNode)
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

    for(auto& pair : location.container->getParameterGroupNodesMap()){
        pair.second->getNodeGui().setSelected(false);
    }
    location.container->deselectAllComments();
    location.node->getNodeGui().setSelected(true);

    if(location.macro != nullptr){
        location.macro->activateWindow();
        location.canvas = location.macro->getCanvas();
    }else{
        location.canvas->requestFocus();
    }

    location.canvas->bringOnTop();
    ofxOceanodeShared::setActiveCanvasUniqueID(location.canvas->getUniqueID());
    ofxOceanodeShared::nodeSelectedInCanvas(location.node);
    ofxOceanodeShared::getLayoutSwitchSuppressFrames() = 4;

    if(ofxOceanodeShared::getGuiLayoutChangesWithMacros()){
        string newIniPath = ofToDataPath(location.canvas->getLayoutIniPath());
        string& activeLayoutPath = ofxOceanodeShared::getActiveCanvasLayoutPath();
        if(!newIniPath.empty() && newIniPath != activeLayoutPath){
            ofxOceanodeShared::getPendingLayoutSavePath() = activeLayoutPath;
            ofxOceanodeShared::getPendingLayoutLoadPath() = newIniPath;
            activeLayoutPath = newIniPath;
        }
    }

    location.canvas->requestCenterOnNode(location.node,
                                         location.macro != nullptr ? 2 : 1);
    return true;
}

bool ofxOceanodeContainer::addParameterToCustomGui(const std::string& panelId, ofxOceanodeAbstractParameter& parameter, CustomGuiWidgetType type)
{
    ofxOceanodeCustomGuiPanel tempPanel(*this, panelId);
    return tempPanel.addParameter(parameter, type);
}

bool ofxOceanodeContainer::removeParameterFromCustomGui(const std::string& panelId, ofxOceanodeAbstractParameter& parameter)
{
    ofxOceanodeCustomGuiPanel tempPanel(*this, panelId);
    return tempPanel.removeParameter(getCustomGuiParameterPath(parameter));
}

bool ofxOceanodeContainer::customGuiContainsParameter(const std::string& panelId, ofxOceanodeAbstractParameter& parameter) const
{
    ofxOceanodeCustomGuiPanel tempPanel(const_cast<ofxOceanodeContainer&>(*this), panelId);
    return tempPanel.containsParameter(parameter);
}

std::string ofxOceanodeContainer::createCustomGuiSnapshot(const std::string& panelId, const std::string& requestedName)
{
    CustomGuiSnapshotBank* bank = getOrCreateCustomGuiSnapshotBank(panelId);
    const CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(bank == nullptr || panel == nullptr) return "";

    CustomGuiSnapshotData snapshot;
    snapshot.id = makeCustomGuiSnapshotId();
    snapshot.name = makeUniqueCustomGuiSnapshotName(*bank, trimCustomGuiSnapshotName(requestedName, "Snapshot"));
    snapshot.slot = getNextAvailableCustomGuiSnapshotSlot(*bank);

    for(const auto& widget : panel->layout.widgets){
        if(widget.parameterRef.parameterPath.empty()) continue;
        ofxOceanodeAbstractParameter* parameter = findCustomGuiParameter(widget.parameterRef.parameterPath);
        if(parameter == nullptr) continue;
        if(!isCustomGuiSnapshotSupportedType(parameter->valueType())) continue;

        CustomGuiSnapshotValue snapshotValue;
        RouterSnapshot routerSnapshot = MacroRouterValueDispatch::captureValue(parameter);
        snapshotValue.type = routerSnapshot.type;
        snapshotValue.value = routerSnapshot.value;
        snapshot.parameterValues[widget.parameterRef.parameterPath] = std::move(snapshotValue);
    }

    if(snapshot.parameterValues.empty()) return "";

    bank->customGuiName = panel->name;
    bank->snapshots.push_back(std::move(snapshot));
    std::sort(bank->snapshots.begin(), bank->snapshots.end(), [](const CustomGuiSnapshotData& a, const CustomGuiSnapshotData& b){
        return a.slot < b.slot;
    });
    bank->currentSnapshotId = bank->snapshots.back().id;
    markCustomGuiSnapshotsDirty();
    return bank->currentSnapshotId;
}

bool ofxOceanodeContainer::updateCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId)
{
    if(snapshotId.empty()) return false;
    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    const CustomGuiPanelData* panel = getCustomGuiPanelData(panelId);
    if(bank == nullptr || panel == nullptr) return false;

    auto it = std::find_if(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
        return snapshot.id == snapshotId;
    });
    if(it == bank->snapshots.end()) return false;

    std::map<std::string, CustomGuiSnapshotValue> capturedValues;
    for(const auto& widget : panel->layout.widgets){
        if(widget.parameterRef.parameterPath.empty()) continue;
        ofxOceanodeAbstractParameter* parameter = findCustomGuiParameter(widget.parameterRef.parameterPath);
        if(parameter == nullptr) continue;
        if(!isCustomGuiSnapshotSupportedType(parameter->valueType())) continue;

        RouterSnapshot routerSnapshot = MacroRouterValueDispatch::captureValue(parameter);
        capturedValues[widget.parameterRef.parameterPath] = {routerSnapshot.type, routerSnapshot.value};
    }

    if(capturedValues.empty()) return false;
    it->parameterValues = std::move(capturedValues);
    bank->customGuiName = panel->name;
    bank->currentSnapshotId = snapshotId;
    markCustomGuiSnapshotsDirty();
    return true;
}

std::string ofxOceanodeContainer::storeCustomGuiSnapshotToSlot(const std::string& panelId, int slot, const std::string& requestedName)
{
    if(slot < 0) return "";

    if(CustomGuiSnapshotData* existing = getCustomGuiSnapshotBySlot(panelId, slot)){
        std::string snapshotId = existing->id;
        if(!requestedName.empty()) renameCustomGuiSnapshot(panelId, snapshotId, requestedName);
        if(updateCustomGuiSnapshot(panelId, snapshotId)){
            return snapshotId;
        }
        return "";
    }

    std::string createdSnapshotId = createCustomGuiSnapshot(panelId, requestedName.empty() ? ("Snapshot " + ofToString(slot + 1)) : requestedName);
    if(createdSnapshotId.empty()) return "";

    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return createdSnapshotId;
    auto it = std::find_if(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
        return snapshot.id == createdSnapshotId;
    });
    if(it == bank->snapshots.end()) return createdSnapshotId;

    it->slot = slot;
    std::sort(bank->snapshots.begin(), bank->snapshots.end(), [](const CustomGuiSnapshotData& a, const CustomGuiSnapshotData& b){
        return a.slot < b.slot;
    });
    bank->currentSnapshotId = createdSnapshotId;
    markCustomGuiSnapshotsDirty();
    return createdSnapshotId;
}

bool ofxOceanodeContainer::recallCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId)
{
    if(snapshotId.empty()) return false;
    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return false;

    auto it = std::find_if(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
        return snapshot.id == snapshotId;
    });
    if(it == bank->snapshots.end()) return false;

    bool appliedAny = false;
    for(const auto& pair : it->parameterValues){
        ofxOceanodeAbstractParameter* parameter = findCustomGuiParameter(pair.first);
        if(parameter == nullptr) continue;
        if(!isCustomGuiSnapshotSupportedType(parameter->valueType())) continue;

        RouterSnapshot routerSnapshot;
        routerSnapshot.type = pair.second.type;
        routerSnapshot.value = pair.second.value;

        try {
            MacroRouterValueDispatch::applyValue(parameter, routerSnapshot);
            appliedAny = true;
        } catch(const std::exception& e) {
            ofLogWarning("ofxOceanodeContainer") << "Error recalling custom GUI snapshot parameter " << pair.first << ": " << e.what();
        }
    }

    if(appliedAny){
        bank->currentSnapshotId = snapshotId;
        markCustomGuiSnapshotsDirty();
    }
    return appliedAny;
}

bool ofxOceanodeContainer::renameCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId, const std::string& requestedName)
{
    if(snapshotId.empty()) return false;
    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return false;

    auto it = std::find_if(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
        return snapshot.id == snapshotId;
    });
    if(it == bank->snapshots.end()) return false;

    const std::string fallbackName = it->name.empty() ? "Snapshot" : it->name;
    std::string trimmedName = trimCustomGuiSnapshotName(requestedName, fallbackName);
    if(trimmedName == it->name) return true;

    auto baseName = trimmedName;
    auto nameExists = [&](const std::string& name){
        return std::any_of(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
            return snapshot.id != snapshotId && snapshot.name == name;
        });
    };

    if(nameExists(trimmedName)){
        for(int i = 2; ; i++){
            const std::string numbered = baseName + " " + ofToString(i);
            if(!nameExists(numbered)){
                trimmedName = numbered;
                break;
            }
        }
    }

    it->name = trimmedName;
    markCustomGuiSnapshotsDirty();
    return true;
}

bool ofxOceanodeContainer::deleteCustomGuiSnapshot(const std::string& panelId, const std::string& snapshotId)
{
    if(snapshotId.empty()) return false;
    CustomGuiSnapshotBank* bank = getCustomGuiSnapshotBank(panelId);
    if(bank == nullptr) return false;

    auto it = std::remove_if(bank->snapshots.begin(), bank->snapshots.end(), [&](const CustomGuiSnapshotData& snapshot){
        return snapshot.id == snapshotId;
    });
    if(it == bank->snapshots.end()) return false;

    const bool wasCurrent = bank->currentSnapshotId == snapshotId;
    bank->snapshots.erase(it, bank->snapshots.end());
    if(bank->snapshots.empty()){
        customGuiSnapshotBanks.erase(std::remove_if(customGuiSnapshotBanks.begin(), customGuiSnapshotBanks.end(), [&](const CustomGuiSnapshotBank& candidate){
            return candidate.customGuiId == panelId;
        }), customGuiSnapshotBanks.end());
    }else if(wasCurrent){
        bank->currentSnapshotId = bank->snapshots.front().id;
    }

    markCustomGuiSnapshotsDirty();
    return true;
}

bool ofxOceanodeContainer::recallCustomGuiSnapshotSlot(const std::string& panelId, int slot)
{
    const CustomGuiSnapshotData* snapshot = getCustomGuiSnapshotBySlot(panelId, slot);
    if(snapshot == nullptr) return false;
    return recallCustomGuiSnapshot(panelId, snapshot->id);
}

void ofxOceanodeContainer::requestCreateCustomGui(const std::string& parameterPath, CustomGuiWidgetType type, bool openInEdit)
{
    pendingCustomGuiName = makeUniqueCustomGuiName("Custom GUI");
    pendingCustomGuiParameterPath = parameterPath;
    pendingCustomGuiWidgetType = type;
    pendingCustomGuiOpenInEdit = openInEdit;
    customGuiCreateModalOpen = true;
}

void ofxOceanodeContainer::drawCustomGuiCreationModal()
{
    if(customGuiCreateModalOpen){
        ImGui::OpenPopup("Create Custom GUI");
        customGuiCreateModalOpen = false;
    }

    bool keepOpen = true;
    if(ImGui::BeginPopupModal("Create Custom GUI", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize)){
        char nameBuffer[256];
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", pendingCustomGuiName.c_str());
        if(ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))){
            pendingCustomGuiName = nameBuffer;
        }

        if(ImGui::Button("Create")){
            auto& panel = createCustomGuiPanel(pendingCustomGuiName);
            if(!pendingCustomGuiParameterPath.empty()){
                ofxOceanodeAbstractParameter* parameter = findCustomGuiParameter(pendingCustomGuiParameterPath);
                if(parameter != nullptr){
                    addParameterToCustomGui(panel.id, *parameter, pendingCustomGuiWidgetType);
                }
            }
            openCustomGuiPanel(panel.id, pendingCustomGuiOpenInEdit);
            pendingCustomGuiParameterPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")){
            pendingCustomGuiParameterPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

vector<ofxOceanodeNode*> ofxOceanodeContainer::getSelectedModules(){
    vector<ofxOceanodeNode*> modulesToCopy;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getNodeGui().getSelected()){
                modulesToCopy.push_back(node.second.get());
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getNodeGui().getSelected()){
                modulesToCopy.push_back(node.second.get());
            }
        }
    }
    return modulesToCopy;
}

vector<ofxOceanodeNode*> ofxOceanodeContainer::getAllModules(){
    vector<ofxOceanodeNode*> modulesToCopy;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second)
        {
            modulesToCopy.push_back(node.second.get());
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            modulesToCopy.push_back(node.second.get());
        }
    }
    return modulesToCopy;
}

ofxOceanodeNodeGui* ofxOceanodeContainer::getGuiFromModel(ofxOceanodeNodeModel* model){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second)
        {
            if(&node.second->getNodeModel() == model){
                return &node.second->getNodeGui();
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(&node.second->getNodeModel() == model){
                return &node.second->getNodeGui();
            }
        }
    }
    return nullptr;
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

bool ofxOceanodeContainer::deleteSelectedModules(){
    for(auto &m : getSelectedModules()) m->deleteSelf();
    
    // Also delete selected comments
    auto selectedCommentIndices = getSelectedCommentIndices();
    // Delete in reverse order to maintain correct indices
    for(int i = selectedCommentIndices.size() - 1; i >= 0; i--){
        comments.erase(comments.begin() + selectedCommentIndices[i]);
    }
    
    if(getSelectedModules().size() > 0 || selectedCommentIndices.size() > 0) return true;
    return false;
}

vector<int> ofxOceanodeContainer::getSelectedCommentIndices(){
    vector<int> selectedIndices;
    for(int i = 0; i < comments.size(); i++){
        if(comments[i].selected){
            selectedIndices.push_back(i);
        }
    }
    return selectedIndices;
}

void ofxOceanodeContainer::deselectAllComments(){
    for(auto &c : comments){
        c.selected = false;
    }
}

#ifdef OFXOCEANODE_USE_MIDI
//TODO: Review for ofxOceanodeParameter
void ofxOceanodeContainer::setIsListeningMidi(bool b){
    isListeningMidi = b;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent, int _id){
    string name = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    
    if(_id == -1){
        _id = midiBindings[name].size();
    }
    
    shared_ptr<ofxOceanodeAbstractMidiBinding> midiBinding = nullptr;
    if(p.valueType() == typeid(float).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<float>>(p.cast<float>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(int).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<int>>(p.cast<int>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(bool).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<bool>>(p.cast<bool>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(void).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<void>>(p.cast<void>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<float>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<float>>>(p.cast<vector<float>>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<int>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<int>>>(p.cast<vector<int>>().getParameter(), _id);
    }
    if(midiBinding != nullptr){
        for(auto &midiInPair : midiIns){
            midiInPair.second.addListener(midiBinding.get());
        }
        midiUnregisterlisteners.push(midiBinding->unregisterUnusedMidiIns.newListener(this, &ofxOceanodeContainer::midiBindingBound));
        if(!isPersistent){
            midiBindings[name].push_back(midiBinding);
        }else{
            persistentMidiBindings[name].push_back(midiBinding);
        }
        return midiBinding;
    }
    return nullptr;
}

bool ofxOceanodeContainer::removeLastMidiBinding(ofxOceanodeAbstractParameter &p){
    string midiBindingName = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    if(midiBindings.count(midiBindingName) != 0){
        for(auto &midiInPair : midiIns){
            midiInPair.second.removeListener(midiBindings[midiBindingName].back().get());
        }
        midiBindings[midiBindingName].pop_back();
        if(midiBindings[midiBindingName].size() == 0){
            midiBindings.erase(midiBindingName);
        }
        return true;
    }
    return false;
}

void ofxOceanodeContainer::midiBindingBound(const void * sender, string &portName){
    ofxOceanodeAbstractMidiBinding * midiBinding = static_cast <ofxOceanodeAbstractMidiBinding *> (const_cast <void *> (sender));
    for(auto &midiInPair : midiIns){
        if(midiInPair.first != portName){
            midiInPair.second.removeListener(midiBinding);
        }
    }
    if(midiOuts.count(portName) != 0){
        midiBinding->bindParameter();
        midiSenderListeners.push(midiBinding->midiMessageSender.newListener([this, portName](ofxMidiMessage& message){
            switch(message.status){
                case MIDI_CONTROL_CHANGE:{
                    midiOuts[portName].sendControlChange(message.channel, message.control, message.value);
                    break;
                }
                case MIDI_NOTE_ON:{
                    midiOuts[portName].sendNoteOn(message.channel, message.pitch, message.velocity);
                }
                default:{
                    
                }
            }
        }));
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBindingFromInfo(string module, string parameter, bool isPersistent, int _id){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    string moduleId = ofSplitString(module, "_").back();
    module.erase(module.rfind(moduleId)-1);
    ofStringReplace(module, "_", " ");
    if(collection.count(module) != 0){
        if(collection[module].count(ofToInt(moduleId))){
            if(collection[module][ofToInt(moduleId)]->getParameters().contains(parameter)){
                return createMidiBinding(static_cast<ofxOceanodeAbstractParameter&>(collection[module][ofToInt(moduleId)]->getParameters().get(parameter)), isPersistent, _id);
            }
        }
    }
	return nullptr;
}

void ofxOceanodeContainer::addNewMidiMessageListener(ofxMidiListener* listener){
    for(auto &midiInPair : midiIns){
        midiInPair.second.addListener(listener);
    }
}

#endif

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter, bool active){
    auto sourceModuleRef = parameterGroupNodesMap.count(sourceModule) == 1 ? parameterGroupNodesMap[sourceModule] : nullptr;
    auto sinkModuleRef = parameterGroupNodesMap.count(sinkModule) == 1 ? parameterGroupNodesMap[sinkModule] : nullptr;
    if(sourceModuleRef == nullptr || sinkModuleRef == nullptr) return nullptr;
    if(sourceModuleRef->getParameters().contains(sourceParameter) && sinkModuleRef->getParameters().contains(sinkParameter)){
        ofAbstractParameter &source = sourceModuleRef->getParameters().get(sourceParameter);
        ofAbstractParameter &sink = sinkModuleRef->getParameters().get(sinkParameter);
        return createConnection(static_cast<ofxOceanodeAbstractParameter &>(source), static_cast<ofxOceanodeAbstractParameter &>(sink), active);
    }
    return nullptr;
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active){
    ofxOceanodeAbstractConnection* connection = nullptr;
    if(source.valueType() == typeid(float).name()){
        if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<vector<int>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<float>(), sink, active);
        }
    }else if(source.valueType() == typeid(int).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<vector<int>>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<int>(), sink, active);
        }
    }else if(source.valueType() == typeid(vector<float>).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<vector<int>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<vector<float>>(), sink, active);
        }
    }else if(source.valueType() == typeid(vector<int>).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<vector<float>>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<vector<int>>(), sink, active);
        }
    }else if(source.valueType() == typeid(void).name()){
        if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<bool>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<float>(), active);
        }
		else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<vector<int>>(), active);
        }
		else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<void>(), sink, active);
        }
    }
    if(connection == nullptr){
        connection = typesRegistry->createCustomTypeConnection(*this, source, sink, active);
    }
    return connection;
}


///////////////////////ENCAPSULATE
///
///


void ofxOceanodeContainer::encapsulateSelectedNodes(const string& macroName) {
	// 1. Validate selection
	auto selectedNodes = getSelectedModules();
	if(selectedNodes.empty()) {
		ofLogWarning("Encapsulation") << "No nodes selected for encapsulation";
		return;
	}
	
	ofLogNotice("Encapsulation") << "Encapsulating " << selectedNodes.size() << " nodes";
	
	// 2. Calculate center position for macro placement
	glm::vec2 centerPos(0, 0);
	for(auto node : selectedNodes) {
		centerPos += node->getNodeGui().getPosition();
	}
	if(!selectedNodes.empty()) {
		centerPos /= selectedNodes.size();
	}
	
	// 3. CRITICAL FIX: Store node information BEFORE cutting them
	// We need to preserve the node info before the nodes are deleted
	struct NodeInfo {
		string originalName;
		string originalEscapedName;
		string nodeType;
		glm::vec2 position;
	};
	
	vector<NodeInfo> originalNodeInfo;
	for(auto node : selectedNodes) {
		NodeInfo info;
		info.originalName = node->getParameters().getName();
		info.originalEscapedName = node->getParameters().getEscapedName();
		info.nodeType = node->getNodeModel().nodeName();
		info.position = node->getNodeGui().getPosition();
		originalNodeInfo.push_back(info);
	}
	
	// 4. Analyze external connections BEFORE cutting (nodes are still valid)
	auto externalConnections = analyzeExternalConnections(selectedNodes);
	ofLogNotice("Encapsulation") << "Found " << externalConnections.size() << " external connections";
	
	// 5. Cut selected nodes (this deletes them from memory)
	if(!cutSelectedModulesWithConnections()) {
		ofLogError("Encapsulation") << "Failed to cut selected nodes";
		return;
	}
	
	// 6. Create macro node
	auto macroNode = createNodeFromName("Macro");
	if(!macroNode) {
		ofLogError("Encapsulation") << "Failed to create macro node";
		return;
	}
	
	macroNode->getNodeGui().setPosition(centerPos);
	macroNode->getNodeGui().setSelected(true);

	// 7. Get macro internals
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Failed to cast to macro model";
		macroNode->deleteSelf();
		return;
	}
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Failed to get macro container";
		macroNode->deleteSelf();
		return;
	}
	
	// 8. Paste nodes inside macro
	if(!macroContainer->pasteModulesAndConnectionsInPosition(glm::vec2(0, 0), false)) {
		ofLogWarning("Encapsulation") << "Failed to paste nodes inside macro";
	}
	
	// CRITICAL FIX: Replace the node mapping section in encapsulateSelectedNodes()
	// The issue is that the mapping logic doesn't work correctly with macros

	// 9. FIXED: Update external connections using stored node info
	auto macroNodes = macroContainer->getAllModules();
	
	// MACRO-AWARE FIX: Ensure any macro nodes have their parameters properly exposed
	ensureMacroParametersExposed(macroNodes);
	
	// Create a mapping from original node names to new node names
	// CRITICAL FIX: Use a smarter mapping strategy for macros
	map<string, string> nodeNameMapping;
	map<string, string> escapedNodeNameMapping;
		
	if(!macroNodes.empty()) {
		
		ofLogNotice("Encapsulation") << "Creating node name mapping from stored info...";
		
		// For macros, we need to match based on exposed parameters, not just type and order
		// because the order might change and macros have unique parameter sets
		
		// Collect the pasted nodes by type
		map<string, vector<ofxOceanodeNode*>> pastedNodesByType;
		for(auto node : macroNodes) {
			string nodeType = node->getNodeModel().nodeName();
			pastedNodesByType[nodeType].push_back(node);
		}
		
		// For each original node, find its best match in the pasted nodes
		for(size_t i = 0; i < originalNodeInfo.size(); i++) {
			const auto& origInfo = originalNodeInfo[i];
			string originalName = origInfo.originalName;
			string nodeType = origInfo.nodeType;
			
			if(nodeType == "Macro") {
				// SPECIAL HANDLING FOR MACROS: Match by parameter similarity
				ofLogNotice("Encapsulation") << "Finding macro match for: " << originalName;
				
				// Find which parameters this original macro was involved in
				set<string> originalMacroParams;
				for(const auto& extConn : externalConnections) {
					if(extConn.isIncoming) {
						for(const auto& internalConn : extConn.internalConnections) {
							if(internalConn.nodeName == originalName) {
								originalMacroParams.insert(internalConn.paramName);
							}
						}
					} else {
						if(extConn.internalConnection.nodeName == originalName) {
							originalMacroParams.insert(extConn.internalConnection.paramName);
						}
					}
				}
				
				ofLogNotice("Encapsulation") << "Original macro " << originalName << " used parameters:";
				for(const auto& param : originalMacroParams) {
					ofLogNotice("Encapsulation") << "  - " << param;
				}
				
				// Find the best matching macro among the pasted ones
				ofxOceanodeNode* bestMatch = nullptr;
				int bestScore = -1;
				
				for(auto pastedMacro : pastedNodesByType[nodeType]) {
					if(nodeNameMapping.find(pastedMacro->getParameters().getName()) != nodeNameMapping.end()) {
						continue; // Already mapped
					}
					
					auto& pastedParams = pastedMacro->getParameters();
					int score = 0;
					
					ofLogNotice("Encapsulation") << "Checking pasted macro: " << pastedMacro->getParameters().getName();
					ofLogNotice("Encapsulation") << "Available parameters:";
					for(auto& param : pastedParams) {
						ofLogNotice("Encapsulation") << "  - " << param->getName();
						if(originalMacroParams.count(param->getName()) > 0) {
							score++;
							ofLogNotice("Encapsulation") << "    MATCH!";
						}
					}
					
					ofLogNotice("Encapsulation") << "Score: " << score << "/" << originalMacroParams.size();
					
					if(score > bestScore) {
						bestScore = score;
						bestMatch = pastedMacro;
					}
				}
				
				if(bestMatch) {
					string newName = bestMatch->getParameters().getName();
					nodeNameMapping[originalName] = newName;
					escapedNodeNameMapping[origInfo.originalEscapedName] = bestMatch->getParameters().getEscapedName();
					// Mark this pasted node as used by adding reverse mapping
					nodeNameMapping[newName] = originalName;
					
					ofLogNotice("Encapsulation") << "MACRO MATCH: " << originalName << " → " << newName
						<< " (score: " << bestScore << "/" << originalMacroParams.size() << ")";
				} else {
					ofLogError("Encapsulation") << "No good match found for macro: " << originalName;
				}
			} else {
				// Regular node logic (unchanged)
				if(pastedNodesByType[nodeType].size() > 0) {
					// Count how many nodes of this type we've seen before this one
					int typeIndex = 0;
					for(size_t j = 0; j < i; j++) {
						if(originalNodeInfo[j].nodeType == nodeType) {
							typeIndex++;
						}
					}
					
					if(typeIndex < pastedNodesByType[nodeType].size()) {
						auto* mappedNode = pastedNodesByType[nodeType][typeIndex];
						string newName = mappedNode->getParameters().getName();
						nodeNameMapping[originalName] = newName;
						escapedNodeNameMapping[origInfo.originalEscapedName] = mappedNode->getParameters().getEscapedName();
						
						ofLogNotice("Encapsulation") << "REGULAR MATCH: " << originalName << " → " << newName;
					}
				}
			}
		}

		{
			auto remapParameterPath = [&](const string& parameterPath) -> string {
				size_t separatorPos = parameterPath.find('/');
				if(separatorPos == string::npos) return "";
				string originalGroupName = parameterPath.substr(0, separatorPos);
				auto mappingIt = escapedNodeNameMapping.find(originalGroupName);
				if(mappingIt == escapedNodeNameMapping.end()) return "";
				return mappingIt->second + parameterPath.substr(separatorPos);
			};

			set<string> selectedEscapedNodeNames;
			for(const auto& info : originalNodeInfo) {
				selectedEscapedNodeNames.insert(info.originalEscapedName);
			}

			vector<CustomGuiPanelData> migratedPanels;
			set<string> migratedPanelIds;
            map<string, pair<string, string>> migratedPanelIdMapping;
			for(const auto& panel : customGuiPanelsData) {
				bool hasParameterWidgets = false;
				bool fullyContained = true;
				bool fullyRemappable = true;
				CustomGuiPanelData migratedPanel = panel;

				for(auto& widget : migratedPanel.layout.widgets) {
					if(widget.parameterRef.parameterPath.empty()) continue;

					hasParameterWidgets = true;
					size_t separatorPos = widget.parameterRef.parameterPath.find('/');
					if(separatorPos == string::npos) {
						fullyContained = false;
						break;
					}

					string originalGroupName = widget.parameterRef.parameterPath.substr(0, separatorPos);
					if(selectedEscapedNodeNames.find(originalGroupName) == selectedEscapedNodeNames.end()) {
						fullyContained = false;
						break;
					}

					string remappedPath = remapParameterPath(widget.parameterRef.parameterPath);
					if(remappedPath.empty()) {
						fullyRemappable = false;
						break;
					}

					ofxOceanodeAbstractParameter* remappedParameter = macroContainer->findCustomGuiParameter(remappedPath);
					if(remappedParameter == nullptr) {
						fullyRemappable = false;
						break;
					}

					widget.parameterRef.parameterPath = remappedPath;
					widget.parameterRef.parameterDisplayName = remappedParameter->getName();
					if(auto* remappedNode = macroContainer->getNodeFromParameter(*remappedParameter)) {
						widget.parameterRef.nodeDisplayName = remappedNode->getParameters().getName();
					}
				}

				if(hasParameterWidgets && fullyContained && fullyRemappable) {
                    const string originalPanelId = migratedPanel.id;
					migratedPanel.id = macroContainer->makeCustomGuiId();
					migratedPanel.name = macroContainer->makeUniqueCustomGuiName(panel.name);
                    migratedPanelIdMapping[originalPanelId] = {migratedPanel.id, migratedPanel.name};
					migratedPanels.push_back(std::move(migratedPanel));
					migratedPanelIds.insert(panel.id);
				}
			}

			if(!migratedPanels.empty()) {
				customGuiPanelsData.erase(
					std::remove_if(customGuiPanelsData.begin(), customGuiPanelsData.end(), [&](const CustomGuiPanelData& panel){
						return migratedPanelIds.find(panel.id) != migratedPanelIds.end();
					}),
					customGuiPanelsData.end()
				);

				for(auto& panel : migratedPanels) {
					macroContainer->customGuiPanelsData.push_back(std::move(panel));
				}

                for(const auto& pair : migratedPanelIdMapping) {
                    auto bankIt = std::find_if(customGuiSnapshotBanks.begin(), customGuiSnapshotBanks.end(), [&](const CustomGuiSnapshotBank& bank){
                        return bank.customGuiId == pair.first;
                    });
                    if(bankIt == customGuiSnapshotBanks.end()) continue;

                    CustomGuiSnapshotBank migratedBank = *bankIt;
                    migratedBank.customGuiId = pair.second.first;
                    migratedBank.customGuiName = pair.second.second;
                    macroContainer->customGuiSnapshotBanks.push_back(std::move(migratedBank));
                }

                customGuiSnapshotBanks.erase(
                    std::remove_if(customGuiSnapshotBanks.begin(), customGuiSnapshotBanks.end(), [&](const CustomGuiSnapshotBank& bank){
                        return migratedPanelIds.find(bank.customGuiId) != migratedPanelIds.end();
                    }),
                    customGuiSnapshotBanks.end()
                );

				rebuildCustomGuiPanels();
				macroContainer->rebuildCustomGuiPanels();
				markCustomGuisDirty();
				macroContainer->markCustomGuisDirty();
                markCustomGuiSnapshotsDirty();
                macroContainer->markCustomGuiSnapshotsDirty();

				ofLogNotice("Encapsulation") << "Migrated " << migratedPanelIds.size() << " Custom GUI panel(s) into new macro";
			}
		}
	}

	if(!externalConnections.empty()) {
		// Update external connections using the mapping
		for(auto& extConn : externalConnections) {
			if(extConn.isIncoming) {
				// Update input router internal connections
				for(auto& internalConn : extConn.internalConnections) {
					string originalName = internalConn.nodeName;
					
					if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
						string newName = nodeNameMapping[originalName];
						if(newName != originalName) {
							ofLogNotice("Encapsulation") << "Updated input: " << originalName << " → " << newName;
							internalConn.nodeName = newName;
						}
					}
				}
			} else {
				// Update output router internal connection
				string originalName = extConn.internalConnection.nodeName;
				
				if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
					string newName = nodeNameMapping[originalName];
					if(newName != originalName) {
						ofLogNotice("Encapsulation") << "Updated output: " << originalName << " → " << newName;
						extConn.internalConnection.nodeName = newName;
					}
				}
			}
		}
		
		// Compute minPosition (same logic as cutSelectedModulesWithConnections) for router positioning
		glm::vec2 minPos(FLT_MAX, FLT_MAX);
		for(const auto& info : originalNodeInfo) {
			minPos.x = std::min(minPos.x, info.position.x);
			minPos.y = std::min(minPos.y, info.position.y);
		}
		if(minPos.x == FLT_MAX) minPos = glm::vec2(0, 0);

		// 10. Create routers and reconnect
		createRoutersAndReconnect(macroNode, externalConnections, minPos);
	}
	
	ofLogNotice("Encapsulation") << "Encapsulation completed successfully";
}

vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes) {
	// For input routers: group by external parameter (one external → multiple internals)
	map<ofxOceanodeAbstractParameter*, ExternalConnection> inputRouters;
	
	// For output routers: group by internal parameter (one internal → multiple externals)
	map<pair<string, string>, ExternalConnection> outputRouters; // Key: {nodeName, paramName}
	
	// Track router names to avoid conflicts within this encapsulation
	map<string, int> nameCounters;
	
	ofLogNotice("Encapsulation") << "=== DEBUGGING CONNECTION ANALYSIS ===";
	ofLogNotice("Encapsulation") << "Total connections in container: " << connections.size();
	ofLogNotice("Encapsulation") << "Selected nodes for encapsulation: " << selectedNodes.size();
	
	// First, let's see what nodes exist in the container
	ofLogNotice("Encapsulation") << "ALL NODES in container:";
	for(auto& nodeTypeMap : dynamicNodes) {
		for(auto& node : nodeTypeMap.second) {
			ofLogNotice("Encapsulation") << "  - " << node.second->getParameters().getName()
				<< " (type: " << node.second->getNodeModel().nodeName() << ")";
		}
	}
	
	// Check if any of our selected nodes are macros or routers
	ofLogNotice("Encapsulation") << "SELECTED NODES details:";
	for(auto node : selectedNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
		
		// Check if this is a router or macro
		if(node->getNodeModel().nodeName().find("Router") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a router!";
		}
		if(node->getNodeModel().nodeName().find("Macro") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a macro!";
		}
	}
	
	ofLogNotice("Encapsulation") << "ANALYZING ALL CONNECTIONS:";
	
	int connectionIndex = 0;
	for(auto& connection : connections) {
		if(!connection) continue;
		
		auto sourceNode = getNodeFromParameter(connection->getSourceParameter());
		auto sinkNode = getNodeFromParameter(connection->getSinkParameter());
		
		if(!sourceNode || !sinkNode) {
			ofLogWarning("Encapsulation") << "Connection " << connectionIndex << ": Could not find nodes";
			connectionIndex++;
			continue;
		}
		
		bool sourceSelected = isNodeInList(sourceNode, selectedNodes);
		bool sinkSelected = isNodeInList(sinkNode, selectedNodes);
		
		string sourceNodeName = sourceNode->getParameters().getName();
		string sinkNodeName = sinkNode->getParameters().getName();
		string sourceParamName = connection->getSourceParameter().getName();
		string sinkParamName = connection->getSinkParameter().getName();
		
		ofLogNotice("Encapsulation") << "Connection " << connectionIndex << ": "
			<< sourceNodeName << "." << sourceParamName << " -> "
			<< sinkNodeName << "." << sinkParamName;
		ofLogNotice("Encapsulation") << "  Source selected: " << (sourceSelected ? "YES" : "NO")
			<< ", Sink selected: " << (sinkSelected ? "YES" : "NO");
		
		// Check if this connection involves existing routers
		bool sourceIsRouter = sourceNodeName.find("Router") != string::npos;
		bool sinkIsRouter = sinkNodeName.find("Router") != string::npos;
		bool sourceIsMacro = sourceNodeName.find("Macro") != string::npos;
		bool sinkIsMacro = sinkNodeName.find("Macro") != string::npos;
		
		if(sourceIsRouter || sinkIsRouter || sourceIsMacro || sinkIsMacro) {
			ofLogWarning("Encapsulation") << "  -> INVOLVES EXISTING ROUTER/MACRO: "
				<< "src_router=" << sourceIsRouter << ", sink_router=" << sinkIsRouter
				<< ", src_macro=" << sourceIsMacro << ", sink_macro=" << sinkIsMacro;
		}
		
		// Skip internal connections (both nodes selected)
		if(sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INTERNAL CONNECTION (preserved)";
			connectionIndex++;
			continue;
		}
		
		// Skip unrelated connections (neither node selected)
		if(!sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> UNRELATED CONNECTION (ignored)";
			connectionIndex++;
			continue;
		}
		
		// This is an external connection we need to handle
		if(sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> OUTGOING CONNECTION (needs output router)";
			
			// Check if sink is a router from previous encapsulation
			if(sinkIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Trying to connect to existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Sink: " << sinkNodeName << "." << sinkParamName;
			}
			
			string fullInternalNodeName = sourceNodeName;
			string originalInternalParamName = sourceParamName;
			auto internalKey = make_pair(fullInternalNodeName, originalInternalParamName);
			
			auto it = outputRouters.find(internalKey);
			if(it != outputRouters.end()) {
				it->second.externalConnections.push_back(&connection->getSinkParameter());
			} else {
				ExternalConnection extConn;
				extConn.routerType = connection->getSourceParameter().valueType();
				extConn.isIncoming = false;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnection.nodeName = fullInternalNodeName;
				extConn.internalConnection.paramName = originalInternalParamName;
				extConn.externalConnections.push_back(&connection->getSinkParameter());
				
				outputRouters[internalKey] = extConn;
				ofLogNotice("Encapsulation") << "    Created output router: " << extConn.routerName;
			}
		}
		else if(!sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INCOMING CONNECTION (needs input router)";
			
			// Check if source is a router from previous encapsulation
			if(sourceIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Source is existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Source: " << sourceNodeName << "." << sourceParamName;
			}
			
			auto externalParam = &connection->getSourceParameter();
			string fullInternalNodeName = sinkNodeName;
			string originalInternalParamName = sinkParamName;
			
			auto it = inputRouters.find(externalParam);
			if(it != inputRouters.end()) {
				it->second.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
			} else {
				ExternalConnection extConn;
                extConn.routerType = connection->getSourceParameter().valueType();
				extConn.externalParam = externalParam;
				extConn.isIncoming = true;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
				
				inputRouters[externalParam] = extConn;
				ofLogNotice("Encapsulation") << "    Created input router: " << extConn.routerName;
			}
		}
		
		connectionIndex++;
	}
	
	// Convert maps to vector
	vector<ExternalConnection> externals;
	for(auto& pair : inputRouters) {
		externals.push_back(pair.second);
	}
	for(auto& pair : outputRouters) {
		externals.push_back(pair.second);
	}
	
	ofLogNotice("Encapsulation") << "=== ANALYSIS COMPLETE ===";
	ofLogNotice("Encapsulation") << "Will create " << externals.size() << " routers";
	
	return externals;
}

void ofxOceanodeContainer::createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections, glm::vec2 minPosition) {
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Invalid macro model for router creation";
		return;
	}
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Invalid macro container for router creation";
		return;
	}
	
	ofLogNotice("Encapsulation") << "=== CREATING ROUTERS INSIDE MACRO ===";
	ofLogNotice("Encapsulation") << "Creating " << connections.size() << " routers inside macro...";
	
	// Get all nodes inside the macro
	auto macroNodes = macroContainer->getAllModules();
	ofLogNotice("Encapsulation") << "Found " << macroNodes.size() << " nodes inside macro:";
	for(auto node : macroNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
	}
	
	int routerIndex = 0;
	vector<ofxOceanodeNode*> createdRouters;
	
	// Create routers and connect them appropriately
	for(auto& extConn : connections) {
		try {
            string routerTypeName = "Router " + macroContainer->getTypesRegistry()->getTypeNameFromTypeDescription(extConn.routerType);
			
			ofLogNotice("Encapsulation") << "Creating router " << (routerIndex + 1) << "/" << connections.size()
				<< ": " << routerTypeName << " named '" << extConn.routerName << "'";
			
			auto routerNode = macroContainer->createNodeFromName(routerTypeName);
			if(!routerNode) {
				ofLogError("Encapsulation") << "Failed to create router: " << routerTypeName;
				continue;
			}
			
			createdRouters.push_back(routerNode);
			
			// Set router name
			auto routerModel = dynamic_cast<abstractRouter*>(&routerNode->getNodeModel());
			if(routerModel) {
				routerModel->getNameParam() = extConn.routerName;
				ofLogNotice("Encapsulation") << "Set router name to: " << extConn.routerName;
			}
			
						glm::vec2 routerPos(0, 0);
						bool posFound = false;

						if(extConn.isIncoming) {
							// Input router: position at the external source node's location, translated to macro canvas space
							if(extConn.externalParam) {
								auto externalNode = getNodeFromParameter(*extConn.externalParam);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								// Fallback: average of target internal nodes, offset left
								int count = 0;
								for(const auto& internalConn : extConn.internalConnections) {
									for(auto node : macroNodes) {
										if(node->getParameters().getName() == internalConn.nodeName) {
											routerPos += node->getNodeGui().getPosition();
											count++;
											break;
										}
									}
								}
								if(count > 0) { routerPos /= count; routerPos.x -= 200; }
								else routerPos = glm::vec2(-200, routerIndex * 80);
							}
						} else {
							// Output router: position at the external destination node's location, translated to macro canvas space
							if(!extConn.externalConnections.empty() && extConn.externalConnections[0]) {
								auto externalNode = getNodeFromParameter(*extConn.externalConnections[0]);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								routerPos = glm::vec2(400, routerIndex * 80);
							}
						}

						routerNode->getNodeGui().setPosition(routerPos);

						ofLogNotice("Encapsulation") << "Positioned " << (extConn.isIncoming ? "input" : "output")
							<< " router '" << extConn.routerName << "' at (" << routerPos.x << ", " << routerPos.y << ")";

			// Get router's Value/Val parameter (void routers use "Val", typed routers use "Value")
			auto& routerParams = routerNode->getParameters();
			string valParamName = "";
			if(routerParams.contains("Value"))    valParamName = "Value";
			else if(routerParams.contains("Val")) valParamName = "Val";

			if(valParamName.empty()) {
				ofLogError("Encapsulation") << "Router does not have Value/Val parameter!";
				ofLogError("Encapsulation") << "Available parameters:";
				for(auto& param : routerParams) {
					ofLogError("Encapsulation") << "  - " << param->getName();
				}
				continue;
			}

			auto routerParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&routerParams.get(valParamName));
			if(!routerParam) {
				ofLogError("Encapsulation") << "Could not cast " << valParamName << " parameter to ofxOceanodeAbstractParameter";
				continue;
			}
			
			if(extConn.isIncoming) {
				// Input router: connect to multiple internal parameters
				ofLogNotice("Encapsulation") << "Connecting input router to " << extConn.internalConnections.size() << " internal parameters:";
				for(const auto& internalConn : extConn.internalConnections) {
					ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, internalConn);
					if(internalParam) {
						// Check if target parameter already has a connection
						if(internalParam->hasInConnection()) {
							ofLogError("Encapsulation") << "ERROR: Target parameter " << internalConn.nodeName
								<< "." << internalConn.paramName << " already has an input connection!";
							ofLogError("Encapsulation") << "This should not happen - investigating connection analysis...";
							continue;
						}
						
						auto conn = macroContainer->createConnection(*routerParam, *internalParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Connected input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to connect input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						}
					} else {
						ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
							<< internalConn.nodeName << "." << internalConn.paramName;
					}
				}
			} else {
				// Output router: connect from single internal parameter
				ofLogNotice("Encapsulation") << "Connecting output router from internal parameter:";
				ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, extConn.internalConnection);
				if(internalParam) {
					auto conn = macroContainer->createConnection(*internalParam, *routerParam);
					if(conn) {
						ofLogNotice("Encapsulation") << "  ✓ Connected " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					} else {
						ofLogError("Encapsulation") << "  ✗ Failed to connect " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					}
				} else {
					ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
						<< extConn.internalConnection.nodeName << "." << extConn.internalConnection.paramName;
				}
			}
			
			routerIndex++;
			
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception creating/connecting router: " << e.what();
		}
	}
	
	// Update everything to process the new routers
	ofEventArgs args;
	for(auto router : createdRouters) {
		router->update(args);
	}
	macroNode->update(args);
	
	// Give time for macro to process routers and expose them as parameters
	ofSleepMillis(100);
	macroNode->update(args);
	
	// Now reconnect external connections to macro parameters
	ofLogNotice("Encapsulation") << "=== RECONNECTING EXTERNAL CONNECTIONS ===";
	
	auto& macroParams = macroNode->getParameters();
	ofLogNotice("Encapsulation") << "Available macro parameters:";
	for(auto& param : macroParams) {
		ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
	}
	
	int reconnectedCount = 0;
	for(auto& extConn : connections) {
		try {
			if(macroParams.contains(extConn.routerName)) {
				auto macroParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&macroParams.get(extConn.routerName));
				if(!macroParam) {
					ofLogError("Encapsulation") << "Could not cast macro parameter: " << extConn.routerName;
					continue;
				}
				
				if(extConn.isIncoming) {
					// Incoming: external → macro input (single connection)
					if(extConn.externalParam) {
						auto conn = createConnection(*extConn.externalParam, *macroParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Reconnected incoming: external → " << extConn.routerName;
							reconnectedCount++;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to reconnect incoming to " << extConn.routerName;
						}
					}
				} else {
					// Outgoing: macro output → multiple externals
					for(auto externalParam : extConn.externalConnections) {
						if(externalParam) {
							auto conn = createConnection(*macroParam, *externalParam);
							if(conn) {
								ofLogNotice("Encapsulation") << "  ✓ Reconnected outgoing: " << extConn.routerName << " → external";
								reconnectedCount++;
							} else {
								ofLogError("Encapsulation") << "  ✗ Failed to reconnect outgoing from " << extConn.routerName;
							}
						}
					}
				}
			} else {
				ofLogError("Encapsulation") << "Macro parameter not found: " << extConn.routerName;
			}
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception reconnecting: " << e.what();
		}
	}
	
	ofLogNotice("Encapsulation") << "=== ENCAPSULATION COMPLETE ===";
	ofLogNotice("Encapsulation") << "Successfully reconnected: " << reconnectedCount << " connections";
}

bool ofxOceanodeContainer::isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList) {
	return std::find(nodeList.begin(), nodeList.end(), node) != nodeList.end();
}

string ofxOceanodeContainer::generateRouterName(const string& paramName, map<string, int>& nameCounters) {
	// Clean parameter name
	string cleanParamName = paramName;
	ofStringReplace(cleanParamName, ".", "_");
	ofStringReplace(cleanParamName, " ", "_");
	
	// Remove consecutive underscores and trim
	while(cleanParamName.find("__") != string::npos) {
		ofStringReplace(cleanParamName, "__", "_");
	}
	if(!cleanParamName.empty() && cleanParamName[0] == '_') {
		cleanParamName = cleanParamName.substr(1);
	}
	if(!cleanParamName.empty() && cleanParamName.back() == '_') {
		cleanParamName = cleanParamName.substr(0, cleanParamName.length() - 1);
	}
	if(cleanParamName.empty()) {
		cleanParamName = "Param";
	}
	
	// Check if this name already exists in this encapsulation
	if(nameCounters.find(cleanParamName) == nameCounters.end()) {
		nameCounters[cleanParamName] = 1;
		return cleanParamName;
	} else {
		nameCounters[cleanParamName]++;
		return cleanParamName + "_" + ofToString(nameCounters[cleanParamName]);
	}
}

ofxOceanodeNode* ofxOceanodeContainer::getNodeFromParameter(ofxOceanodeAbstractParameter& param) {
	// Use the nodeModel to get the parameter group name
	auto nodeModel = param.getNodeModel();
	if(!nodeModel) return nullptr;
	
	string paramGroupName = nodeModel->getParameterGroup().getEscapedName();
	
	auto it = parameterGroupNodesMap.find(paramGroupName);
	if(it != parameterGroupNodesMap.end()) {
		return it->second;
	}
	
	return nullptr;
}

// Fix the getParameterTypeName method to properly handle nodePort:

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findInternalParameter(
	const vector<ofxOceanodeNode*>& macroNodes,
	const InternalConnection& internalConn) {
	
	ofLogVerbose("Encapsulation") << "Looking for internal parameter: " << internalConn.nodeName << "." << internalConn.paramName;
	
	for(auto node : macroNodes) {
		if(node->getParameters().getName() == internalConn.nodeName) {
			auto& nodeParams = node->getParameters();
			
			ofLogVerbose("Encapsulation") << "Found matching node: " << internalConn.nodeName;
			
			// MACRO-AWARE FIX: Check if this is a macro node
			bool isMacroNode = (node->getNodeModel().nodeName() == "Macro");
			
			if(isMacroNode) {
				ofLogNotice("Encapsulation") << "Node is a macro, looking for exposed parameter: " << internalConn.paramName;
				
				// For macro nodes, the exposed router parameters appear directly in the parameter group
				// after the macro has processed its internal routers
				
				// Force an update to ensure all router parameters are exposed
				ofEventArgs args;
				node->update(args);
				
				// Small delay to allow parameter exposure to complete
				ofSleepMillis(50);
				node->update(args);
				
				// Now check for the parameter
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogNotice("Encapsulation") << "✓ Found macro exposed parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Macro parameter not found: " << internalConn.paramName;
					ofLogError("Encapsulation") << "Available macro parameters:";
					for(auto& param : nodeParams) {
						ofLogError("Encapsulation") << "  - '" << param->getName() << "'";
					}
				}
			} else {
				// Regular node logic (unchanged)
				ofLogVerbose("Encapsulation") << "Regular node, available parameters:";
				for(auto& param : nodeParams) {
					ofLogVerbose("Encapsulation") << "  - '" << param->getName() << "'";
				}
				
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogVerbose("Encapsulation") << "✓ Found and cast parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Parameter not found in node: " << internalConn.paramName;
				}
			}
			break;
		}
	}
	
	ofLogError("Encapsulation") << "Could not find internal parameter: "
		<< internalConn.nodeName << "." << internalConn.paramName;
	return nullptr;
}

string ofxOceanodeContainer::extractNodeType(const string& nodeName) {
	// Extract base type from node name (e.g., "Vector Item Operations 2" → "Vector Item Operations")
	string nodeType = nodeName;
	auto lastSpacePos = nodeType.find_last_of(' ');
	if(lastSpacePos != string::npos) {
		string possibleId = nodeType.substr(lastSpacePos + 1);
		bool isNumeric = !possibleId.empty() && std::all_of(possibleId.begin(), possibleId.end(), ::isdigit);
		if(isNumeric) {
			nodeType = nodeType.substr(0, lastSpacePos);
		}
	}
	return nodeType;
}

void ofxOceanodeContainer::ensureMacroParametersExposed(const vector<ofxOceanodeNode*>& macroNodes) {
	ofLogNotice("Encapsulation") << "Ensuring macro parameters are exposed...";
	
	// Force multiple updates to ensure all macro router parameters are exposed
	for(int i = 0; i < 3; i++) {
		ofEventArgs args;
		for(auto node : macroNodes) {
			if(node->getNodeModel().nodeName() == "Macro") {
				node->update(args);
			}
		}
		ofSleepMillis(25); // Small delay between updates
	}
	
	// Log final parameter state
	for(auto node : macroNodes) {
		if(node->getNodeModel().nodeName() == "Macro") {
			ofLogNotice("Encapsulation") << "Macro " << node->getParameters().getName() << " exposed parameters:";
			for(auto& param : node->getParameters()) {
				ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
			}
		}
	}
}
