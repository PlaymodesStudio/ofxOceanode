//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModel("Macro"){
    color = ofColor::black;
    presetPath = "";
    currentPreset = -1;
	showWindow = false;
	localPreset = true;
    lastActiveState = true;
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
		
		
		
		if(ImGui::Checkbox("Local Macro", &localPreset)){
			if(currentMacro == ""){
				localPreset = true;
			}
			if(!localPreset){
				container->loadPreset(currentMacroPath);
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("Show Window", &showWindow);
		
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
        
		ImGui::SameLine();
		
		bool firstSaveAsOpen = false;
        if (ImGui::Button("Save")){
			ofLog() << "Save current preset and notify other macros";
			// If we try to save a new macro or a local preset, the save macro as window apears
			if(currentMacroPath == "" || localPreset){
				ImGui::OpenPopup("Save Macro As :");
				firstSaveAsOpen = true;
			}else{
				container->savePreset(currentMacroPath);
				ofxOceanodeShared::macroUpdated(currentMacroPath);
			}
		}
        
        ImGui::SameLine();
        
		if (ImGui::Button("Save As")){
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
		
		
//		ImGui::PopStyleColor(1);
		ImGui::Spacing();
		ImGui::Spacing();
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
    
    container = make_shared<ofxOceanodeContainer>(registry, typesRegistry);
    newNodeListener = container->newNodeCreated.newListener(this, &ofxOceanodeNodeMacro::newNodeCreated);
    
    canvas.setContainer(container);
    canvas.setup("Macro " + ofToString(getNumIdentifier()), canvasParentID);
	
	if(additionalInfo != ""){
		localPreset = false;
		container->loadPreset(additionalInfo);
		updateCurrentCategoryFromPath(additionalInfo);
		currentMacroPath = additionalInfo;
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
    }
    ofEventArgs args;
    node->update(args);
}

void ofxOceanodeNodeMacro::macroSave(ofJson &json, string path){
	if(localPreset){
		container->savePreset(path + "/" + nodeName() + "_" + ofToString(getNumIdentifier()));
		json["LocalPreset"] = true;
	}else{
		json["LocalPreset"] = false;
		json["CategoryStruct"] = currentCategory;
		json["Macro"] = currentMacro;
	}
}

void ofxOceanodeNodeMacro::macroLoad(ofJson &json, string path){
	try {
		localPreset = json["LocalPreset"];
	} catch (ofJson::exception) {
		ofLog() << "Cannot get local preset";
		localPreset = true;
	}
	if(localPreset){
		container->loadPreset(path + "/" + nodeName() + "_" + ofToString(getNumIdentifier()));
	}else{
		// TODO: Load preset from the
		auto currentCategoryVec = json["CategoryStruct"].get<deque<string>>();
		currentCategory = currentCategoryVec;
		currentMacro = json["Macro"];
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

void ofxOceanodeNodeMacro::presetWillBeLoaded(){
//    container->loadPreset_presetWillBeLoaded();
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
