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
	isLoadingPreset = false;
	
	// Initialize snapshot system members
	currentSnapshotSlot = -1;

	// Initialize inline-rename state
	routerSortEditingIndex   = -1;
	routerSortEditNeedsFocus = false;
	memset(routerSortEditBuf, 0, sizeof(routerSortEditBuf));
	matrixRows.set("Snapshot Matrix Rows", 2, 1, 8);
	matrixCols.set("Snapshot Matrix Cols", 8, 1, 8);
	showSnapshotNames.set("Show Names", true);
	buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f);
	showSnapshotMatrix = true;
	retriggerSnapshotOnActive.set("Retrigger Snapshot on Active", false);
	
	
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
		// Clear snapshots when changing presets to avoid data persistence
		snapshots.clear();
		currentSnapshotSlot = -1;
		
		localPreset = false;
		isLoadingPreset = true;
		if(clearContainerOnLoad) container->clearContainer();
		innerPresetLoadingPath = nextPresetPath;
		container->loadPreset(nextPresetPath);
		innerPresetLoadingPath = "";

		// Explicitly load snapshots from the global macro path
		loadSnapshotsFromPath(nextPresetPath);

		isLoadingPreset = false;
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
				if(retriggerSnapshotOnActive && currentSnapshotSlot >= 0) {
					// Retrigger the current snapshot to fire its parameters again
					loadRouterSnapshot(currentSnapshotSlot);
				}
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
		
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));  // Dark background
		ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.7f, 0.7f, 0.7f, 0.7f));  // White text
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // Hovered button
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
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
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		
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
			//ofLog() << "Save current preset and notify other macros";
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
	addInspectorParameter(retriggerSnapshotOnActive.set("Retrigger Snapshot on Active", false));
	
	
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
	allNodesCreatedListener = container->allNodesCreated.newListener(this, &ofxOceanodeNodeMacro::allNodesCreated);
	
	canvas.setContainer(container);
	canvas.setup("Macro " + ofToString(getNumIdentifier()), canvasParentID);
	
	// get Snap params from shared
	bool b = ofxOceanodeShared::getSnapToGrid();
	canvas.setSnapToGrid(b);
	int i = ofxOceanodeShared::getSnapGridDivs();
	canvas.setGridDivisions(ofxOceanodeShared::getSnapGridDivs());
	canvas.updateGridSize();
	
	if(additionalInfo != ""){
		localPreset = false;
		isLoadingPreset = true;
		innerPresetLoadingPath = additionalInfo;
		container->loadPreset(additionalInfo);
		innerPresetLoadingPath = "";
		isLoadingPreset = false;
		updateCurrentCategoryFromPath(additionalInfo);
		currentMacroPath = additionalInfo;
		
		// Load snapshots if they exist
		loadSnapshotsFromPath(additionalInfo);
	}
	
	snapshotUpdatedListener = ofxOceanodeShared::getSnapshotUpdatedEvent().newListener([this](string &path){
		
		if(!localPreset && path == currentMacroPath) {
			loadSnapshotsFromPath(currentMacroPath);
		}
	});
	
	macroUpdatedListener = ofxOceanodeShared::getMacroUpdatedEvent().newListener([this](string &s){
		//ofLog() << s;
		if(s == currentMacroPath && !localPreset){
			if(clearContainerOnLoad) container->clearContainer();
			container->loadPreset(currentMacroPath);
		}
	});
	
	addInspectorParameter(colorParam.set("Color", color));
	colorListener = colorParam.newListener([this](ofColor &c){
		color = c;
	});
	addInspectorParameter(localName.set("Local Name", "Local"));
	addInspectorParameter(resetPhaseOnActive.set("Reset Ph on Active", false));
	addInspectorParameter(clearContainerOnLoad.set("Clear Container on Load Preset", false));

	// Router sort order inspector region
	memset(routerSortSepNameBuf, 0, sizeof(routerSortSepNameBuf));
	addInspectorParameter(routerSortInspector.set("Router Order", [this]() {
		renderRouterSortInterface();
	}));
}

