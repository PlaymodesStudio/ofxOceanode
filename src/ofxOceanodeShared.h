//
//  ofxOceanodeShared.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 17/04/2020.
//

#ifndef ofxOceanodeShared_h
#define ofxOceanodeShared_h

#include "portal.h"
#include <unordered_map>

class ofxOceanodeNode;

typedef int ofxOceanodeConfigurationFlags;

enum ofxOceanodeConfigurationFlags_
{
    ofxOceanodeConfigurationFlags_None              = 0,
    ofxOceanodeConfigurationFlags_DisableRenderAll  = 1 << 0,   //Only render visible nodes
    ofxOceanodeConfigurationFlags_DisableHistograms = 2 << 1    // Do not display histograms in vectors
};

struct macroCategory{
	macroCategory(std::string _name = "Parent") : name(_name){};
	std::string name;
	std::vector<std::pair<string, string>> macros;
	std::vector<std::shared_ptr<macroCategory>> categories;
};

class ofxOceanodeShared{
public:
    static unsigned int getDockspaceID(){
        return getInstance().getDockspaceIDFromInstance();
    }
    
    static unsigned int getCentralNodeID(){
        return getInstance().getCentralNodeIDFromInstance();
    }
    
    static unsigned int getLeftNodeID(){
        return getInstance().getLeftNodeIDFromInstance();
    }
    
    static void setDockspaceID(unsigned int _did){
        auto &instance = getInstance();
        if(instance.getDockspaceIDFromInstance() == 0){
            instance.setDockspaceIDFromInstance(_did);
        }
    }
    
    static void setCentralNodeID(unsigned int _cnid){
        auto &instance = getInstance();
        if(instance.getCentralNodeIDFromInstance() == 0){
            instance.setCentralNodeIDFromInstance(_cnid);
        }
    }
    
    static void setLeftNodeID(unsigned int _lnid){
        auto &instance = getInstance();
        if(instance.getLeftNodeIDFromInstance() == 0){
            instance.setLeftNodeIDFromInstance(_lnid);
        }
    }
	
	static std::shared_ptr<macroCategory> getMacroDirectoryStructure(){
		return getInstance().macroDirectoryStructure;
	}
	
	static void readMacros(){
		ofDirectory dir;
		if(!dir.doesDirectoryExist("Macros")){
			dir.createDirectory("Macros");
		}
		getInstance().macroDirectoryStructure = make_shared<macroCategory>();
		readDirectory("Macros", getInstance().macroDirectoryStructure);
	}
	
	static void readDirectory(string dirName, std::shared_ptr<macroCategory> category){
		ofDirectory dir;
		dir.open(dirName);
		dir.sort();
		for(auto &folder : dir.getFiles()){
			if(folder.isDirectory()){
				ofDirectory dir2(folder.path());
				dir2.sort();
				if(dir2.size() != 0 && (!dir2.getFile(0).isDirectory() ||
                                        (dir2.getFile(0).isDirectory() && ofStringTimesInString(dir2.getName(0), "Macro_") > 0))){
					category->macros.emplace_back(make_pair(folder.getFileName(), folder.path()));
				}else{
					category->categories.push_back(make_shared<macroCategory>(folder.getFileName()));
					readDirectory(folder.path(), category->categories.back());
				}
			}
		}
	}
	
	static void updateMacrosStructure(){
		readMacros();
	}
	
	static void macroUpdated(string macroPath){
		getInstance().macroUpdatedEvent.notify(macroPath);
	}
	
	static ofEvent<string>& getMacroUpdatedEvent(){
		return getInstance().macroUpdatedEvent;
	}
	
	static ofEvent<string>& getSnapshotUpdatedEvent() {
			static ofEvent<string> snapshotUpdatedEvent;
			return snapshotUpdatedEvent;
		}
		
		static void snapshotUpdated(const string& macroPath) {
			ofNotifyEvent(getSnapshotUpdatedEvent(), const_cast<string&>(macroPath));
		}
	
	static void addPortal(abstractPortal* _portal){
		getInstance().portalsMap[_portal->getName()].push_back(_portal);
	}
	
