//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//  Snapshot management added by Santi Vilanova on January 2025
//

#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModel("Macro"){
	color = ofColor::black;
	description = "Encapsulation of a graph";
	presetPath = "";
	currentPreset = -1;
	showWindow = false;
	localPreset = true;
	lastActiveState = true;
	
	// Initialize snapshot system members
	currentSnapshotSlot = -1;
	matrixRows.set("Snapshot Matrix Rows", 2, 1, 8);
	matrixCols.set("Snapshot Matrix Cols", 8, 1, 8);
	showSnapshotNames.set("Show Names", true);
	buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f);
	showSnapshotMatrix = true;
	
	// Add minimized view update callback
	minimizedViewCallback = [this](ImVec2 size) {
		// Only show output router values in minimized view
		for(auto& routerPair : routerNodes) {
			if(!routerPair.second.isInput) {
				auto& router = routerPair.second;
				auto& params = router.node->getParameters();
				auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
				
				if(valueParam) {
					string valueStr;
					
					if(valueParam->valueType() == typeid(float).name()) {
						valueStr = ofToString(valueParam->cast<float>().getParameter().get(), 2);
					}
					else if(valueParam->valueType() == typeid(vector<float>).name()) {
						auto vec = valueParam->cast<vector<float>>().getParameter().get();
						valueStr = "[";
						for(size_t i = 0; i < vec.size(); i++) {
							valueStr += ofToString(vec[i], 2);
							if(i < vec.size() - 1) valueStr += ", ";
						}
						valueStr += "]";
					}
					else if(valueParam->valueType() == typeid(int).name()) {
						valueStr = ofToString(valueParam->cast<int>().getParameter().get());
					}
					else if(valueParam->valueType() == typeid(vector<int>).name()) {
						auto vec = valueParam->cast<vector<int>>().getParameter().get();
						valueStr = "[";
						for(size_t i = 0; i < vec.size(); i++) {
							valueStr += ofToString(vec[i]);
							if(i < vec.size() - 1) valueStr += ", ";
						}
						valueStr += "]";
					}
					
					ImGui::Text("%s: %s", router.routerName.c_str(), valueStr.c_str());
				}
			}
		}
	};
}

void ofxOceanodeNodeMacro::update(ofEventArgs &a){
	if(nextPresetPath != ""){
		localPreset = false;
		container->loadPreset(nextPresetPath);
		nextPresetPath = "";
	}
	if(active){
		container->update();
	}
}

void ofxOceanodeNodeMacro::draw(ofEventArgs &a){
	if(showWindow){
		canvas.draw(&showWindow, color, localPreset ? localName.get() : currentMacro);
	}
	if(active){
		container->draw();
	}
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
	registry = container->getRegistry();
	typesRegistry = container->getTypesRegistry();
	canvasParentID = container->getCanvasID();
}