<<<<<<< Updated upstream
=======
void ofxOceanodeNodeMacro::processRouterNode(ofxOceanodeNode* node){
	shared_ptr<ofAbstractParameter> newCreatedParam;

	// Check for routerDropdown first — it doesn't go through the type registry
	auto* dropdownRouter = dynamic_cast<routerDropdown*>(&node->getNodeModel());
	if(dropdownRouter != nullptr){
		// Build a synced int parameter
		auto paramRef = dynamic_pointer_cast<ofParameter<int>>(dropdownRouter->getValue().newReference());
		auto param = make_shared<ofParameter<int>>();
		param->set(dropdownRouter->getNameParam().get(), paramRef->get(), 0,
		           max(0, (int)dropdownRouter->getCurrentOptions().size() - 1));

		deleteListeners.push(paramRef->newListener([paramRef, param](int &val){ param->set(paramRef->get()); }));
		deleteListeners.push(param->newListener([paramRef, param](int &val){ paramRef->set(param->get()); }));
		deleteListeners.push(dropdownRouter->getNameParam().newListener([param](string &s){
			param->setName(s);
		}));

		newCreatedParam = param;

		string paramName = newCreatedParam->getName();
		while(getParameterGroup().contains(paramName)) paramName = "_" + paramName;
		newCreatedParam->setName(paramName);

		auto addedAbstract = addParameter(*newCreatedParam.get());

		// Set initial dropdown options on the macro pin
		auto opts = dropdownRouter->getCurrentOptions();
		if(!opts.empty()){
			addedAbstract->cast<int>().setDropdownOptions(opts);
		}

		// Watch the options parameter on the router node for changes, keep macro pin in sync
		auto& routerParams = node->getParameters();
		if(routerParams.contains("Options")){
			auto& optionsAbstract = routerParams.get("Options");
			auto* optionsOceaParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&optionsAbstract);
			if(optionsOceaParam && optionsOceaParam->valueType() == typeid(vector<string>).name()){
				// Capture shared_ptr to keep addedAbstract alive in lambda
				dropdownRouterListeners[paramName + "_opts"] =
					optionsOceaParam->cast<vector<string>>().getParameter().newListener(
						[addedAbstract, param](vector<string> &newOpts){
							addedAbstract->cast<int>().setDropdownOptions(newOpts);
							param->setMax(max(0, (int)newOpts.size() - 1));
							if(param->get() >= (int)newOpts.size()) param->set(0);
						}
					);
			}
		}

		ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
		nameParamFromRouter = paramName;

		parameterGroupChanged.notify(this);
		deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter, paramName](){
			getParameterGroup().remove(nameParamFromRouter);
			dropdownRouterListeners.erase(paramName);
			dropdownRouterListeners.erase(paramName + "_opts");
		}, 0));

		updateRouterInfo(node);

		// Auto-register in sort order and set up rename/delete tracking.
		// shared_ptr<string> currentName is shared between the rename listener
		// and the delete listener so that a rename keeps both in sync.
		{
			string rName = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
			if(!isRouterInSortOrder(rName))
				routerSortOrder.push_back(rName);

			auto currentName = make_shared<string>(rName);
			auto& nameRef = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();

			deleteListeners.push(nameRef.newListener([this, currentName](string& newName) mutable {
				// Update sort order entry
				for(auto& e : routerSortOrder)
					if(!isSortSeparatorEntry(e) && e == *currentName){ e = newName; break; }
				// Keep routerNodes map in sync so the inspector "isUnknown" check
				// reflects the new name immediately after a runtime rename.
				auto nodeIt = routerNodes.find(*currentName);
				if(nodeIt != routerNodes.end()){
					RouterInfo info = nodeIt->second;
					info.routerName = newName;
					routerNodes.erase(nodeIt);
					routerNodes[newName] = info;
				}
				*currentName = newName;
			}));

			deleteListeners.push(node->deleteModule.newListener([this, currentName](){
				routerSortOrder.erase(
					remove(routerSortOrder.begin(), routerSortOrder.end(), *currentName),
					routerSortOrder.end());
			}));
		}
		return;
	}

	// Generic path for all other router types
	newCreatedParam = typesRegistry->createRouterFromType(node);
	string paramName = newCreatedParam->getName();
	while(getParameterGroup().contains(paramName)) paramName = "_" + paramName;
	newCreatedParam->setName(paramName);
	addParameter(*newCreatedParam.get());

	ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
	nameParamFromRouter = paramName;

	parameterGroupChanged.notify(this);
	deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter](){
		getParameterGroup().remove(nameParamFromRouter);
	}, 0));

	updateRouterInfo(node);

	// Auto-register in sort order and set up rename/delete tracking.
	// shared_ptr<string> currentName is shared between the rename listener
	// and the delete listener so that a rename keeps both in sync.
	{
		string rName = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
		if(!isRouterInSortOrder(rName))
			routerSortOrder.push_back(rName);

		auto currentName = make_shared<string>(rName);
		auto& nameRef = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();

		deleteListeners.push(nameRef.newListener([this, currentName](string& newName) mutable {
			// Update sort order entry
			for(auto& e : routerSortOrder)
				if(!isSortSeparatorEntry(e) && e == *currentName){ e = newName; break; }
			// Keep routerNodes map in sync so the inspector "isUnknown" check
			// reflects the new name immediately after a runtime rename.
			auto nodeIt = routerNodes.find(*currentName);
			if(nodeIt != routerNodes.end()){
				RouterInfo info = nodeIt->second;
				info.routerName = newName;
				routerNodes.erase(nodeIt);
				routerNodes[newName] = info;
			}
			*currentName = newName;
		}));

		deleteListeners.push(node->deleteModule.newListener([this, currentName](){
			routerSortOrder.erase(
				remove(routerSortOrder.begin(), routerSortOrder.end(), *currentName),
				routerSortOrder.end());
		}));
	}
}