	static void removePortal(abstractPortal* _portal){
		auto& map = getInstance().portalsMap;
		auto it = map.find(_portal->getName());
		if(it != map.end()){
			auto& vec = it->second;
			vec.erase(std::remove(vec.begin(), vec.end(), _portal), vec.end());
			if(vec.empty()){
				map.erase(it);
			}
		}
	}
	
	static void updatePortalName(abstractPortal* _portal, const std::string& oldName, const std::string& newName){
		auto& map = getInstance().portalsMap;
		// Remove from old bucket
		auto itOld = map.find(oldName);
		if(itOld != map.end()){
			auto& vec = itOld->second;
			vec.erase(std::remove(vec.begin(), vec.end(), _portal), vec.end());
			if(vec.empty()){
				map.erase(itOld);
			}
		}
		// Insert into new bucket
		map[newName].push_back(_portal);
	}
	
	static void portalUpdated(abstractPortal* _portal){
		auto& map = getInstance().portalsMap;
		auto it = map.find(_portal->getName());
		if(it != map.end()){
			for(auto p : it->second){
				p->match(_portal);
			}
		}
	}
	   
	static void requestPortalUpdate(abstractPortal* _portal){
	 auto& map = getInstance().portalsMap;
	 auto it = map.find(_portal->getName());
	 if(it != map.end()){
	  for(auto p : it->second){
	   if(_portal->match(p)){
	   	break;
	   }
	  }
	 }
	}
    
    static ofEvent<void>& getPresetWillBeLoadedEvent(){
        return getInstance().presetWillBeLoadedEvent;
    }
    
    static ofEvent<void>& getPresetHasLoadedEvent(){
        return getInstance().presetHasLoadedEvent;
    }
    
    static void startedLoadingPreset(){
        getInstance().presetWillBeLoadedEvent.notify();
        getInstance().presetLoading = true;
    }
    
    static void finishedLoadingPreset(){
        getInstance().presetLoading = false;
        getInstance().presetHasLoadedEvent.notify();
    }
    
    static bool isPresetLoading(){
        return getInstance().presetLoading;
    }
    
    static int getConfigurationFlags(){
        return getInstance().configurationFlags;
    }
    
    static void setConfigurationFlags(ofxOceanodeConfigurationFlags f){
        getInstance().configurationFlags = f;
    }
	
	template<typename T>
	static vector<portal<T>*> getAllPortals() {
		vector<portal<T>*> typedPortals;
		
		for (auto& kv : getInstance().portalsMap) {
			for (auto* abstractP : kv.second) {
				if (abstractP != nullptr) {
					auto* typedPortal = dynamic_cast<portal<T>*>(abstractP);
					if (typedPortal != nullptr) {
						typedPortals.push_back(typedPortal);
					}
				}
			}
		}
		
		return typedPortals;
	}
	
	static std::string getCurrentPresetName(){
		return getInstance().currentPresetName;
	}

	static void setCurrentPresetName(const std::string& presetName){
		getInstance().currentPresetName = presetName;
	}
	
	static string getCurrentBankName(){
		return getInstance().currentBankName;
	}
	
	static void setCurrentBankName(const string& bankName){
		getInstance().currentBankName = bankName;
	}

	static std::string getCurrentPresetPath(){
		return getInstance().currentPresetPath;
	}

	static void setCurrentPresetPath(const std::string& presetPath){
		getInstance().currentPresetPath = presetPath;
	}
    
	// Snap to Grid
	static void setSnapToGrid(bool b){
		getInstance().snapToGrid = b;
	}
	
	static void setSnapGridDiv(int i){
		getInstance().snapGridDiv = i;
	}
	
	static bool getSnapToGrid(){
		return getInstance().snapToGrid;
	}
	
	static int getSnapGridDivs(){
		return getInstance().snapGridDiv;
	}

	// Auto Inspector show/hide
	static void setAutoInspectorShowHide(bool b){
		getInstance().autoInspectorShowHide = b;
	}

	static bool getAutoInspectorShowHide(){
		return getInstance().autoInspectorShowHide;
	}

	// Node dimensions (mirrors ofxOceanodeCanvas values, updated by the canvas)
	static int getNodeWidthWidget(){ return getInstance().nodeWidthWidget; }
	static int getNodeWidthText()  { return getInstance().nodeWidthText; }
	static void setNodeWidthWidget(int w){ getInstance().nodeWidthWidget = w; }
	static void setNodeWidthText(int t)  { getInstance().nodeWidthText = t; }