void ofxOceanodeNodeMacro::setup(string additionalInfo){
	
	// Core parameters
	addParameter(active.set("Active", true));
	activeListener = active.newListener([this](bool &b){
		if(lastActiveState != b){
			if(b){
				container->activate();
				if(resetPhaseOnActive) container->resetPhase();
			}else{
				container->deactivate();
			}
		}
		lastActiveState = b;
	});

	auto presetControlRef = addParameter(presetControl.set("Preset Control Gui", [this](){
		
		bool addBank = false;
		
		if(localPreset){
			ImGui::Text(localName->c_str());
		}else{
			ImGui::Text("%s", currentMacro.c_str());
		}
		if(ImGui::IsItemClicked(1)){
			ImGui::OpenPopup("Macro");
		}
		if(ImGui::BeginPopup("Macro")){
			auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
			
			std::function<bool(shared_ptr<macroCategory>)> drawCategory =
			[this, &addBank, &drawCategory](shared_ptr<macroCategory> category) -> bool{
				for(auto d : category->categories){
					if(ImGui::BeginMenu(d->name.c_str())){
						if(drawCategory(d)){
							if(currentCategory.size() == 0){
								currentCategoryMacro = d;
							}
							currentCategory.push_front(d->name);
							ImGui::EndMenu();
							return true;
						}
						ImGui::EndMenu();
					}
				}
				if(category->categories.size() != 0){
					//TOD
					if(ImGui::MenuItem("Add Bank")){
						// TODO: Where? CurrentCategory?
						addBank = true;
					}
				}
				for(auto m : category->macros){
					if(ImGui::MenuItem(m.first.c_str())){
						nextPresetPath = m.second;
						currentMacroPath = m.second;
						currentMacro = m.first;
						currentCategory.clear();
						return true;
					}
				}
				return false;
			};
			
			drawCategory(macroDirectoryStructure);
			
			ImGui::EndPopup();
		}
		
		if(addBank){
			ImGui::OpenPopup("Add New Macro Bank");
		}
		
		if(ImGui::BeginPopupModal("Add New Macro Bank", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
			static char cString[256];
			if (ImGui::InputText("Bank Name", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				//                string proposedNewName(cString);
				//                ofStringReplace(proposedNewName, " ", "_");
				//                if(find(banks.begin(), banks.end(), proposedNewName) == banks.end()){
				//                    if(proposedNewName != ""){
				//                        banks.push_back(proposedNewName);
				//                        currentBank = banks.size()-1;
				//                        currentPreset[banks[currentBank]] = "";
				//                    }
				ImGui::CloseCurrentPopup();
				//TODO: New Bank
				//                }
				//                strcpy(cString, "");
			}
			if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
				strcpy(cString, "");
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		
		if(ImGui::Checkbox("Is Local?", &localPreset)){
			if(currentMacro == ""){
				localPreset = true;
			}
			if(!localPreset){
				container->loadPreset(currentMacroPath);
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("Show Window?", &showWindow);
		
		bool firstSaveAsOpen = false;
		if (ImGui::Button("[Save]")){
			ofLog() << "Save current preset and notify other macros";
			// If we try to save a new macro or a local preset, the save macro as window apears
			if(currentMacroPath == "" || localPreset){
				ImGui::OpenPopup("Save Macro As :");
				firstSaveAsOpen = true;
			}else{
				container->savePreset(currentMacroPath);
				saveSnapshots();  // Add snapshot saving
				ofxOceanodeShared::macroUpdated(currentMacroPath);
			}
		}
		
		ImGui::SameLine();
		
		if (ImGui::Button("[Save As]")){
			ImGui::OpenPopup("Save Macro As :");
			firstSaveAsOpen = true;
		}
		
		//ImGui::SetNextWindowSize(ImVec2(200,100));
		if(ImGui::BeginPopupModal("Save Macro As :", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
			static char cString[256];
			
			if(firstSaveAsOpen){
				saveAsTempCategory = currentCategory;
				ImGui::SetKeyboardFocusHere(0);
			}
			
			bool openNameAlreadyExistsPopup = false;
			if (ImGui::InputText("##Preset Name : ", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				string proposedNewName(cString);
				ofStringReplace(proposedNewName, " ", "_");
				
				
				bool nameExists = false;
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				for(int i = 0 ; i < saveAsTempCategory.size(); i++){
					string categoryNameToCompare = saveAsTempCategory[i];
					macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
															[categoryNameToCompare](shared_ptr<macroCategory> &mc){return mc->name == categoryNameToCompare;});
				}
				// TODO: Check if name exists with find_if
//				if(macroDirectoryStructure->macros.count(proposedNewName) != 0){
//					nameExists = true;
//				}
				
				if(!nameExists)
				{
					if(strcmp(proposedNewName.c_str(), "") != 0){
						string saveAsCategoryWithSlash = ofToDataPath("Macros/", true);
						for(auto s : saveAsTempCategory) saveAsCategoryWithSlash = saveAsCategoryWithSlash + s + "/";
						container->savePreset(saveAsCategoryWithSlash + string(proposedNewName));
						localPreset = false;
						currentMacro = string(proposedNewName);
						currentMacroPath = saveAsCategoryWithSlash + string(proposedNewName);
						currentCategory = saveAsTempCategory;
						currentCategoryMacro = macroDirectoryStructure;
						saveSnapshots();  // Add snapshot saving
						ofxOceanodeShared::updateMacrosStructure();
					}
					strcpy(cString, "");
					ImGui::CloseCurrentPopup();
					saveAsTempCategory.clear();
				}
				else
				{
					cout << "Preset name already existing : " << proposedNewName << endl;
					strcpy(cString, "");
					
					openNameAlreadyExistsPopup = true;
				}
			}
			if(openNameAlreadyExistsPopup){
				ImGui::OpenPopup("Preset name already exists");
			}
			
			if(ImGui::BeginPopupModal("Preset name already exists", NULL))
			{
				if (ImGui::Button("OK", ImVec2(220,0)) || (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter)) && !openNameAlreadyExistsPopup)) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
				openNameAlreadyExistsPopup = false;
			}
			
			if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
				strcpy(cString, "");
			}
			string saveAsCategoryWithSlash;
			for(auto s : saveAsTempCategory) saveAsCategoryWithSlash = saveAsCategoryWithSlash + s + "/";
			if(saveAsCategoryWithSlash == "") saveAsCategoryWithSlash = "None";
			if (ImGui::Button(saveAsCategoryWithSlash.c_str())){
				ImGui::OpenPopup("Choose Category");
			}
			
			if(ImGui::BeginPopup("Choose Category")){
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				
				std::function<bool(shared_ptr<macroCategory>)> drawCategory =
				[this, &drawCategory](shared_ptr<macroCategory> category) -> bool{
					for(auto d : category->categories){
						if(d->categories.size() == 0){
							if(ImGui::MenuItem(d->name.c_str())){
								saveAsTempCategory.clear();
								saveAsTempCategory.push_front(d->name);
								return true;
							}
						}else if(ImGui::BeginMenu(d->name.c_str())){
							if(drawCategory(d)){
								saveAsTempCategory.push_front(d->name);
								ImGui::EndMenu();
								return true;
							}
							ImGui::EndMenu();
						}
					}
					return false;
				};
				
				drawCategory(macroDirectoryStructure);
				
				ImGui::EndPopup();
			}
			if (ImGui::Button("Cancel"))
			{
				strcpy(cString, "");
				ImGui::CloseCurrentPopup();
				saveAsTempCategory.clear();
			}
			ImGui::EndPopup();
		}

		
		ImGui::Text(". . . . . . . . . . . . . . . . .");

	}));
	
	
	
	// Add snapshot parameters
	addParameter(activeSnapshotSlot.set("Snapshot", -1, -1, 64));
	
	
	addInspectorParameter(matrixRows.set("Snapshot Rows", 2, 1, 8));
	addInspectorParameter(matrixCols.set("Snapshot Cols", 8, 1, 8));
	addInspectorParameter(buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f));
	addInspectorParameter(showSnapshotNames.set("Show Names", true));
	addInspectorParameter(addSnapshotButton.set("Add Snapshot"));
	
	// Add snapshot inspector
	addInspectorParameter(snapshotInspector.set("Snapshot Names", [this]() {
		renderInspectorInterface();
	}));
	
	// Setup snapshot event listeners
	addSnapshotListener = addSnapshotButton.newListener([this](){
		int newSlot = snapshots.size();
		storeRouterSnapshot(newSlot);
	});
	
	matrixSizeListener = matrixRows.newListener([this](int& value){
		onMatrixSizeChanged(value);
	});
	
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int& slot){
		if(slot >= 0) {
			loadRouterSnapshot(slot);
		}
	});
	
	// Setup preset control
	auto presetNamingRef = addParameter(presetNaming.set("Preset Naming Gui", [this](){

//		ImGui::SameLine();
		// Render snapshot matrix
		if(showSnapshotMatrix) {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
			renderSnapshotMatrix();
			ImGui::PopStyleVar();
		}

		
		
//		ImGui::PopStyleColor(1);
		ImGui::Text(". . . . . . . . . . . . . . . . .");
	}));
	
	presetControlRef->addReceiveFunc<int>([this](const int &i){
		loadMacroInsideCategory(i);
	});
	
	presetControlRef->addReceiveFunc<float>([this](const float &f){
		loadMacroInsideCategory(floor(f));
	});
	
	presetControlRef->addReceiveFunc<vector<int>>([this](const vector<int> &vi){
		loadMacroInsideCategory(vi[0]);
	});
	
	presetControlRef->addReceiveFunc<vector<float>>([this](const vector<float> &vf){
		loadMacroInsideCategory(floor(vf[0]));
	});
	
	// Initialize container
	container = make_shared<ofxOceanodeContainer>(registry, typesRegistry);
	newNodeListener = container->newNodeCreated.newListener(this, &ofxOceanodeNodeMacro::newNodeCreated);
	
	canvas.setContainer(container);
	canvas.setup("Macro " + ofToString(getNumIdentifier()), canvasParentID);
	
	if(additionalInfo != ""){
		localPreset = false;
		container->loadPreset(additionalInfo);
		updateCurrentCategoryFromPath(additionalInfo);
		currentMacroPath = additionalInfo;
		
		// Load snapshots if they exist
		loadSnapshotsFromPath(additionalInfo);
	}
	
	macroUpdatedListener = ofxOceanodeShared::getMacroUpdatedEvent().newListener([this](string &s){
		ofLog() << s;
		if(s == currentMacroPath && !localPreset){
			container->loadPreset(currentMacroPath);
		}
	});
	
	addInspectorParameter(colorParam.set("Color", color));
	colorListener = colorParam.newListener([this](ofColor &c){
		color = c;
	});
	addInspectorParameter(localName.set("Local Name", "Local"));
	addInspectorParameter(resetPhaseOnActive.set("Reset Ph on Active", false));
}