>>>>>>> Stashed changes
void ofxOceanodeNodeMacro::newNodeCreated(ofxOceanodeNode* &node){
	string nodeName = node->getParameters().getName();
	if(ofSplitString(nodeName, " ")[0] == "Router"){
		if(isLoadingPreset){
			// Pre-load nameParam from the router's preset JSON so that
			// allNodesCreated() can build routerByName with the correct saved
			// names instead of the default type labels ("Float", "Int", …).
			// This mirrors what loadPresetBeforeConnections does (step 3), but
			// we need the name to be correct already at allNodesCreated() time
			// (step 2) so our sort-order lookup works.
			if(!innerPresetLoadingPath.empty()){
				string escapedName = node->getNodeModel().nodeName();
				ofStringReplace(escapedName, " ", "_");
				string filePath = innerPresetLoadingPath + "/" + escapedName
				    + "_" + ofToString(node->getNodeModel().getNumIdentifier()) + ".json";
				if(ofFile::doesFileExist(filePath)){
					ofJson routerJson = ofLoadJson(filePath);
					if(!routerJson.empty()){
						// Call abstractRouter::loadBeforeConnections which does
						// ofDeserialize(json, nameParam) — sets the name param
						// to the saved value before allNodesCreated() fires.
						static_cast<abstractRouter*>(&node->getNodeModel())
						    ->loadBeforeConnections(routerJson);
					}
				}
			}
			toCreateRouters.push_back(node);
			return;
		}
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
    node->setActive(active);
    if(active) node->update(args);
}

void ofxOceanodeNodeMacro::allNodesCreated(){
<<<<<<< Updated upstream
	//sort all nodes by y position
	std::sort(toCreateRouters.begin(), toCreateRouters.end(), [](ofxOceanodeNode* node1, ofxOceanodeNode* node2){
		return node1->getNodeGui().getPosition().y < node2->getNodeGui().getPosition().y;
	});
	
	for(auto node : toCreateRouters){
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
		
		ofEventArgs args;
		node->update(args);
=======
	// Nothing queued → no router nodes were (re)created in this load pass.
	// This happens when all routers are merely repositioned (not newly created),
	// i.e. the container already held them from a previous load in the same session.
	// Leave routerSortOrder unchanged and discard the pending buffer.
	if(toCreateRouters.empty()){
		pendingRouterSortOrder.clear();
		return;
>>>>>>> Stashed changes
	}

	// Remove any separator parameters left over from a previous load.
	// Router parameters clean themselves up via their deleteModule listeners;
	// separators have no such listener so we remove them manually here.
	{
		vector<string> toRemove;
		for(int i = 0; i < getParameterGroup().size(); i++){
			const string& pName = getParameterGroup().get(i).getName();
			if(pName.size() > 11 && pName.substr(0, 11) == "SEPARATOR:|")
				toRemove.push_back(getParameterGroup().get(i).getEscapedName());
		}
		for(const auto& n : toRemove) getParameterGroup().remove(n);
	}

	// Rebuild routerSortOrder from scratch.
	// The tracking blocks inside processRouterNode() will push each router
	// name as it is processed; separators are pushed inline below.
	routerSortOrder.clear();

	if(!pendingRouterSortOrder.empty()){
		// ── Sorted path ─────────────────────────────────────────────────────
		// Router nameParams have been pre-loaded by newNodeCreated(), so the
		// map keys are the correct saved names, not the default type labels.
		map<string, ofxOceanodeNode*> routerByName;
		for(auto* node : toCreateRouters){
			string name = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
			routerByName[name] = node;
		}

		set<ofxOceanodeNode*> processed;

		for(const auto& entry : pendingRouterSortOrder){
			if(isSortSeparatorEntry(entry)){
				// Inject separator into the parameter group AND into the active order.
				addSeparator(getSortSeparatorLabel(entry), getSortSeparatorColor(entry));
				routerSortOrder.push_back(entry);
			} else {
				auto it = routerByName.find(entry);
				if(it != routerByName.end()){
					processRouterNode(it->second);   // pushes entry to routerSortOrder
					ofEventArgs args;
					it->second->update(args);
					processed.insert(it->second);
				}
				// Entries not found (router was deleted) are silently dropped.
			}
		}

		// Routers not listed in the sort order: append in Y-position order
		// (backward compatibility — new routers added to the inner macro).
		vector<ofxOceanodeNode*> remaining;
		for(auto* node : toCreateRouters)
			if(processed.find(node) == processed.end()) remaining.push_back(node);
		sort(remaining.begin(), remaining.end(), [](ofxOceanodeNode* a, ofxOceanodeNode* b){
			return a->getNodeGui().getPosition().y < b->getNodeGui().getPosition().y;
		});
		for(auto* node : remaining){
			processRouterNode(node);
			ofEventArgs args;
			node->update(args);
		}
	} else {
		// ── Unsorted path (no saved order) ──────────────────────────────────
		// Original Y-position sort; routerSortOrder is rebuilt in Y-order.
		sort(toCreateRouters.begin(), toCreateRouters.end(), [](ofxOceanodeNode* a, ofxOceanodeNode* b){
			return a->getNodeGui().getPosition().y < b->getNodeGui().getPosition().y;
		});
		for(auto* node : toCreateRouters){
			processRouterNode(node);
			ofEventArgs args;
			node->update(args);
		}
	}

	pendingRouterSortOrder.clear();
	toCreateRouters.clear();
}

void ofxOceanodeNodeMacro::macroSave(ofJson &json, string path){
	if(localPreset){
		string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
		container->savePreset(localPath);
		json["LocalPreset"] = true;
		
		// Save snapshots to a separate file in the local macro folder
		if(!snapshots.empty()) {
			string snapshotsFilePath = localPath + "/snapshots.json";
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}
			ofSavePrettyJson(snapshotsFilePath, snapshotsJson);
			
			// Keep saving to JSON for backward compatibility
			ofJson jsonSnapshots;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				jsonSnapshots[ofToString(pair.first)] = slotJson;
			}
			json["Snapshots"] = jsonSnapshots;
		}
	}else{
		json["LocalPreset"] = false;
		json["CategoryStruct"] = currentCategory;
		json["Macro"] = currentMacro;
		json["MacroPath"] = currentMacroPath;
	}
	
	json["RetriggerSnapshotOnActive"] = retriggerSnapshotOnActive.get();

	// Save router sort order
	if(!routerSortOrder.empty()) {
		ofJson sortArr = ofJson::array();
		for(const auto& e : routerSortOrder) sortArr.push_back(e);
		json["RouterSortOrder"] = sortArr;
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
	if(json.count(clearContainerOnLoad.getEscapedName()) == 0){
		clearContainerOnLoad = false;
	}else{
		deserializeParameter(json, clearContainerOnLoad);
	}
	
	if(json.count("RetriggerSnapshotOnActive") == 0){
		retriggerSnapshotOnActive = false;
	}else{
		retriggerSnapshotOnActive = json["RetriggerSnapshotOnActive"].get<bool>();
	}
	
	// Load router sort order before container->loadPreset() so allNodesCreated() can use it
	loadRouterSortFromJson(json);

	if(clearContainerOnLoad) container->clearContainer();
	isLoadingPreset = true;

	// Clear existing snapshots to prevent old data persisting
	snapshots.clear();
	currentSnapshotSlot = -1;
	
	try {
		localPreset = json.value("LocalPreset", true);
		
		if(localPreset){
			string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
			innerPresetLoadingPath = localPath;
			container->loadPreset(localPath);
			innerPresetLoadingPath = "";
			
			// Store the local path for future snapshot saving
			presetPath = localPath;
			
			// First check for snapshots.json file in the local macro folder
			string snapshotsFilePath = localPath + "/snapshots.json";
			if(ofFile::doesFileExist(snapshotsFilePath)) {
				//ofLog() << "Loading snapshots from file: " << snapshotsFilePath;
				loadSnapshotsFromPath(localPath);
			}
			// If no separate file, check for embedded snapshots (backward compatibility)
			else if(json.contains("Snapshots") && !json["Snapshots"].is_null()) {
				//ofLog() << "Loading snapshots from embedded JSON";
				for(const auto& item : json["Snapshots"].items()) {
					int slot = ofToInt(item.key());
					SnapshotData snapshot;
					loadSnapshotFromJson(snapshot, item.value());
					snapshots[slot] = snapshot;
				}
			}
		}else{
			// Reset local path since we're now using a global macro
			presetPath = "";
			
			// TODO: Load preset from the
			auto currentCategoryVec = json["CategoryStruct"].get<deque<string>>();
			currentCategory = currentCategoryVec;
			currentMacro = json["Macro"];
			currentMacroPath = "";
			
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
					innerPresetLoadingPath = iter->second;
					container->loadPreset(iter->second);
					innerPresetLoadingPath = "";
					currentMacroPath = iter->second;
				}
			} else {
				innerPresetLoadingPath = currentMacroPath;
				container->loadPreset(currentMacroPath);
				innerPresetLoadingPath = "";
			}
			
			// Load snapshots if they exist
			if(!currentMacroPath.empty()) {
				loadSnapshotsFromPath(currentMacroPath);
			}
		}
	} catch (ofJson::exception) {
		ofLog() << "Cannot get local preset";
		localPreset = true;

		string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
		innerPresetLoadingPath = localPath;
		container->loadPreset(localPath);
		innerPresetLoadingPath = "";

		// Store the local path for future snapshot saving
		presetPath = localPath;
	}
	isLoadingPreset = false;
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
	
	// Clear snapshots when a new preset is about to be loaded
	// This ensures old snapshot data doesn't persist between presets
	snapshots.clear();
	currentSnapshotSlot = -1;
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
		abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
		if(absRouter == nullptr) continue;
		if(absRouter->isExcludeFromSnapshot()) continue;
		
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
			saveSnapshots(); // Save immediately when a snapshot name is changed
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
	ofLogNotice("SnapshotSync") << "saveSnapshots called, snapshot count: " << snapshots.size();
	
	// For global macros
	if(!localPreset && !currentMacroPath.empty()) {
		string filename = ofFilePath::removeTrailingSlash(currentMacroPath) + "/snapshots.json";
		ofLogNotice("SnapshotSync") << "Global macro snapshot file: " << filename;
		
		if(snapshots.empty()) {
			// Delete the snapshots file if no snapshots exist
			if(ofFile::doesFileExist(filename)) {
				ofFile::removeFile(filename);
				ofLogNotice("SnapshotSync") << "Deleted empty snapshots file: " << filename;
			} else {
				ofLogNotice("SnapshotSync") << "No snapshots file to delete";
			}
		} else {
			// Create JSON with snapshots and save
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}
			
			ofSavePrettyJson(filename, snapshotsJson);
			ofLogNotice("SnapshotSync") << "Saved " << snapshots.size() << " snapshots to: " << filename;
		}
		
		// Broadcast update regardless of whether we saved or deleted
		ofxOceanodeShared::snapshotUpdated(currentMacroPath);
	}
	// For local macros
	else if(localPreset) {
		string localPath;
		
		// Try to use the same path that was used to load the preset
		if(presetPath != "") {
			localPath = presetPath;
		}
		// If no existing path, construct it from the canvas ID
		else {
			if(canvasParentID.empty()) {
				localPath = "Presets/" + nodeName() + "_" + ofToString(getNumIdentifier());
			} else {
				localPath = "Presets/" + canvasParentID + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
			}
		}
		
		string snapshotsFilePath = localPath + "/snapshots.json";
		ofLogNotice("SnapshotSync") << "Local macro snapshot file: " << snapshotsFilePath;
		
		if(snapshots.empty()) {
			// Delete the snapshots file if no snapshots exist
			if(ofFile::doesFileExist(snapshotsFilePath)) {
				ofFile::removeFile(snapshotsFilePath);
				ofLogNotice("SnapshotSync") << "Deleted empty local snapshots file: " << snapshotsFilePath;
			} else {
				ofLogNotice("SnapshotSync") << "No local snapshots file to delete";
			}
		} else {
			// Create JSON with snapshots and save
			ofJson snapshotsJson;
			for(const auto& pair : snapshots) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["routerValues"] = routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}
			
			ofSavePrettyJson(snapshotsFilePath, snapshotsJson);
			ofLogNotice("SnapshotSync") << "Saved " << snapshots.size() << " local snapshots to: " << snapshotsFilePath;
		}
	}
}

