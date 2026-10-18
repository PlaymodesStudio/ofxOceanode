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
#include "ofxOceanodeColors.h"
#include "MacroDialogAppearance.h"
#include <cstdio>
#include <filesystem>

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

bool ofxOceanodeNodeMacro::drawNodeParameter(ofxOceanodeAbstractParameter& parameter,
                                             float labelWidth, float widgetWidth) {
	if(parameter.getName() != active.getName()) return false;
	ImGui::BeginGroup();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Active");
	ImGui::SameLine(labelWidth, 0.f);
	bool enabled = active.get();
	if(ImGui::Checkbox("##Active", &enabled)) active = enabled;
	const char* scopeLabel = presetManager.isLocal() ? "Local" : "Global";
	const float scopeWidth = ImGui::CalcTextSize(scopeLabel).x + 2.f * ImGui::GetStyle().FramePadding.x;
	// SameLine offsets already include the current group's canvas origin.
	ImGui::SameLine(labelWidth + widgetWidth - scopeWidth, 0.f);
	ImVec4 background = ImGui::GetStyleColorVec4(ImGuiCol_Button);
	background.x = std::min(1.f, background.x + 0.18f);
	background.y = std::min(1.f, background.y + 0.18f);
	background.z = std::min(1.f, background.z + 0.18f);
	ImGui::PushStyleColor(ImGuiCol_Button, background);
	if(ImGui::Button(scopeLabel, ImVec2(scopeWidth, 0))){
		if(presetManager.isLocal()){
			if(presetManager.getCurrentMacroPath().empty()) openMacroSaveDialog(true);
			else {
				presetManager.setNextPresetPath(presetManager.getCurrentMacroPath());
				pendingMacroReferenceUserEdited = true;
			}
		}else{
			pendingGuiAction = [this](){
				const string path = getLocalWorkingDirectory() + "/Detached";
				if(!saveLibraryDefinition(path)){
					guiState.saveError = "Could not make a local copy.";
					return;
				}
				localName = presetManager.getCurrentMacro();
				presetManager.setLocal(true);
				presetManager.setPresetPath(path);
				loadMacroLayout(path);
				markMacroReferenceUserEdited();
			};
		}
	}
	ImGui::PopStyleColor();
	if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", presetManager.isLocal()
		? "Use the library version, or Save As to create one."
		: "Make a local copy of this Macro.");
	ImGui::EndGroup();
	return true;
}

void ofxOceanodeNodeMacro::openMacroSaveDialog(bool toLibrary) {
	guiState.saveToLibrary = toLibrary;
	guiState.openSavePopup = true;
	guiState.closeSavePopup = false;
	guiState.confirmOverwrite = false;
	guiState.creatingSaveFolder = false;
	guiState.revealSaveFolder = true;
	guiState.newFolderName[0] = '\0';
	guiState.saveError.clear();
	const string name = presetManager.isLocal() ? localName.get() : presetManager.getCurrentMacro();
	std::snprintf(guiState.saveName, sizeof(guiState.saveName), "%s", name.c_str());
	const string description = macroDescription.get();
	guiState.saveDescription.assign(std::max(size_t(4096), description.size() + 1024), '\0');
	std::copy(description.begin(), description.end(), guiState.saveDescription.begin());
	presetManager.getSaveAsTempCategory() = presetManager.getCurrentCategory();
}