void ofxOceanodeNodeMacro::newNodeCreated(ofxOceanodeNode* &node){
	string nodeName = node->getParameters().getName();
	if(ofSplitString(nodeName, " ")[0] == "Router"){
		auto newCreatedParam = typesRegistry->createRouterFromType(node);
		string paramName = newCreatedParam->getName();
		while (getParameterGroup().contains(paramName)) {
			paramName = "_" + paramName;
		}
		newCreatedParam->setName(paramName);
		addParameter(*newCreatedParam.get());
		
		ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
		nameParamFromRouter = paramName;
		
		parameterGroupChanged.notify(this);
		deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter](){
			getParameterGroup().remove(nameParamFromRouter);
		}, 0));
		
		// Update router info for snapshots
		updateRouterInfo(node);
	}
	ofEventArgs args;
	node->update(args);
}

void ofxOceanodeNodeMacro::macroSave(ofJson &json, string path){
	if(localPreset){
		container->savePreset(path + "/" + nodeName() + "_" + ofToString(getNumIdentifier()));
		json["LocalPreset"] = true;
		
		// Save snapshots in preset JSON for local presets
		if(!snapshots.empty()) {
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}
			json["Snapshots"] = snapshotsJson;
		}
	}else{
		json["LocalPreset"] = false;
		json["CategoryStruct"] = currentCategory;
		json["Macro"] = currentMacro;
		json["MacroPath"] = currentMacroPath;
	}
}