string ofxOceanodeNodeMacro::calculateSnapshotHash() {
	if(snapshots.empty()) return "";
	
	string hashInput;
	for(const auto& pair : snapshots) {
		hashInput += ofToString(pair.first) + pair.second.name;
		// Add some representation of the router values
		for(const auto& routerPair : pair.second.routerValues) {
			hashInput += routerPair.first + routerPair.second.type;
		}
	}
	
	// Simple hash - you could use a proper hash function here
	return ofToString(std::hash<string>{}(hashInput));
}

void ofxOceanodeNodeMacro::loadSnapshotsFromPath(const string& path) {
	// Check if the path already ends with snapshots.json
	string snapshotsFile;
	if(ofFilePath::getFileName(path) == "snapshots.json") {
		snapshotsFile = path;
	} else {
		snapshotsFile = ofFilePath::removeTrailingSlash(path) + "/snapshots.json";
	}
	
	ofLogNotice("SnapshotSync") << "Loading snapshots from: " << snapshotsFile;
	
	if(ofFile::doesFileExist(snapshotsFile)) {
		//ofLog() << "Found snapshots file, loading...";
		try {
			ofJson snapshotsJson = ofLoadJson(snapshotsFile);
			snapshots.clear();
			for(const auto& item : snapshotsJson.items()) {
				int slot = ofToInt(item.key());
				SnapshotData snapshot;
				loadSnapshotFromJson(snapshot, item.value());
				snapshots[slot] = snapshot;
				//ofLog() << "Loaded snapshot for slot " << slot << " with name " << snapshot.name;
			}
			//ofLog() << "Loaded " << snapshots.size() << " snapshots from " << snapshotsFile;
			ofLogNotice("SnapshotSync") << "Loaded " << snapshots.size() << " snapshots";
			
			lastSnapshotHash = calculateSnapshotHash();
			
		} catch(const std::exception& e) {
			ofLogError("Macro") << "Error loading snapshots: " << e.what();
		}
	} else {
		//ofLogWarning("Macro") << "Snapshots file does not exist: " << snapshotsFile;
		ofLogWarning("SnapshotSync") << "Snapshots file does not exist: " << snapshotsFile;
		
	}
}

