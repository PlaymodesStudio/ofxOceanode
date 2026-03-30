//
//  ofxOceanodeNodeMacroGui.cpp
//  example-basic
//
//  GUI rendering methods for ofxOceanodeNodeMacro.
//  Split from ofxOceanodeNodeMacro.cpp in Phase 6 refactoring.
//
//  All ImGui rendering code lives here; the main .cpp has zero ImGui calls
//  (except the minimal draw() pass-through).
//

#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"

// ─────────────────────────────────────────────────────────────────────────────
// renderMinimizedView — extracted from the minimizedViewCallback lambda
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderMinimizedView(ImVec2 size) {
	// Only show output router values in minimized view
	for(auto& routerPair : routerManager.getRouterNodes()) {
		if(!routerPair.second.isInput) {
			auto& router = routerPair.second;
			auto& params = router.node->getParameters();
			if(!params.contains("Value")) continue; // skip void routers
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
}

// ─────────────────────────────────────────────────────────────────────────────
// renderPresetControlGui — extracted from the presetControl lambda in setup()
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderPresetControlGui() {
	
	bool addBank = false;
	
	if(presetManager.isLocal()){
		ImGui::Text(localName->c_str());
	}else{
		ImGui::Text("%s", presetManager.getCurrentMacro().c_str());
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
						if(presetManager.getCurrentCategory().size() == 0){
								presetManager.getCurrentCategoryMacro() = d;
							}
							presetManager.getCurrentCategory().push_front(d->name);
						ImGui::EndMenu();
						return true;
					}
					ImGui::EndMenu();
				}
			}
			if(category->categories.size() != 0){
					if(ImGui::MenuItem("Add Bank")){
						addBank = true;
					}
				}
			for(auto m : category->macros){
				if(ImGui::MenuItem(m.first.c_str())){
					presetManager.setNextPresetPath(m.second);
					presetManager.setCurrentMacroPath(m.second);
					presetManager.setCurrentMacro(m.first);
					presetManager.getCurrentCategory().clear();
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
			// TODO: Implement bank creation
			ImGui::CloseCurrentPopup();
		}
		if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
			strcpy(cString, "");
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	if(ImGui::Checkbox("Is Local?", &presetManager.getLocalRef())){
		if(presetManager.getCurrentMacro() == ""){
			presetManager.setLocal(true);
		}
		if(!presetManager.isLocal()){
			container->loadPreset(presetManager.getCurrentMacroPath());
		}
	}
	ImGui::SameLine();
	bool prevShowWindow = showWindow;
	ImGui::Checkbox("Show Window?", &showWindow);
	if(showWindow && !prevShowWindow){
		canvas.requestFocus();
	}
	
	bool firstSaveAsOpen = false;
	if (ImGui::Button("[Save]")){
		// If we try to save a new macro or a local preset, the save macro as window appears
		if(presetManager.getCurrentMacroPath() == "" || presetManager.isLocal()){
			ImGui::OpenPopup("Save Macro As :");
			firstSaveAsOpen = true;
		}else{
			container->savePreset(presetManager.getCurrentMacroPath());
				snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
			// Save router sort order to the macro folder
			{
				string sortOrderFile = presetManager.getCurrentMacroPath() + "/router_sort_order.json";
				ofJson sortJson;
				if(!sortOrder.getOrder().empty()) {
					ofJson sortArr = ofJson::array();
					for(const auto& e : sortOrder.getOrder()) sortArr.push_back(e);
					sortJson["RouterSortOrder"] = sortArr;
				}
				ofSavePrettyJson(sortOrderFile, sortJson);
			}
			ofxOceanodeShared::macroUpdated(presetManager.getCurrentMacroPath());
		}
	}
	
	ImGui::SameLine();
	
	if (ImGui::Button("[Save As]")){
		ImGui::OpenPopup("Save Macro As :");
		firstSaveAsOpen = true;
	}
	
	if(ImGui::BeginPopupModal("Save Macro As :", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
		static char cString[256];
		
		if(firstSaveAsOpen){
			presetManager.getSaveAsTempCategory() = presetManager.getCurrentCategory();
			ImGui::SetKeyboardFocusHere(0);
		}
		
		bool openNameAlreadyExistsPopup = false;
		if (ImGui::InputText("##Preset Name : ", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			string proposedNewName(cString);
			ofStringReplace(proposedNewName, " ", "_");
			
			
			bool nameExists = false;
			auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
			for(int i = 0 ; i < presetManager.getSaveAsTempCategory().size(); i++){
				string categoryNameToCompare = presetManager.getSaveAsTempCategory()[i];
				macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
														[categoryNameToCompare](shared_ptr<macroCategory> &mc){return mc->name == categoryNameToCompare;});
			}
			
			if(!nameExists)
			{
				if(strcmp(proposedNewName.c_str(), "") != 0){
					string saveAsCategoryWithSlash = ofToDataPath("Macros/", true);
					for(auto s : presetManager.getSaveAsTempCategory()) saveAsCategoryWithSlash = saveAsCategoryWithSlash + s + "/";
					container->savePreset(saveAsCategoryWithSlash + string(proposedNewName));
					presetManager.setLocal(false);
					presetManager.setCurrentMacro(string(proposedNewName));
					presetManager.setCurrentMacroPath(saveAsCategoryWithSlash + string(proposedNewName));
					presetManager.getCurrentCategory() = presetManager.getSaveAsTempCategory();
					presetManager.setCurrentCategoryMacro(macroDirectoryStructure);
					// Save router sort order to the macro folder
					{
						string sortOrderFile = presetManager.getCurrentMacroPath() + "/router_sort_order.json";
						ofJson sortJson;
						if(!sortOrder.getOrder().empty()) {
							ofJson sortArr = ofJson::array();
							for(const auto& e : sortOrder.getOrder()) sortArr.push_back(e);
							sortJson["RouterSortOrder"] = sortArr;
						}
						ofSavePrettyJson(sortOrderFile, sortJson);
					}
					snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
					ofxOceanodeShared::updateMacrosStructure();
				}
				strcpy(cString, "");
				ImGui::CloseCurrentPopup();
				presetManager.getSaveAsTempCategory().clear();
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
		for(auto s : presetManager.getSaveAsTempCategory()) saveAsCategoryWithSlash = saveAsCategoryWithSlash + s + "/";
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
							presetManager.getSaveAsTempCategory().clear();
								presetManager.getSaveAsTempCategory().push_front(d->name);
							return true;
						}
					}else if(ImGui::BeginMenu(d->name.c_str())){
						if(drawCategory(d)){
							presetManager.getSaveAsTempCategory().push_front(d->name);
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
			presetManager.getSaveAsTempCategory().clear();
		}
		ImGui::EndPopup();
	}
	
	
	ImGui::Text(". . . . . . . . . . . . . . . . .");
	
}

// ─────────────────────────────────────────────────────────────────────────────
// renderPresetNamingGui — extracted from the presetNaming lambda in setup()
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderPresetNamingGui() {

	// Render snapshot matrix
	if(showSnapshotMatrix) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
		renderSnapshotMatrix();
		ImGui::PopStyleVar();

		// Morph time controls
		float nodeW = (float)(ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget());
		ImGui::SetNextItemWidth(nodeW);
		ImGui::SliderFloat("##morphMs", &morphEngine.getMorphMs(), 0.f, 10000.f, "Morph: %.0f ms");
		ImGui::SetNextItemWidth(nodeW);
		ImGui::SliderFloat("##morphBiPow", &morphEngine.getMorphBiPow(), -1.f, 1.f, "BiPow: %.2f");

		// Morphing progress bar (bipow-mapped)
		float progress = morphEngine.isMorphing() ? morphEngine.getProgress() : (snapshotSystem.getCurrentSlot() >= 0 ? 1.f : 0.f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
		ImGui::ProgressBar(progress, ImVec2(nodeW, 6.f), "");
		ImGui::PopStyleColor();

		ImGui::Text(". . . . . . . . . . . . . . . . .");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// renderSnapshotMatrix — snapshot button grid (moved from main .cpp)
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderSnapshotMatrix() {
	ImGui::PushID("Matrix");
	
	const int numRows = matrixRows;
	const int numCols = matrixCols;
	
	for(int i = 0; i < numRows; i++) {
		for(int j = 0; j < numCols; j++) {
			if(j > 0) ImGui::SameLine();
			
			int slot = i * numCols + j;
			ImGui::PushID(slot);
			
			bool hasData = snapshotSystem.hasSnapshot(slot);
			bool isActive = (slot == snapshotSystem.getCurrentSlot());
			
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
			
			string label = (hasData && showSnapshotNames) ? snapshotSystem.getSnapshots().at(slot).name : ofToString(slot);
			
			if(ImGui::Button(label.c_str(), ImVec2(buttonSize, buttonSize/1.5))) {
				if(ImGui::GetIO().KeyShift) {
					storeRouterSnapshot(slot);
				} else {
					snapshotSystem.setCurrentSlot(slot);
					loadRouterSnapshot(slot);
				}
			}
			
			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}
	}
	
	ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
// renderInspectorInterface — snapshot name/load/clear list (moved from main .cpp)
// ─────────────────────────────────────────────────────────────────────────────

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
			snapshotSystem.clearAll();
			snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Cancel", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	if(snapshotSystem.isEmpty()) {
		ImGui::Text("No snapshots stored");
		return;
	}
	
	for(auto it = snapshotSystem.getSnapshots().begin(); it != snapshotSystem.getSnapshots().end();) {
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
			snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
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
			if(snapshotSystem.getCurrentSlot() == it->first) {
				snapshotSystem.setCurrentSlot(-1);
			}
			it = snapshotSystem.getSnapshots().erase(it);
			snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
		} else {
			++it;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// syncParameterGroupToSortOrder — reorders parameter group to match sort order
// (moved from main .cpp)
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::syncParameterGroupToSortOrder() {
	// Build the full desired parameter order: non-managed params stay in their
	// original positions; router/separator slots are replaced by the new order.
	// Uses ofParameterGroup::reorder() which sorts in-place without removing or
	// re-creating any parameter objects, so connections are never broken.

	// Collect current names.
	vector<string> originalNames;
	for(int i = 0; i < (int)getParameterGroup().size(); i++)
		originalNames.push_back(getParameterGroup().get(i).getName());

	auto isManaged = [&](const string& name) -> bool {
		if(name.size() > 11 && name.substr(0, 11) == "SEPARATOR:|") return true;
		return isRouterInSortOrder(name);
	};

	// Build the new order for managed slots from sortOrder.getOrder().
	vector<string> managedNew;
	for(const auto& entry : sortOrder.getOrder()) {
		string paramName = entry;
		if(isSortSeparatorEntry(entry)) {
			ofColor c = getSortSeparatorColor(entry);
			paramName = "SEPARATOR:|" + getSortSeparatorLabel(entry) + "|" +
				ofToString((int)c.r) + "," + ofToString((int)c.g) + "," +
				ofToString((int)c.b) + "," + ofToString((int)c.a);
		}
		if(getParameterGroup().contains(paramName)) managedNew.push_back(paramName);
	}

	// Merge: non-managed params keep original positions; managed slots get new order.
	int mi = 0;
	vector<string> newOrder;
	for(const auto& name : originalNames) {
		if(isManaged(name)) {
			if(mi < (int)managedNew.size()) newOrder.push_back(managedNew[mi++]);
		} else {
			newOrder.push_back(name);
		}
	}
	while(mi < (int)managedNew.size()) newOrder.push_back(managedNew[mi++]);

	// Reorder in-place — no parameters are removed or re-created.
	getParameterGroup().reorder(newOrder);

	parameterGroupChanged.notify(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// renderRouterSortInterface — router sort order inspector with drag-drop,
// separators, rename (moved from main .cpp)
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderRouterSortInterface() {
#ifndef OFXOCEANODE_HEADLESS
	ImGui::SeparatorText("Router Management");
	ImGui::TextDisabled("Drag to reorder, double-click to rename.");
	ImGui::Spacing();

	int moveFrom = -1, moveTo = -1;
	// Disable drag-and-drop while a rename is in progress so the InputText
	// doesn't accidentally get cancelled by an unintended drag gesture.
	bool anyEditing = (guiState.routerSortEditingIndex >= 0);

	// Track whether we need to early-return because a separator was deleted
	// (erasing from sortOrder.getOrder() mid-loop requires us to stop iterating).
	bool separatorDeleted = false;

	// Deferred type-change: set from the context menu, executed after PopID so
	// the ImGui ID stack is clean before we rebuild sortOrder.getOrder().
	string typeChangeFrom, typeChangeTo;

	for(int i = 0; i < (int)sortOrder.getOrder().size(); i++){
		ImGui::PushID(i);
		bool isSep     = isSortSeparatorEntry(sortOrder.getOrder()[i]);
		bool isEditing = (guiState.routerSortEditingIndex == i); // both routers and separators can edit

		// Display label: separators get decorative dashes; routers show their name.
		string displayLabel = isSep
			? ("--  " + getSortSeparatorLabel(sortOrder.getOrder()[i]) + "  --")
			: sortOrder.getOrder()[i];

		// A router is "unknown" if it is not (yet) registered in routerManager,
		// e.g. an entry left over from a deleted router.  Separators are never unknown.
		bool isUnknown = !isSep && (routerManager.getRouterNodes().find(sortOrder.getOrder()[i]) == routerManager.getRouterNodes().end());

		if(isEditing){
			// ── Inline rename InputText (routers and separators) ─────────────
			if(guiState.routerSortEditNeedsFocus){
				ImGui::SetKeyboardFocusHere();
				guiState.routerSortEditNeedsFocus = false;
			}
			ImGui::SetNextItemWidth(isSep ? -40.0f : -1.0f); // leave room for x button on seps
			bool confirmed = ImGui::InputText("##rename", guiState.routerSortEditBuf, sizeof(guiState.routerSortEditBuf),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

			if(confirmed){
				string newName(guiState.routerSortEditBuf);
				if(isSep){
					// Separator rename: update the parameter name in the group and
					// replace the sort-order entry with the new label.
					if(!newName.empty()){
						string oldLabel  = getSortSeparatorLabel(sortOrder.getOrder()[i]);
						ofColor sepColor = getSortSeparatorColor(sortOrder.getOrder()[i]);
						if(newName != oldLabel){
							string oldParamName = "SEPARATOR:|" + oldLabel + "|" +
								ofToString((int)sepColor.r) + "," + ofToString((int)sepColor.g) + "," +
								ofToString((int)sepColor.b) + "," + ofToString((int)sepColor.a);
							string newParamName = "SEPARATOR:|" + newName + "|" +
								ofToString((int)sepColor.r) + "," + ofToString((int)sepColor.g) + "," +
								ofToString((int)sepColor.b) + "," + ofToString((int)sepColor.a);
							// Update the parameter name in-place so the visual divider
							// reflects the new label immediately (no need to reload).
							for(int j = 0; j < getParameterGroup().size(); j++){
								if(getParameterGroup().get(j).getName() == oldParamName){
									getParameterGroup().get(j).setName(newParamName);
									break;
								}
							}
							sortOrder.getOrder()[i] = makeSortSeparatorEntry(newName, sepColor);
						}
					}
				} else {
					// Router rename: set the abstractRouter's nameParam; the existing
					// rename listener cascades the update to sortOrder.getOrder(), routerManager,
					// and the macro's published parameter name.
					if(!newName.empty() && newName != sortOrder.getOrder()[i]){
						auto it = routerManager.getRouterNodes().find(sortOrder.getOrder()[i]);
						if(it != routerManager.getRouterNodes().end()){
							static_cast<abstractRouter*>(&it->second.node->getNodeModel())
								->getNameParam().set(newName);
						}
					}
				}
				guiState.routerSortEditingIndex = -1;
			} else if(ImGui::IsItemDeactivated()){
				// Lost focus (Escape or click elsewhere) → cancel silently
				guiState.routerSortEditingIndex = -1;
			}

			// Delete button shown inline even while editing a separator
			if(isSep){
				ImGui::SameLine();
				if(ImGui::SmallButton("x")){
					string sepLabel = getSortSeparatorLabel(sortOrder.getOrder()[i]);
					removeSeparator(sepLabel);
					sortOrder.getOrder().erase(sortOrder.getOrder().begin() + i);
					guiState.routerSortEditingIndex = -1;
					separatorDeleted = true;
					ImGui::PopID();
					break; // stop iterating — sortOrder.getOrder() just changed size
				}
			}
		} else {
			// ── Normal display ───────────────────────────────────────────────
			if(isUnknown) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			if(isSep)     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));

			// For separators, constrain the selectable to leave 40 px for the
			// delete ("x") button (same margin as the InputText in edit mode).
			// ImVec2(0,0) would fill the entire available width, pushing the
			// SameLine button outside the clip rect and making it unclickable.
			float selWidth = isSep
				? max(0.0f, ImGui::GetContentRegionAvail().x - 40.0f)
				: 0.0f;
			ImGui::Selectable(displayLabel.c_str(), false, ImGuiSelectableFlags_None, ImVec2(selWidth, 0));

			if(isUnknown || isSep) ImGui::PopStyleColor();

			// Double-click enters rename mode for any known entry (routers AND separators).
			if(!isUnknown
				&& ImGui::IsItemHovered()
				&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
				guiState.routerSortEditingIndex   = i;
				guiState.routerSortEditNeedsFocus = true;
				// Pre-fill with just the label (not the full __SEP__:... prefix for seps).
				string currentLabel = isSep ? getSortSeparatorLabel(sortOrder.getOrder()[i]) : sortOrder.getOrder()[i];
				strncpy(guiState.routerSortEditBuf, currentLabel.c_str(), sizeof(guiState.routerSortEditBuf) - 1);
				guiState.routerSortEditBuf[sizeof(guiState.routerSortEditBuf) - 1] = '\0';
			}

			// ── Right-click context menu (only for known router rows) ─────────
			if(!isSep && !isUnknown && ImGui::BeginPopupContextItem("##routerCtx")){
				// ── Range editing ────────────────────────────────────────────
				auto& innerParams = routerManager.getRouterNodes()[sortOrder.getOrder()[i]].node->getParameters();
				if(innerParams.contains("Min") && innerParams.contains("Max")){
					auto* minP = dynamic_cast<ofxOceanodeAbstractParameter*>(&innerParams.get("Min"));
					auto* maxP = dynamic_cast<ofxOceanodeAbstractParameter*>(&innerParams.get("Max"));
					if(minP && maxP){
						ImGui::SeparatorText("Range");
						bool rangeChanged = false;
						string vt = minP->valueType();
						if(vt == typeid(float).name()){
							float mn = minP->cast<float>().getParameter().get();
							float mx = maxP->cast<float>().getParameter().get();
							ImGui::SetNextItemWidth(120.f);
							rangeChanged |= ImGui::DragFloat("Min##mnf", &mn, 0.01f);
							ImGui::SetNextItemWidth(120.f);
							rangeChanged |= ImGui::DragFloat("Max##mxf", &mx, 0.01f);
							if(rangeChanged){
								minP->cast<float>().getParameter().set(mn);
								maxP->cast<float>().getParameter().set(mx);
							}
						} else if(vt == typeid(int).name()){
							int mn = minP->cast<int>().getParameter().get();
							int mx = maxP->cast<int>().getParameter().get();
							ImGui::SetNextItemWidth(120.f);
							rangeChanged |= ImGui::DragInt("Min##mni", &mn);
							ImGui::SetNextItemWidth(120.f);
							rangeChanged |= ImGui::DragInt("Max##mxi", &mx);
							if(rangeChanged){
								minP->cast<int>().getParameter().set(mn);
								maxP->cast<int>().getParameter().set(mx);
							}
						} else {
							ImGui::TextDisabled("No range for this type");
						}
					}
					ImGui::Separator();
				}

				// ── Snapshot exclusion ───────────────────────────────────────
				{
					auto* absRouter = static_cast<abstractRouter*>(
						&routerManager.getRouterNodes()[sortOrder.getOrder()[i]].node->getNodeModel());
					bool excl = absRouter->isExcludeFromSnapshot();
					if(ImGui::Checkbox("Exclude from Snapshots##excl", &excl))
						absRouter->getSnapshotExcludeParam().set(excl);
				}
				ImGui::Separator();

				// ── Change Type submenu ──────────────────────────────────────
				string curNodeType = routerManager.getRouterNodes()[sortOrder.getOrder()[i]].node->getNodeModel().nodeName();
				if(ImGui::BeginMenu("Change Type")){
					for(auto& mPair : registry->getRegisteredModels()){
						const string& mName = mPair.first;
						// Show all "Router X" entries; highlight the current type
						// with a checkmark and gray it out (non-clickable).
						if(mName.size() > 7 && mName.substr(0, 7) == "Router "){
							bool isCurrent = (mName == curNodeType);
							if(ImGui::MenuItem(mName.c_str() + 7, nullptr, isCurrent, !isCurrent)){
								typeChangeFrom = sortOrder.getOrder()[i];
								typeChangeTo   = mName;
								ImGui::CloseCurrentPopup();
							}
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}

			// Drag-drop (disabled while any rename is in progress)
			if(!anyEditing){
				if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
					ImGui::SetDragDropPayload("ROUTER_SORT_ITEM", &i, sizeof(int));
					ImGui::Text("%s", displayLabel.c_str());
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

			// Delete button for separators (shown in normal display mode too)
			if(isSep){
				ImGui::SameLine();
				if(ImGui::SmallButton("x")){
					string sepLabel = getSortSeparatorLabel(sortOrder.getOrder()[i]);
					removeSeparator(sepLabel);
					sortOrder.getOrder().erase(sortOrder.getOrder().begin() + i);
					separatorDeleted = true;
					ImGui::PopID();
					break; // stop iterating — sortOrder.getOrder() just changed size
				}
			}
		}

		ImGui::PopID();

		// Execute a type change requested from the context menu this iteration.
		// Placed after PopID so the ImGui ID stack is clean before we mutate
		// sortOrder.getOrder(); we return immediately and let ImGui re-render next frame.
		if(!typeChangeFrom.empty()){
			changeRouterType(typeChangeFrom, typeChangeTo);
			return;
		}
	}

	// Apply deferred drag-drop move (only if no separator was deleted this frame).
	if(!separatorDeleted && moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo){
		string item = sortOrder.getOrder()[moveFrom];
		sortOrder.getOrder().erase(sortOrder.getOrder().begin() + moveFrom);
		int insertAt = (moveFrom < moveTo) ? moveTo - 1 : moveTo;
		sortOrder.getOrder().insert(sortOrder.getOrder().begin() + insertAt, item);

		syncParameterGroupToSortOrder();
	}

	// "Add separator" row
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::SetNextItemWidth(120);
	ImGui::InputText("##sep_name", guiState.routerSortSepNameBuf, sizeof(guiState.routerSortSepNameBuf));
	ImGui::SameLine();
	if(ImGui::SmallButton("+ Sep")){
		if(guiState.routerSortSepNameBuf[0] != '\0'){
			string sepLabel(guiState.routerSortSepNameBuf);
			ofColor sepColor(200, 200, 200, 255);
			addSeparator(sepLabel, sepColor);
			sortOrder.getOrder().push_back(makeSortSeparatorEntry(sepLabel, sepColor));
			guiState.routerSortSepNameBuf[0] = '\0';
			parameterGroupChanged.notify(this);
		}
	}
#endif
}