void ofxOceanodeNodeMacro::renderPresetControlGui() {
	ImGui::BeginGroup();
	const string name = presetManager.isLocal() ? localName.get() : presetManager.getCurrentMacro();
	const float zoom = ofxOceanodeShared::getZoomLevel();
	const float width = (ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget()) * zoom;
	// Draw inside a fixed-size item: long names must not resize the node.
	const ImVec2 namePosition = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));
	const ImVec4 nameClip(namePosition.x, namePosition.y, namePosition.x + width,
		namePosition.y + ImGui::GetTextLineHeight());
	ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), namePosition,
		ImGui::GetColorU32(ImGuiCol_Text), name.empty() ? "-" : name.c_str(), nullptr, 0.f, &nameClip);
	if(ImGui::IsItemHovered() && ImGui::CalcTextSize(name.c_str()).x > width)
		ImGui::SetTooltip("%s", name.c_str());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f * zoom, ImGui::GetStyle().ItemSpacing.y));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.f * zoom, ImGui::GetStyle().FramePadding.y));
	const bool previousShow = showWindow;
	ImGui::Checkbox("Show?", &showWindow);
	if(showWindow && !previousShow){ canvas.forceDockWindow(); canvas.requestFocus(); }
	const float showWidth = ImGui::GetItemRectSize().x;
	const float gap = ImGui::GetStyle().ItemSpacing.x;
	const float loadText = ImGui::CalcTextSize("Load").x;
	const float saveText = ImGui::CalcTextSize("Save").x;
	const float saveAsText = ImGui::CalcTextSize("Save As").x;
	const float padding = std::max(0.f, (width - showWidth - 3.f * gap - loadText - saveText - saveAsText) / 3.f);
	ImGui::SameLine();
	if(ImGui::Button("Load", ImVec2(loadText + padding, 0))){
		guiState.openLoadPopup = true;
		guiState.loadSelectionPath.clear();
		guiState.loadSelectionName.clear();
		guiState.saveError.clear();
	}
	ImGui::SameLine();
	if(ImGui::Button("Save", ImVec2(saveText + padding, 0))){
		guiState.saveError.clear();
		if(presetManager.isLocal() && localName.get().empty()) openMacroSaveDialog(false);
		else pendingGuiAction = [this](){
			if(!(presetManager.isLocal() ? saveLocalDefinition() : saveGlobalDefinition()))
				guiState.saveError = "Could not save Macro contents.";
		};
	}
	ImGui::SameLine();
	if(ImGui::Button("Save As", ImVec2(saveAsText + padding, 0))) openMacroSaveDialog(true);
	ImGui::PopStyleVar(2);

	renderMacroLoadDialog();
	renderMacroSaveDialog();
	if(!guiState.saveError.empty() && !ImGui::IsPopupOpen("Save Macro")){
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextUnformatted(guiState.saveError.c_str());
		ImGui::PopTextWrapPos();
	}
	ImGui::EndGroup();
}