void ofxOceanodeNodeMacro::syncSnapshotsFromDisk() {
	if(!localPreset && !currentMacroPath.empty()) {
		ofLogNotice("SnapshotSync") << "Manual sync requested for: " << currentMacroPath;
		
		loadSnapshotsFromPath(currentMacroPath);
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
		ofLogNotice("SnapshotSync") << "Clearing snapshot slot: " << slot;
		snapshots.erase(it);
		if(currentSnapshotSlot == slot) {
			currentSnapshotSlot = -1;
		}
		
		// Save immediately when a snapshot is cleared
		saveSnapshots();
		
		ofLogNotice("SnapshotSync") << "Snapshot " << slot << " cleared and saved";
	}
}

void ofxOceanodeNodeMacro::renameSnapshot(int slot, const string& newName) {
	auto it = snapshots.find(slot);
	if(it != snapshots.end()) {
		it->second.name = newName;
		saveSnapshots(); // Save immediately when a snapshot is renamed
	}
}

void ofxOceanodeNodeMacro::clearAllSnapshots() {
	ofLogNotice("SnapshotSync") << "Clearing all snapshots";
	snapshots.clear();
	currentSnapshotSlot = -1;
	
	// Important: Call saveSnapshots to delete the file
	saveSnapshots();
	
	ofLogNotice("SnapshotSync") << "All snapshots cleared and saved";
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
		abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
		if(absRouter == nullptr) continue;
		if(absRouter->isExcludeFromSnapshot()) continue;
		
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

	// Save snapshots to disk immediately for both local and global presets
	saveSnapshots();
}