/**
 * macroLoad - Loads macro configuration from JSON
 *
 * CHANGES FROM ORIGINAL VERSION:
 * 1. Improved error handling:
 *    - Replaced try-catch with json.value() to provide default values
 *    - Reduces exception overhead and improves readability
 *
 * 2. Better code organization:
 *    - Added localPath variable for clarity
 *    - Improves maintainability and reduces potential for path construction errors
 *
 * 3. Added snapshot functionality:
 *    - Now loads snapshot data from JSON when available
 *    - Enables state preservation and recall within macro nodes
 *    - Supports multiple snapshots indexed by slot number
 *
 * These changes make the code more robust (better error handling),
 * more maintainable (clearer variable naming), and more powerful
 * (snapshot functionality) while maintaining backward compatibility
 * with existing presets.
 */

/* Original implementation:
void ofxOceanodeNodeMacro::macroLoad(ofJson &json, string path){
	try {
		localPreset = json["LocalPreset"];
	} catch (ofJson::exception) {
		ofLog() << "Cannot get local preset";
		localPreset = true;
	}
	if(localPreset){
		container->loadPreset(path + "/" + nodeName() + "_" + ofToString(getNumIdentifier()));
	}
}
*/
void ofxOceanodeNodeMacro::macroLoad(ofJson &json, string path){
	try {
		localPreset = json.value("LocalPreset", true);
		
		if(localPreset){
			string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
			container->loadPreset(localPath);
			
			// Load snapshots from JSON for local presets
			if(json.contains("Snapshots") && !json["Snapshots"].is_null()) {
				snapshots.clear();
				for(const auto& item : json["Snapshots"].items()) {
					int slot = ofToInt(item.key());
					SnapshotData snapshot;
					loadSnapshotFromJson(snapshot, item.value());
					snapshots[slot] = snapshot;
				}
			}
		}else{
			// TODO: Load preset from the
			auto currentCategoryVec = json["CategoryStruct"].get<deque<string>>();
			currentCategory = currentCategoryVec;
			currentMacro = json["Macro"];
			currentMacroPath = json.value("MacroPath", "");
			
			if(currentMacroPath.empty()) {
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				for(int i = 0 ; i < currentCategory.size(); i++){
					string categoryNameToCompare = currentCategory[i];
					macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
														 [categoryNameToCompare](shared_ptr<macroCategory> &mc){return mc->name == categoryNameToCompare;});
//            auto result = std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
//                                                    [categoryNameToCompare](macroCategory &mc){return mc.name == categoryNameToCompare;});
//            macroDirectoryStructure->name = (*result).name;
//            macroDirectoryStructure->macros = (*result).macros;
//            macroDirectoryStructure->categories.clear();
//            for(auto c : (*result).categories){
//                macroDirectoryStructure->categories.push_back(c);
//            }
				}
				currentCategoryMacro = macroDirectoryStructure;
				auto iter = std::find_if(currentCategoryMacro->macros.begin(), currentCategoryMacro->macros.end(), [this](const std::pair<string, string> &pair){
					return pair.first == currentMacro;
				});
				if(iter != currentCategoryMacro->macros.end()){
					// TODO: Carregar o to load?
					container->loadPreset(iter->second);
					currentMacroPath = iter->second;
				}
			} else {
				container->loadPreset(currentMacroPath);
			}
			
			// Load snapshots if they exist
			if(!currentMacroPath.empty()) {
				loadSnapshotsFromPath(currentMacroPath);
			}
		}
	} catch (ofJson::exception) {
		ofLog() << "Cannot get local preset";
		localPreset = true;
		container->loadPreset(path + "/" + nodeName() + "_" + ofToString(getNumIdentifier()));
	}
}