	// Base font size at zoom 1.0 — set by ofxOceanodeCanvas::setupFonts() and used
	// by any GUI code that needs to derive the current zoom factor at render time via
	// ImGui::GetFontSize() / ofxOceanodeShared::getZoomBaseFontSize().
	static float getZoomBaseFontSize(){ return getInstance().zoomBaseFontSize; }
	static void  setZoomBaseFontSize(float s){ getInstance().zoomBaseFontSize = s; }

	// Continuous zoom level — set by the active ofxOceanodeCanvas each frame before
	// rendering node GUIs, so any code that needs a smooth zoom factor can read it
	// instead of deriving a stepped value from the discrete font size.
	static float getZoomLevel(){ return getInstance().zoomLevel; }
	static void  setZoomLevel(float z){ getInstance().zoomLevel = z; }

	// Active canvas tracking
	static void setActiveCanvasUniqueID(const string& uid){
		auto& inst = getInstance();
		if(inst.activeCanvasUniqueID != uid){
			inst.activeCanvasUniqueID = uid;
			ofNotifyEvent(activeCanvasUniqueIDChangedEvent, inst.activeCanvasUniqueID);
		}
	}

	static const string& getActiveCanvasUniqueID(){
		return getInstance().activeCanvasUniqueID;
	}

	inline static ofEvent<string> activeCanvasUniqueIDChangedEvent;

	// Node selected in canvas — fired whenever a single node is clicked/selected
	// in any canvas (main or macro). Subscribers receive the pointer to the node
	// that became selected (nullptr means deselect-all with no new selection).
	inline static ofEvent<ofxOceanodeNode*> nodeSelectedInCanvasEvent;

	static void nodeSelectedInCanvas(ofxOceanodeNode* node){
		ofNotifyEvent(nodeSelectedInCanvasEvent, node);
	}

	static ofEvent<ofxOceanodeNode*>& getNodeSelectedInCanvasEvent(){
		return nodeSelectedInCanvasEvent;
	}

private:
    ofxOceanodeShared(){};
    
    static ofxOceanodeShared &getInstance(){
        static ofxOceanodeShared instance;
        return instance;
    }
    
    unsigned int getDockspaceIDFromInstance(){
        return dockspace_id;
    }
    unsigned int getCentralNodeIDFromInstance(){
        return centralNode_id;
    }
    unsigned int getLeftNodeIDFromInstance(){
        return leftNode_id;
    }
    
    void setDockspaceIDFromInstance(unsigned int _did){dockspace_id = _did;};
    void setCentralNodeIDFromInstance(unsigned int _cnid){centralNode_id = _cnid;};
    void setLeftNodeIDFromInstance(unsigned int _lnid){leftNode_id = _lnid;};
    
    unsigned int dockspace_id = 0;
    unsigned int centralNode_id = 0;
    unsigned int leftNode_id = 0;
	
    shared_ptr<macroCategory> macroDirectoryStructure;
	ofEvent<string> macroUpdatedEvent;
    
	bool presetLoading;
    ofEvent<void> presetWillBeLoadedEvent;
    ofEvent<void> presetHasLoadedEvent;
	string currentPresetName = "";
	string currentPresetPath = "";
	string currentBankName = "";

	std::unordered_map<std::string, std::vector<abstractPortal*>> portalsMap;
    
    ofxOceanodeConfigurationFlags configurationFlags;
	
	// Node dimensions
	int nodeWidthWidget = 150;
	int nodeWidthText   = 90;

	// Base (zoom=1.0) font size.  Initialised to 0 (unset); the canvas writes the
	// real value from ZOOM_FONT_SIZES[2] via setZoomBaseFontSize() in setupFonts(),
	// which runs before any node GUI is rendered.
	float zoomBaseFontSize = 0.0f;

	// Continuous zoom level (set by the active canvas each frame).
	float zoomLevel = 1.0f;

	// Snap to Grid
	bool snapToGrid = false;
	int snapGridDiv = 4;

	// Auto Inspector show/hide
	bool autoInspectorShowHide = true;

	// Active canvas tracking
	string activeCanvasUniqueID = "";
};

#endif /* ofxOceanodeShared_h */