// ─────────────────────────────────────────────────────────────────────────────
// Router Sort Order
// ─────────────────────────────────────────────────────────────────────────────

bool ofxOceanodeNodeMacro::isSortSeparatorEntry(const string& e) const {
	return e.size() > 7 && e.substr(0, 7) == "__SEP__";
}

string ofxOceanodeNodeMacro::getSortSeparatorLabel(const string& e) const {
	// format: "__SEP__:label"  or  "__SEP__:label:r,g,b,a"
	size_t first = e.find(':');
	if(first == string::npos) return "";
	size_t second = e.find(':', first + 1);
	return (second == string::npos)
		? e.substr(first + 1)
		: e.substr(first + 1, second - first - 1);
}

ofColor ofxOceanodeNodeMacro::getSortSeparatorColor(const string& e) const {
	size_t first = e.find(':');
	if(first == string::npos) return ofColor(200, 200, 200, 255);
	size_t second = e.find(':', first + 1);
	if(second == string::npos) return ofColor(200, 200, 200, 255);
	auto parts = ofSplitString(e.substr(second + 1), ",");
	if(parts.size() < 4) return ofColor(200, 200, 200, 255);
	return ofColor(ofToInt(parts[0]), ofToInt(parts[1]), ofToInt(parts[2]), ofToInt(parts[3]));
}