void ofxOceanodeNodeMacro::loadMacroInsideCategory(int newPresetIndex){
	if(newPresetIndex < currentCategoryMacro->macros.size() && currentCategoryMacro->macros[newPresetIndex].first != currentMacro){
		nextPresetPath = currentCategoryMacro->macros[newPresetIndex].second;
		currentMacroPath = nextPresetPath;
		currentMacro = currentCategoryMacro->macros[newPresetIndex].first;
	}
}

void ofxOceanodeNodeMacro::updateCurrentCategoryFromPath(string path){
#ifdef TARGET_WIN32
	vector<string> splittedInfo = ofSplitString(path, "\\");
#else
	vector<string> splittedInfo = ofSplitString(path, "/");
#endif
	currentCategory.clear();
	currentMacro = splittedInfo.back();
	for(int i = splittedInfo.size() - 2; i >= 0; i--){
		if(splittedInfo[i] != "Macros" && splittedInfo[i] != "data") {
			currentCategory.push_front(splittedInfo[i]);
		}else{
			break;
		}
	}
	
	auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
	for(int i = 0 ; i < currentCategory.size(); i++){
		string categoryNameToCompare = currentCategory[i];
		macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
											 [categoryNameToCompare](shared_ptr<macroCategory> &mc){return mc->name == categoryNameToCompare;});
	}
	currentCategoryMacro = macroDirectoryStructure;
}

void ofxOceanodeNodeMacro::loadBeforeConnections(ofJson &json){
//    container->loadPreset_loadNodes(currentMacroPath);
//    container->loadPreset_deactivateConnections();
//    container->loadPreset_loadBeforeConnections(currentMacroPath);
}

void ofxOceanodeNodeMacro::presetRecallBeforeSettingParameters(ofJson &json){
//    container->loadPreset_loadNodePreset(currentMacroPath);
//    container->loadPreset_loadConnections(currentMacroPath);
}

void ofxOceanodeNodeMacro::presetRecallAfterSettingParameters(ofJson &json){
//    container->loadPreset_midiBindings(currentMacroPath);
//    container->loadPreset_loadNodePreset(currentMacroPath);
//    container->loadPreset_loadComments(currentMacroPath);
}

/**
 * Activated presetWillBeLoaded method to support enhanced preset loading
 * and snapshot functionality. This provides proper container preparation
 * before loading presets.
 */
void ofxOceanodeNodeMacro::presetWillBeLoaded(){
	container->loadPreset_presetWillBeLoaded();
}

void ofxOceanodeNodeMacro::presetHasLoaded(){
//    container->loadPreset_presetHasLoaded();
}

void ofxOceanodeNodeMacro::activateConnections(){
//    container->loadPreset_activateConnections();
}

void ofxOceanodeNodeMacro::deactivateConnections(){
//    container->loadPreset_deactivateConnections();
}

// Snapshot-specific methods

void ofxOceanodeNodeMacro::updateRouterConnections() {
	routerNodes.clear();
	
	auto nodes = container->getAllModules();
	if(nodes.empty()) return;
	
	for(auto* node : nodes) {
		if(!node) continue;
		
		if(dynamic_cast<abstractRouter*>(&node->getNodeModel()) != nullptr) {
			updateRouterInfo(node);
		}
	}
}

void ofxOceanodeNodeMacro::updateRouterInfo(ofxOceanodeNode* node) {
	auto& params = node->getParameters();
	
	// Check if "Value" parameter exists before trying to access it
	if(params.contains("Value")) {
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
		
		if(valueParam) {
			RouterInfo info;
			info.node = node;
			info.routerName = static_cast<abstractRouter&>(node->getNodeModel()).getNameParam().get();
			info.parameterType = valueParam->valueType();
			info.isInput = checkIsInputRouter(node);
			
			routerNodes[info.routerName] = info;
		}
	}
}

bool ofxOceanodeNodeMacro::checkIsInputRouter(ofxOceanodeNode* node) {
	auto& params = node->getParameters();
	auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
	if(!valueParam) return false;
	
	return !valueParam->hasInConnection();
}