void ofxOceanodeNodeMacro::renderMacroLoadDialog() {
	if(guiState.openLoadPopup){
		ofxOceanodeShared::updateMacrosStructure();
		guiState.hoverMacroPath.clear();
		ImGui::OpenPopup("Load Macro Contents");
		guiState.openLoadPopup = false;
	}
	if(!ImGui::IsPopupOpen("Load Macro Contents")) return;
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSize(ImVec2(std::min(480.f, display.x * .85f), std::min(520.f, display.y * .85f)), ImGuiCond_Appearing);
	MacroDialogAppearance::Style appearance;
	if(!ImGui::BeginPopupModal("Load Macro Contents", nullptr, ImGuiWindowFlags_NoResize)) return;

	bool activateSelection = false;
	ImGui::BeginChild("Sources", ImVec2(0, -2.f * ImGui::GetFrameHeightWithSpacing()), false);
	{
		MacroDialogAppearance::Rows rows;
		auto selectSource = [&](const string& path, const string& name, bool isPreset){
			const bool selected = guiState.loadSelectionPath == path && guiState.loadSelectionIsPreset == isPreset;
			rows.begin(selected, isPreset);
			if(ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowDoubleClick)){
				guiState.loadSelectionPath = path;
				guiState.loadSelectionName = name;
				guiState.loadSelectionIsPreset = isPreset;
				activateSelection = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
			}
			rows.end();
			if(!isPreset && ImGui::IsItemHovered()){
				if(guiState.hoverMacroPath != path){
					guiState.hoverMacroPath = path;
					guiState.hoverMacroDescription.clear();
					if(ofFile::doesFileExist(path + "/description.json")){
						const auto json = ofLoadJson(path + "/description.json");
						if(json.is_object() && json.contains("Description") && json["Description"].is_string())
							guiState.hoverMacroDescription = json["Description"].get<string>();
					}
				}
				if(!guiState.hoverMacroDescription.empty()){
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.f);
					ImGui::TextUnformatted(guiState.hoverMacroDescription.c_str());
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			}
		};
		rows.begin();
		const bool presetsOpen = ImGui::TreeNodeEx("Preset", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
		rows.end();
		if(presetsOpen){
			std::function<void(const string&, bool)> drawPresets = [&](const string& folder, bool banks){
				ofDirectory directory(folder);
				directory.listDir();
				directory.sort();
				auto entries = directory.getFiles();
				if(!banks){
					std::stable_sort(entries.begin(), entries.end(), [](const ofFile& a, const ofFile& b){
						const auto aName = a.getFileName(), bName = b.getFileName();
						const int aIndex = aName.find("--") == string::npos ? INT_MAX : ofToInt(aName);
						const int bIndex = bName.find("--") == string::npos ? INT_MAX : ofToInt(bName);
						return std::make_pair(aIndex, aName) < std::make_pair(bIndex, bName);
					});
				}
				bool any = false;
				for(const auto& entry : entries){
					if(!entry.isDirectory()) continue;
					any = true;
					ImGui::PushID(entry.path().c_str());
					const string filename = entry.getFileName();
					if(ofFile::doesFileExist(entry.path() + "/modules.json")){
						const auto separator = filename.find("--");
						const string label = separator == string::npos ? filename
							: ofToString(ofToInt(filename)) + " | " + filename.substr(separator + 2);
						selectSource(entry.path(), label, true);
					}else{
						if(banks) ImGui::SetNextItemOpen(filename == ofxOceanodeShared::getCurrentBankName(), ImGuiCond_Once);
						rows.begin();
						const bool open = ImGui::TreeNodeEx(filename.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
						rows.end();
						if(open){
							drawPresets(entry.path(), false);
							ImGui::TreePop();
						}
					}
					ImGui::PopID();
				}
				if(!any){ rows.begin(false, true); ImGui::TextDisabled("No presets"); rows.end(); }
			};
			drawPresets(ofToDataPath("Presets", true), true);
			ImGui::TreePop();
		}
		rows.begin();
		const bool macrosOpen = ImGui::TreeNodeEx("Macro", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
		rows.end();
		if(macrosOpen){
			std::function<void(shared_ptr<macroCategory>)> drawMacros = [&](shared_ptr<macroCategory> category){
				if(!category) return;
				for(const auto& child : category->categories){
					rows.begin();
					const bool open = ImGui::TreeNodeEx(child->name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
					rows.end();
					if(open){
						drawMacros(child);
						ImGui::TreePop();
					}
				}
				for(const auto& macro : category->macros){
					ImGui::PushID(macro.second.c_str());
					selectSource(macro.second, macro.first, false);
					ImGui::PopID();
				}
				if(category->categories.empty() && category->macros.empty()){
					rows.begin(false, true); ImGui::TextDisabled("No Macros"); rows.end();
				}
			};
			drawMacros(ofxOceanodeShared::getMacroDirectoryStructure());
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();
	ImGui::Separator();
	if(guiState.loadSelectionName.empty()) ImGui::TextDisabled("Select a Preset or Macro");
	else ImGui::TextColored(OceanodeColors::SelectedNodeText, "%s", guiState.loadSelectionName.c_str());
	ImGui::BeginDisabled(guiState.loadSelectionPath.empty());
	const bool load = ImGui::Button("Load");
	ImGui::EndDisabled();
	if(!guiState.loadSelectionPath.empty() && (load || activateSelection)){
		const string path = guiState.loadSelectionPath;
		if(guiState.loadSelectionIsPreset){
			pendingGuiAction = [this, path](){
				if(!importPresetContents(path)) guiState.saveError = "Could not import preset.";
			};
		}else{
			presetManager.setNextPresetPath(path);
			pendingMacroReferenceUserEdited = true;
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

bool ofxOceanodeNodeMacro::createMacroLibraryFolder(const string& requestedName) {
	const string name = ofTrim(requestedName);
	guiState.saveError.clear();
	if(name.empty() || name == "." || name == ".." || name.back() == '.' ||
	   name.find_first_of("/\\:*?\"<>|") != string::npos ||
	   std::any_of(name.begin(), name.end(), [](unsigned char c){ return c < 32; })){
		guiState.saveError = "Enter a valid folder name.";
		return false;
	}
	string parent = ofToDataPath("Macros", true);
	for(const auto& folder : presetManager.getSaveAsTempCategory()){
		parent += "/" + folder;
		if(ofFile::doesFileExist(parent + "/modules.json")){
			guiState.saveError = "Select a library folder, rather than a Macro definition.";
			return false;
		}
	}
	std::error_code error;
	if(!std::filesystem::create_directory(std::filesystem::path(parent) / name, error)){
		guiState.saveError = error ? "Could not create folder: " + error.message()
		                          : "A file or folder with this name already exists.";
		return false;
	}
	presetManager.getSaveAsTempCategory().push_back(name);
	guiState.creatingSaveFolder = false;
	guiState.revealSaveFolder = true;
	guiState.confirmOverwrite = false;
	ofxOceanodeShared::updateMacrosStructure();
	return true;
}

void ofxOceanodeNodeMacro::renderMacroSaveLibraryTree() {
	ImGui::BeginChild("Save Library", ImVec2(380, 220), false);
	{
		MacroDialogAppearance::Rows rows;
		std::function<void(shared_ptr<macroCategory>, std::deque<string>)> drawFolder =
			[&](shared_ptr<macroCategory> category, std::deque<string> folders){
				if(!category) return;
				const auto& selected = presetManager.getSaveAsTempCategory();
				const bool isSelected = folders == selected;
				const bool isAncestor = folders.size() <= selected.size() &&
					std::equal(folders.begin(), folders.end(), selected.begin());
				if(guiState.revealSaveFolder && isAncestor) ImGui::SetNextItemOpen(true);
				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
					ImGuiTreeNodeFlags_SpanAvailWidth;
				rows.begin(isSelected);
				const bool open = ImGui::TreeNodeEx(folders.empty() ? "Macros (root)" : category->name.c_str(), flags);
				rows.end();
				if(ImGui::IsItemClicked() || ImGui::IsItemActivated()){
					presetManager.getSaveAsTempCategory() = folders;
					guiState.confirmOverwrite = false;
					guiState.saveError.clear();
				}
				if(isSelected && guiState.revealSaveFolder) ImGui::SetScrollHereY(.5f);
				if(!open) return;
				for(const auto& child : category->categories){
					auto nested = folders;
					nested.push_back(child->name);
					drawFolder(child, nested);
				}
				for(const auto& macro : category->macros){
					ImGui::PushID(macro.second.c_str());
					const bool target = isSelected && macro.first == guiState.saveName;
					rows.begin(target);
					if(ImGui::Selectable(macro.first.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups)){
						presetManager.getSaveAsTempCategory() = folders;
						std::snprintf(guiState.saveName, sizeof(guiState.saveName), "%s", macro.first.c_str());
						guiState.confirmOverwrite = false;
						guiState.saveError.clear();
					}
					rows.end();
					ImGui::PopID();
				}
				ImGui::TreePop();
			};
		drawFolder(ofxOceanodeShared::getMacroDirectoryStructure(), {});
		guiState.revealSaveFolder = false;
	}
	ImGui::EndChild();

	string location = "Macros";
	for(const auto& folder : presetManager.getSaveAsTempCategory()) location += "/" + folder;
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 380);
	ImGui::TextColored(OceanodeColors::SelectedNodeText, "Folder: %s", location.c_str());
	ImGui::PopTextWrapPos();
	if(!guiState.creatingSaveFolder){
		if(ImGui::Button("New Folder")){
			guiState.creatingSaveFolder = true;
			guiState.newFolderName[0] = '\0';
			guiState.saveError.clear();
			ImGui::SetKeyboardFocusHere();
		}
	}
	if(guiState.creatingSaveFolder){
		ImGui::SetNextItemWidth(380);
		const bool enter = ImGui::InputText("##New Folder Name", guiState.newFolderName,
			sizeof(guiState.newFolderName), ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Create Folder") || enter) createMacroLibraryFolder(guiState.newFolderName);
		ImGui::SameLine();
		if(ImGui::Button("Cancel Folder")){
			guiState.creatingSaveFolder = false;
			guiState.saveError.clear();
		}
	}
}

void ofxOceanodeNodeMacro::renderMacroSaveDialog() {
	if(guiState.openSavePopup){
		ofxOceanodeShared::updateMacrosStructure();
		ImGui::OpenPopup("Save Macro");
		guiState.openSavePopup = false;
	}
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
	MacroDialogAppearance::Style appearance;
	if(!ImGui::BeginPopupModal("Save Macro", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
	if(guiState.closeSavePopup){
		guiState.closeSavePopup = false;
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	ImGui::TextDisabled("%s", guiState.saveToLibrary ? "Save to Macro library" : "Save local Macro");
	ImGui::Separator();
	{
		MacroDialogAppearance::Rows rows;
		rows.begin();
		ImGui::BeginGroup();
		ImGui::TextUnformatted("Name");
		ImGui::SetNextItemWidth(380);
		if(ImGui::InputText("##Name", guiState.saveName, sizeof(guiState.saveName))) guiState.confirmOverwrite = false;
		ImGui::EndGroup();
		rows.end();
		rows.begin();
		ImGui::BeginGroup();
		ImGui::TextUnformatted("Description");
		ImGui::InputTextMultiline("##Description", guiState.saveDescription.data(), guiState.saveDescription.size(), ImVec2(380, ImGui::GetTextLineHeight() * 5));
		ImGui::EndGroup();
		rows.end();
	}
	if(guiState.saveToLibrary) renderMacroSaveLibraryTree();
	ImGui::Separator();

	if(!guiState.saveError.empty()) ImGui::TextWrapped("%s", guiState.saveError.c_str());
	if(guiState.confirmOverwrite) ImGui::TextWrapped("This Macro already exists. Overwrite its library contents?");
	ImGui::BeginDisabled(bool(pendingGuiAction) || guiState.creatingSaveFolder);
	if(ImGui::Button(guiState.confirmOverwrite ? "Overwrite" : "Save")){
		string name = ofTrim(string(guiState.saveName));
		if(guiState.saveToLibrary) ofStringReplace(name, " ", "_");
		guiState.saveError.clear();
		if(name.empty()) guiState.saveError = "Enter a name.";
		else if(guiState.saveToLibrary && (name == "." || name == ".." ||
			name.find_first_of("/\\:*?\"<>|") != string::npos || name.back() == '.' ||
			std::any_of(name.begin(), name.end(), [](unsigned char c){ return c < 32; })))
			guiState.saveError = "Enter a valid folder name.";
		else {
			string path = ofToDataPath("Macros", true);
			for(const auto& folder : presetManager.getSaveAsTempCategory()) path += "/" + folder;
			path += "/" + name;
			const bool exists = guiState.saveToLibrary && ofDirectory::doesDirectoryExist(path);
			if(exists && !ofFile::doesFileExist(path + "/modules.json"))
				guiState.saveError = "This name belongs to a library folder. Choose another name.";
			else if(exists && !guiState.confirmOverwrite) guiState.confirmOverwrite = true;
			else {
				const string description(guiState.saveDescription.data());
				const bool toLibrary = guiState.saveToLibrary;
				pendingGuiAction = [this, name, description, path, toLibrary](){
					const string oldDescription = macroDescription.get();
					const string oldName = localName.get();
					macroDescription = description;
					if(!toLibrary) localName = name;
					if(!(toLibrary ? saveLibraryDefinition(path) : saveLocalDefinition())){
						macroDescription = oldDescription;
						localName = oldName;
						guiState.saveError = "Could not save Macro contents.";
						return;
					}
					if(toLibrary){
						presetManager.setLocal(false);
						presetManager.setCurrentMacroPath(path);
						ofxOceanodeShared::updateMacrosStructure();
						presetManager.updateCategoryFromPath(path);
						loadMacroLayout(path);
						markMacroReferenceUserEdited();
						ofxOceanodeShared::snapshotUpdated(path);
						ofxOceanodeShared::macroUpdated(path);
					}
					guiState.closeSavePopup = true;
				};
			}
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(bool(pendingGuiAction));
	if(ImGui::Button("Cancel")){
		guiState.creatingSaveFolder = false;
		guiState.saveError.clear();
		guiState.confirmOverwrite = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::EndPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
// renderPresetNamingGui — extracted from the presetNaming lambda in setup()
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderPresetNamingGui() {

	// Render snapshot matrix
	if(showSnapshotMatrix) {
		// Scale inter-button spacing with zoom so gaps don't dominate at high zoom.
		// Use the continuous zoomLevel published by the canvas rather than deriving
		// a stepped factor from the discrete pre-cached font size.
		const float zoomFactor = ofxOceanodeShared::getZoomLevel();
		// Stable node width derived from configured dimensions × zoom factor.
		// Do NOT use GetContentRegionAvail().x here — it causes an infinite expansion
		// feedback loop when the snapshot matrix is visible.
		const float scaledTotalW = (float)(ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget()) * zoomFactor;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
		renderSnapshotMatrix();
		ImGui::PopStyleVar();

		// Morph time controls — use the stable node width (not GetContentRegionAvail).
		ImGui::SetNextItemWidth(scaledTotalW);
		ImGui::SliderFloat("##morphMs", &morphEngine.getMorphMs(), 0.f, 10000.f, "Morph: %.0f ms");
		ImGui::SetNextItemWidth(scaledTotalW);
		ImGui::SliderFloat("##morphBiPow", &morphEngine.getMorphBiPow(), -1.f, 1.f, "BiPow: %.2f");

		// Morphing progress bar — use explicit stable width (not -FLT_MIN sentinel).
		float progress = morphEngine.isMorphing() ? morphEngine.getProgress() : (snapshotSystem.getCurrentSlot() >= 0 ? 1.f : 0.f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, OceanodeColors::MorphProgressBar);
		ImGui::ProgressBar(progress, ImVec2(scaledTotalW, 6.0f * zoomFactor), "");
		ImGui::PopStyleColor();

//		ImGui::Text(". . . . . . . . . . . . . . . . .");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// renderSnapshotMatrix — snapshot button grid (moved from main .cpp)
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::renderSnapshotMatrix() {
	ImGui::PushID("Matrix");

	const int numRows = matrixRows;
	const int numCols = matrixCols;

	// Compute per-button width from the configured node dimensions (zoom-scaled) so
	// the result is a stable fixed value that does not feed back into the ImGui
	// layout loop.  GetContentRegionAvail().x must NOT be used here — calling it
	// inside the parameter callback that has no explicit width constraint makes the
	// node window expand each frame, which in turn grows GetContentRegionAvail(),
	// which grows the buttons, producing unbounded width growth.
	const float zoomFactor_sm = ofxOceanodeShared::getZoomLevel();
	const float scaledTotalW_sm = (float)(ofxOceanodeShared::getNodeWidthText() + ofxOceanodeShared::getNodeWidthWidget()) * zoomFactor_sm;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float btnW    = (scaledTotalW_sm - spacing * (numCols - 1)) / numCols;
	// Height: standard ImGui item height (same as a slider/text input).
	// GetFrameHeight() is already zoom-correct — no zoomFactor multiplication needed.
	const float btnH    = ImGui::GetFrameHeight();

	for(int i = 0; i < numRows; i++) {
		for(int j = 0; j < numCols; j++) {
			if(j > 0) ImGui::SameLine();
			
			int slot = i * numCols + j;
			ImGui::PushID(slot);
			
			bool hasData = snapshotSystem.hasSnapshot(slot);
			bool isActive = (slot == snapshotSystem.getCurrentSlot());
			
			// Set button colors based on state
			if(isActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, OceanodeColors::SnapshotActive);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OceanodeColors::SnapshotActiveHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, OceanodeColors::SnapshotActivePressed);
			}
			else if(hasData) {
				ImGui::PushStyleColor(ImGuiCol_Button, OceanodeColors::SnapshotFilled);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OceanodeColors::SnapshotFilledHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, OceanodeColors::SnapshotFilledPressed);
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}
			
			// Text is always visible here — we already early-returned when !renderWidgets_sm.
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
			
			string label = (hasData && showSnapshotNames) ? snapshotSystem.getSnapshots().at(slot).name : ofToString(slot);
			
			if(ImGui::Button(label.c_str(), ImVec2(btnW, btnH))) {
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
	if(snapshotSystem.isEmpty()) {
		ImGui::Text("No snapshots stored");
		return;
	}
	
	if(guiState.showClearAllSnapshotsPopup) {
		ImGui::OpenPopup("Clear All Snapshots?");
		guiState.showClearAllSnapshotsPopup = false;
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

	bool snapshotsChanged = false;
	for(auto it = snapshotSystem.getSnapshots().begin(); it != snapshotSystem.getSnapshots().end();) {
		ImGui::PushID(it->first);
		
		bool shouldDelete = false;
		char nameBuf[256];
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", it->second.name.c_str());
		
		// Slot number
		ImGui::Text("Slot %d", it->first);
		
		// Name input
		ImGui::SetNextItemWidth(150);
		if(ImGui::InputText("##name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
			it->second.name = nameBuf;
			snapshotsChanged = true;
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
			snapshotsChanged = true;
		} else {
			++it;
		}
	}
	// A global save synchronously broadcasts an update that reloads this map.
	// Wait until the iterator is no longer in use before saving.
	if(snapshotsChanged) {
		snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
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
	struct UserEditGuard {
		const std::vector<string>& order;
		std::vector<string> before;
		bool& edited;
		~UserEditGuard(){ if(order != before) edited = true; }
	} userEditGuard{sortOrder.getOrder(), sortOrder.getOrder(), routerOrderUserEdited};
	const float zoomLevel = ofxOceanodeShared::getZoomLevel();
//	ImGui::SeparatorText("Router Management");
	ImGui::TextDisabled("Drag to reorder, double-click to rename.");
	ImGui::Spacing();

	const float sepDeleteMargin = 40.0f * zoomLevel;
	
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
			ImGui::SetNextItemWidth(isSep ? -sepDeleteMargin : -1.0f); // leave room for x button on seps
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
			if(isSep)     ImGui::PushStyleColor(ImGuiCol_Text, OceanodeColors::SelectedNodeText);

			// For separators, constrain the selectable to leave 40 px for the
			// delete ("x") button (same margin as the InputText in edit mode).
			// ImVec2(0,0) would fill the entire available width, pushing the
			// SameLine button outside the clip rect and making it unclickable.
			float selWidth = isSep
				? max(0.0f, ImGui::GetContentRegionAvail().x - sepDeleteMargin)
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
							ImGui::SetNextItemWidth(120.0f * zoomLevel);
							rangeChanged |= ImGui::DragFloat("Min##mnf", &mn, 0.01f);
							ImGui::SetNextItemWidth(120.0f * zoomLevel);
							rangeChanged |= ImGui::DragFloat("Max##mxf", &mx, 0.01f);
							if(rangeChanged){
								minP->cast<float>().getParameter().set(mn);
								maxP->cast<float>().getParameter().set(mx);
							}
						} else if(vt == typeid(int).name()){
							int mn = minP->cast<int>().getParameter().get();
							int mx = maxP->cast<int>().getParameter().get();
							ImGui::SetNextItemWidth(120.0f * zoomLevel);
							rangeChanged |= ImGui::DragInt("Min##mni", &mn);
							ImGui::SetNextItemWidth(120.0f * zoomLevel);
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
	ImGui::SetNextItemWidth(120.0f * zoomLevel);
	ImGui::InputText("##sep_name", guiState.routerSortSepNameBuf, sizeof(guiState.routerSortSepNameBuf));
	ImGui::SameLine();
	ImGui::ColorEdit4("##sep_color", guiState.routerSortSepColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
	ImGui::SameLine();
	if(ImGui::SmallButton("[+ Separator]")){
		if(guiState.routerSortSepNameBuf[0] != '\0'){
			string sepLabel(guiState.routerSortSepNameBuf);
			ofColor sepColor(
				guiState.routerSortSepColor[0] * 255.0f,
				guiState.routerSortSepColor[1] * 255.0f,
				guiState.routerSortSepColor[2] * 255.0f,
				guiState.routerSortSepColor[3] * 255.0f
			);
			addSeparator(sepLabel, sepColor);
			sortOrder.getOrder().push_back(makeSortSeparatorEntry(sepLabel, sepColor));
			guiState.routerSortSepNameBuf[0] = '\0';
			parameterGroupChanged.notify(this);
		}
	}

}