string ofxOceanodeNodeMacro::makeSortSeparatorEntry(const string& label, const ofColor& c) const {
	return "__SEP__:" + label + ":" +
		ofToString((int)c.r) + "," + ofToString((int)c.g) + "," +
		ofToString((int)c.b) + "," + ofToString((int)c.a);
}

bool ofxOceanodeNodeMacro::isRouterInSortOrder(const string& name) const {
	for(const auto& e : routerSortOrder)
		if(!isSortSeparatorEntry(e) && e == name) return true;
	return false;
}

void ofxOceanodeNodeMacro::loadRouterSortFromJson(const ofJson& json) {
	// Load into the PENDING buffer, not into the active routerSortOrder.
	// The active routerSortOrder is cleared and rebuilt by allNodesCreated() once
	// all router nodes have been created and their nameParams pre-loaded.
	pendingRouterSortOrder.clear();
	if(json.contains("RouterSortOrder") && json["RouterSortOrder"].is_array()){
		for(const auto& item : json["RouterSortOrder"])
			pendingRouterSortOrder.push_back(item.get<string>());
	}
}

void ofxOceanodeNodeMacro::renderRouterSortInterface() {
#ifndef OFXOCEANODE_HEADLESS
	ImGui::TextDisabled("Drag to reorder, double-click to rename.");
	ImGui::TextDisabled("Changes apply on next preset load.");
	ImGui::Spacing();

	int moveFrom = -1, moveTo = -1;
	// Disable drag-and-drop while a rename is in progress so the InputText
	// doesn't accidentally get cancelled by an unintended drag gesture.
	bool anyEditing = (routerSortEditingIndex >= 0);

	for(int i = 0; i < (int)routerSortOrder.size(); i++){
		ImGui::PushID(i);
		bool isSep    = isSortSeparatorEntry(routerSortOrder[i]);
		bool isEditing = (!isSep && routerSortEditingIndex == i);

		string label = isSep
			? ("\xe2\x94\x80\xe2\x94\x80  " + getSortSeparatorLabel(routerSortOrder[i]) + "  \xe2\x94\x80\xe2\x94\x80")
			: routerSortOrder[i];

		bool isUnknown = !isSep && (routerNodes.find(routerSortOrder[i]) == routerNodes.end());

		if(isEditing){
			// ── Inline rename InputText ──────────────────────────────────────
			if(routerSortEditNeedsFocus){
				ImGui::SetKeyboardFocusHere();
				routerSortEditNeedsFocus = false;
			}
			ImGui::SetNextItemWidth(-1);
			bool confirmed = ImGui::InputText("##rename", routerSortEditBuf, sizeof(routerSortEditBuf),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

			if(confirmed){
				// Apply the rename to the router's nameParam.
				// The processRouterNode rename listener takes care of updating
				// routerSortOrder, routerNodes, and the macro parameter name.
				string newName(routerSortEditBuf);
				if(!newName.empty() && newName != routerSortOrder[i]){
					auto it = routerNodes.find(routerSortOrder[i]);
					if(it != routerNodes.end()){
						static_cast<abstractRouter*>(&it->second.node->getNodeModel())
							->getNameParam().set(newName);
					}
				}
				routerSortEditingIndex = -1;
			} else if(ImGui::IsItemDeactivated()){
				// Lost focus (Escape or click elsewhere) → cancel silently
				routerSortEditingIndex = -1;
			}
		} else {
			// ── Normal display ───────────────────────────────────────────────
			if(isUnknown) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			if(isSep)     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));

			ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0, 0));

			if(isUnknown || isSep) ImGui::PopStyleColor();

			// Double-click enters rename mode (known router entries only)
			if(!isSep && !isUnknown
				&& ImGui::IsItemHovered()
				&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
				routerSortEditingIndex   = i;
				routerSortEditNeedsFocus = true;
				strncpy(routerSortEditBuf, routerSortOrder[i].c_str(), sizeof(routerSortEditBuf) - 1);
				routerSortEditBuf[sizeof(routerSortEditBuf) - 1] = '\0';
			}

			// Drag-drop (disabled while any rename is in progress)
			if(!anyEditing){
				if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
					ImGui::SetDragDropPayload("ROUTER_SORT_ITEM", &i, sizeof(int));
					ImGui::Text("%s", label.c_str());
					ImGui::EndDragDropSource();
				}
				if(ImGui::BeginDragDropTarget()){
					if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROUTER_SORT_ITEM")){
						moveFrom = *(const int*)payload->Data;
						moveTo   = i;
					}
					ImGui::EndDragDropTarget();
				}
			}

			// Delete button for separators
			if(isSep){
				ImGui::SameLine();
				if(ImGui::SmallButton("x")){
					routerSortOrder.erase(routerSortOrder.begin() + i);
					ImGui::PopID();
					if(moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo){
						string item = routerSortOrder[moveFrom];
						routerSortOrder.erase(routerSortOrder.begin() + moveFrom);
						int insertAt = (moveFrom < moveTo) ? moveTo - 1 : moveTo;
						routerSortOrder.insert(routerSortOrder.begin() + insertAt, item);
					}
					return;
				}
			}
		}

		ImGui::PopID();
	}

	// Apply deferred drag-drop move
	if(moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo){
		string item = routerSortOrder[moveFrom];
		routerSortOrder.erase(routerSortOrder.begin() + moveFrom);
		int insertAt = (moveFrom < moveTo) ? moveTo - 1 : moveTo;
		routerSortOrder.insert(routerSortOrder.begin() + insertAt, item);
	}

	// "Add separator" row
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::SetNextItemWidth(120);
	ImGui::InputText("##sep_name", routerSortSepNameBuf, sizeof(routerSortSepNameBuf));
	ImGui::SameLine();
	if(ImGui::SmallButton("+ Sep")){
		if(routerSortSepNameBuf[0] != '\0'){
			routerSortOrder.push_back(makeSortSeparatorEntry(
				string(routerSortSepNameBuf), ofColor(200, 200, 200, 255)));
			routerSortSepNameBuf[0] = '\0';
		}
	}
#endif
}