void ofxOceanodeNodeMacro::loadRouterSnapshot(int slot) {
	auto it = snapshots.find(slot);
	if(it == snapshots.end()) return;
	
	updateRouterConnections();
	const auto& values = it->second.routerValues;
	 
	activeSnapshotSlotListener.unsubscribe();
	activeSnapshotSlot = slot;
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int& slot){
		if(slot >= 0) {
			loadRouterSnapshot(slot);
		}
	});
	
	for(auto& routerPair : routerNodes) {
		if(!routerPair.second.isInput) continue;
		
		auto valueIt = values.find(routerPair.second.routerName);
		if(valueIt == values.end()) continue;
		
		auto& router = routerPair.second;
		auto& params = router.node->getParameters();
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
		if(!valueParam) continue;
		
		try {
			if(valueParam->valueType() == typeid(float).name()) {
				valueParam->cast<float>().getParameter() = valueIt->second.value.get<float>();
			}
			else if(valueParam->valueType() == typeid(vector<float>).name()) {
				valueParam->cast<vector<float>>().getParameter() = valueIt->second.value.get<vector<float>>();
			}
			else if(valueParam->valueType() == typeid(int).name()) {
				valueParam->cast<int>().getParameter() = static_cast<int>(round(valueIt->second.value.get<float>()));
			}
			else if(valueParam->valueType() == typeid(vector<int>).name()) {
				auto floatVec = valueIt->second.value.get<vector<float>>();
				vector<int> intVec;
				for(auto val : floatVec) {
					intVec.push_back(static_cast<int>(round(val)));
				}
				valueParam->cast<vector<int>>().getParameter() = intVec;
			}
			else if(valueParam->valueType() == typeid(bool).name()) {
				valueParam->cast<bool>().getParameter() = valueIt->second.value.get<bool>();
			}
			else if(valueParam->valueType() == typeid(vector<bool>).name()) {
				valueParam->cast<vector<bool>>().getParameter() = valueIt->second.value.get<vector<bool>>();
			}
			else if(valueParam->valueType() == typeid(string).name()) {
				valueParam->cast<string>().getParameter() = valueIt->second.value.get<string>();
			}
			else if(valueParam->valueType() == typeid(vector<string>).name()) {
				valueParam->cast<vector<string>>().getParameter() = valueIt->second.value.get<vector<string>>();
			}
			else if(valueParam->valueType() == typeid(ofColor).name()) {
				ofColor color;
				auto& colorJson = valueIt->second.value;
				color.r = colorJson["r"].get<int>();
				color.g = colorJson["g"].get<int>();
				color.b = colorJson["b"].get<int>();
				color.a = colorJson["a"].get<int>();
				valueParam->cast<ofColor>().getParameter() = color;
			}
			else if(valueParam->valueType() == typeid(ofFloatColor).name()) {
				ofFloatColor color;
				auto& colorJson = valueIt->second.value;
				color.r = colorJson["r"].get<float>();
				color.g = colorJson["g"].get<float>();
				color.b = colorJson["b"].get<float>();
				color.a = colorJson["a"].get<float>();
				valueParam->cast<ofFloatColor>().getParameter() = color;
			}
			else if(valueParam->valueType() == typeid(void).name()) {
				valueParam->cast<void>().getParameter().trigger();
			}
		} catch(const std::exception& e) {
			ofLogError("Snapshot") << "Error loading value: " << e.what();
		}
	}
	currentSnapshotSlot = slot;
}

void ofxOceanodeNodeMacro::renderSnapshotMatrix() {
	ImGui::PushID("Matrix");
	
	const int numRows = matrixRows;
	const int numCols = matrixCols;

	for(int i = 0; i < numRows; i++) {
		for(int j = 0; j < numCols; j++) {
			if(j > 0) ImGui::SameLine();
			
			int slot = i * numCols + j;
			ImGui::PushID(slot);
			
			bool hasData = snapshots.count(slot) > 0;
			bool isActive = (slot == currentSnapshotSlot);
			
			// Set button colors based on state
			if(isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.0f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			}
			else if(hasData) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.5f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.6f, 0.0f, 1.0f));
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

			string label = (hasData && showSnapshotNames) ? snapshots[slot].name : ofToString(slot);

			if(ImGui::Button(label.c_str(), ImVec2(buttonSize, buttonSize/1.5))) {
				if(ImGui::GetIO().KeyShift) {
					storeRouterSnapshot(slot);
				} else {
					currentSnapshotSlot = slot;
					loadRouterSnapshot(slot);
				}
			}
			
			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}
	}
	
	ImGui::PopID();
}

void ofxOceanodeNodeMacro::renderInspectorInterface() {
	// Global clear button in the inspector
	if(ImGui::Button("Clear All Snapshots", ImVec2(130, 0))) {
		ImGui::OpenPopup("Clear All Snapshots?");
	}
	
	// Handle clear all confirmation popup
	if(ImGui::BeginPopupModal("Clear All Snapshots?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Are you sure you want to clear all snapshots?\nThis action cannot be undone.");
		ImGui::Separator();
		
		if(ImGui::Button("Yes", ImVec2(120, 0))) {
			clearAllSnapshots();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Cancel", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	if(snapshots.empty()) {
		ImGui::Text("No snapshots stored");
		return;
	}

	for(auto it = snapshots.begin(); it != snapshots.end();) {
		ImGui::PushID(it->first);
		
		bool shouldDelete = false;
		char nameBuf[256];
		strcpy(nameBuf, it->second.name.c_str());
		
		// Slot number
		ImGui::Text("Slot %d", it->first);
		
		// Name input
		ImGui::SetNextItemWidth(150);
		if(ImGui::InputText("##name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
			it->second.name = nameBuf;
		}
		
		// Load button
		ImGui::SameLine();
		if(ImGui::Button("Load")) {
			loadRouterSnapshot(it->first);
		}
		
		// Clear button with confirmation
		ImGui::SameLine();
		if(ImGui::Button("Clear")) {
			ImGui::OpenPopup("Clear Snapshot?");
		}
		
		// Confirmation popup for individual clear
		if(ImGui::BeginPopup("Clear Snapshot?")) {
			ImGui::Text("Clear snapshot %d (%s)?", it->first, it->second.name.c_str());
			ImGui::Separator();
			
			if(ImGui::Button("Yes", ImVec2(120, 0))) {
				shouldDelete = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if(ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		
		ImGui::PopID();
		ImGui::Separator();
		
		// Handle deletion if confirmed
		if(shouldDelete) {
			if(currentSnapshotSlot == it->first) {
				currentSnapshotSlot = -1;
			}
			it = snapshots.erase(it);
			saveSnapshots(); // Save the updated state
		} else {
			++it;
		}
	}
}

void ofxOceanodeNodeMacro::saveSnapshots() {
	if(!currentMacroPath.empty() && !localPreset && !snapshots.empty()) {
		ofJson snapshotsJson;
		for(const auto& pair : snapshots) {
			ofJson slotJson;
			slotJson["name"] = pair.second.name;
			slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
			snapshotsJson[ofToString(pair.first)] = slotJson;
		}
		string filename = ofFilePath::removeTrailingSlash(currentMacroPath) + "/snapshots.json";
		ofSavePrettyJson(filename, snapshotsJson);
	}
}

void ofxOceanodeNodeMacro::loadSnapshotsFromPath(const string& path) {
   string snapshotsFile = ofFilePath::removeTrailingSlash(path) + "/snapshots.json";
   if(ofFile::doesFileExist(snapshotsFile)) {
	   try {
		   ofJson snapshotsJson = ofLoadJson(snapshotsFile);
		   snapshots.clear();
		   for(const auto& item : snapshotsJson.items()) {
			   int slot = ofToInt(item.key());
			   SnapshotData snapshot;
			   loadSnapshotFromJson(snapshot, item.value());
			   snapshots[slot] = snapshot;
		   }
	   } catch(const std::exception& e) {
		   ofLogError("Macro") << "Error loading snapshots: " << e.what();
	   }
   }
}

void ofxOceanodeNodeMacro::loadSnapshotFromJson(SnapshotData& snapshot, const ofJson& json) {
	try {
		if(json.contains("name") && !json["name"].is_null()) {
			snapshot.name = json["name"].get<string>();
		} else {
			snapshot.name = "Snapshot";
		}
		
		if(json.contains("routerValues") && !json["routerValues"].is_null()) {
			snapshot.routerValues = jsonToRouterValues(json["routerValues"]);
		}
	} catch(const std::exception& e) {
		ofLogError("Snapshot") << "Error parsing snapshot: " << e.what();
		throw;
	}
}

void ofxOceanodeNodeMacro::clearSnapshot(int slot) {
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		snapshots.erase(it);
		if(currentSnapshotSlot == slot) {
			currentSnapshotSlot = -1;
		}
	}
}

void ofxOceanodeNodeMacro::renameSnapshot(int slot, const string& newName) {
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		it->second.name = newName;
	}
}

void ofxOceanodeNodeMacro::clearAllSnapshots() {
	snapshots.clear();
	currentSnapshotSlot = -1;
	saveSnapshots();
}

void ofxOceanodeNodeMacro::onMatrixSizeChanged(int& value) {
	matrixRows.set(ofClamp(matrixRows.get(), 1, 8));
	matrixCols.set(ofClamp(matrixCols.get(), 1, 8));
}

ofJson ofxOceanodeNodeMacro::routerValuesToJson(const std::map<string, RouterSnapshot>& values) {
	ofJson json;
	for(const auto& pair : values) {
		json[pair.first] = routerSnapshotToJson(pair.second);
	}
	return json;
}

std::map<string, RouterSnapshot> ofxOceanodeNodeMacro::jsonToRouterValues(const ofJson& json) {
	std::map<string, RouterSnapshot> values;
	for(auto it = json.begin(); it != json.end(); ++it) {
		values[it.key()] = jsonToRouterSnapshot(it.value());
	}
	return values;
}

ofJson ofxOceanodeNodeMacro::routerSnapshotToJson(const RouterSnapshot& snapshot) {
	ofJson json;
	json["type"] = snapshot.type;
	json["value"] = snapshot.value;
	return json;
}

RouterSnapshot ofxOceanodeNodeMacro::jsonToRouterSnapshot(const ofJson& json) {
	RouterSnapshot snapshot;
	snapshot.type = json["type"].get<string>();
	snapshot.value = json["value"];
	return snapshot;
}

void ofxOceanodeNodeMacro::storeRouterSnapshot(int slot) {
	updateRouterConnections();
	
	SnapshotData snapshotData;
	
	// Preserve existing name if the slot already exists
	auto existingSnapshot = snapshots.find(slot);
	if(existingSnapshot != snapshots.end()) {
		snapshotData.name = existingSnapshot->second.name;
	} else {
		snapshotData.name = ofToString(slot);
	}
	
	for(auto& routerPair : routerNodes) {
		auto& router = routerPair.second;
		if(!router.isInput) continue;

		auto& params = router.node->getParameters();
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
		if(!valueParam) continue;

		RouterSnapshot snapshot;
		snapshot.type = valueParam->valueType();
		
		try {
			if(snapshot.type == typeid(float).name()) {
				snapshot.value = valueParam->cast<float>().getParameter().get();
			}
			else if(snapshot.type == typeid(vector<float>).name()) {
				snapshot.value = valueParam->cast<vector<float>>().getParameter().get();
			}
			else if(snapshot.type == typeid(int).name()) {
				int val = valueParam->cast<int>().getParameter().get();
				snapshot.value = static_cast<float>(val);
			}
			else if(snapshot.type == typeid(vector<int>).name()) {
				auto vec = valueParam->cast<vector<int>>().getParameter().get();
				vector<float> floatVec;
				for(auto val : vec) {
					floatVec.push_back(static_cast<float>(val));
				}
				snapshot.value = floatVec;
			}
			else if(snapshot.type == typeid(bool).name()) {
				snapshot.value = valueParam->cast<bool>().getParameter().get();
			}
			else if(snapshot.type == typeid(vector<bool>).name()) {
				auto vec = valueParam->cast<vector<bool>>().getParameter().get();
				vector<bool> boolVec(vec.begin(), vec.end());
				snapshot.value = boolVec;
			}
			else if(snapshot.type == typeid(string).name()) {
				snapshot.value = valueParam->cast<string>().getParameter().get();
			}
			else if(snapshot.type == typeid(vector<string>).name()) {
				snapshot.value = valueParam->cast<vector<string>>().getParameter().get();
			}
			else if(snapshot.type == typeid(ofColor).name()) {
				auto color = valueParam->cast<ofColor>().getParameter().get();
				snapshot.value = {
					{"r", color.r},
					{"g", color.g},
					{"b", color.b},
					{"a", color.a}
				};
			}
			else if(snapshot.type == typeid(ofFloatColor).name()) {
				auto color = valueParam->cast<ofFloatColor>().getParameter().get();
				snapshot.value = {
					{"r", color.r},
					{"g", color.g},
					{"b", color.b},
					{"a", color.a}
				};
			}
			else if(snapshot.type == typeid(void).name()) {
				snapshot.value = nullptr;  // Just store the trigger state
			}
			
			snapshotData.routerValues[router.routerName] = snapshot;
		} catch(const std::exception& e) {
			ofLogError("Snapshot") << "Error storing value: " << e.what();
		}
	}
	snapshots[slot] = snapshotData;
	currentSnapshotSlot = slot;
	
	// Save snapshots to disk if not a local preset
	if(!localPreset && !currentMacroPath.empty()) {
		saveSnapshots();
	}
}
