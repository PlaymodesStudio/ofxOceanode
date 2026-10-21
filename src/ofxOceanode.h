//
//  ofxOceanode.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanode_h
#define ofxOceanode_h

#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeTime.h"
#include "MacroMixerMixing.h"

//#include "ofxOceanodeAbstractTimeline.h"

#include "ofxImGui.h"
#include "imgui.h"
#include "router.h"
#include "portal.h"
#include "Nodes/Default_Nodes/portalSelector.h"
#include "bufferNode.h"
#include "bufferHeader.h"
#include "bufferHeaderPro.h"

class ofxOceanodeProportionalDocking;
class ofxOceanodeHelp;
class ofxOceanodeLayoutManager;

class ofxOceanode {
public:
    ofxOceanode();
    ~ofxOceanode();
    
    void setup();
    
    void update();
    
    void draw();
    
    void exit();
    
    template<typename T>
    void registerModel(string category = "Default"){
        nodeRegistry->registerModel<T>(category);
    };
    
    template<typename T, typename... Args>
    void registerModel(string category, Args... args){
        nodeRegistry->registerModel<T>(category, args...);
    };

    template<typename T, typename... Args>
    void registerModel(string category, ofxOceanodeNodeRegistrationFlags registrationFlags, Args... args){
        nodeRegistry->registerModel<T>(category, registrationFlags, args...);
    }

    template<typename T, typename... Args>
    bool refreshModel(string category, Args... args){
        return nodeRegistry->refreshModel<T, Args...>(category, args...);
    }

    // The same when the node's name is known (no model built to read it)
    template<typename T, typename... Args>
    bool refreshModelNamed(const string& name, string category, Args... args){
        return nodeRegistry->refreshModelNamed<T, Args...>(name, category, args...);
    }

    // Addons own their menu action and optional background-task/UI callbacks.
    void registerConfigMenuItem(const std::string &label, std::function<void()> selected,
                                std::function<void()> update = {}, std::function<void()> draw = {},
                                std::function<bool()> enabled = {}){
        configMenuItems[label] = {std::move(selected), std::move(update), std::move(draw), std::move(enabled)};
    }

    // Utility actions appear in Help > Utilities and keep running when its menu is closed.
    void registerUtilityMenuItem(const std::string &label, std::function<void()> selected,
                                 std::function<void()> update = {}, std::function<void()> draw = {},
                                 std::function<bool()> enabled = {}){
        utilityMenuItems[label] = {std::move(selected), std::move(update), std::move(draw), std::move(enabled)};
    }
    
    template<typename T>
    void registerBuiltInType(string name, T defaultValue){
        typesRegistry->registerType<T>(name);
        nodeRegistry->registerModel<portalSelector<T>>("Portal", name, defaultValue);
    };

    template<typename T>
    void registerType(string name = typeid(T).name(), T defaultValue = T()){
        typesRegistry->registerType<T>(name);
        nodeRegistry->registerModel<router<T>>("Router", name, defaultValue);
		nodeRegistry->registerModel<portal<T>>("Portal", name, defaultValue);
		nodeRegistry->registerModel<portalSelector<T>>("Portal", name, defaultValue);
    };
    template<typename T>
    void registerType(string name,T defaultValue,MacroMixerMixing::Definition<T> mixing){
        registerType<T>(name,defaultValue);
        auto& registry=MacroMixerMixing::registry();
        if(registry.registerValueType<T>(mixing.type,"Router "+name,mixing.validate,defaultValue)) {
            registry.registerValuePolicy<T>(mixing.type,mixing.policy,mixing.label,mixing.mix,mixing.options,true);
            if(mixing.allowSwitch) registry.registerSwitchPolicy(mixing.type);
        }
    }
    
    template<typename T1, typename T2 = T1>
    void registerTypeWithBufferAndHeader(string name = typeid(T1).name(), T1 defaultValue = T1(),
                      std::function<void(T1&, T2&)> bufferAssignFunction = [](T1 &data, T2 &container){container = data;},
                      std::function<T1(T2&)> bufferReturnFunction = [](T2 &container)->T1{return container;},
                      std::function<bool(T1&)> bufferCheckFunction = [](T1 &data)->bool{return true;}){
        typesRegistry->registerType<T1>(name);
        nodeRegistry->registerModel<router<T1>>("Router", name, defaultValue);
        nodeRegistry->registerModel<portal<T1>>("Portal", name, defaultValue);
        nodeRegistry->registerModel<portalSelector<T1>>("Portal", name, defaultValue);
        nodeRegistry->registerModel<bufferNode<T1, T2>>("Buffer", name, defaultValue, false, bufferAssignFunction, bufferReturnFunction, bufferCheckFunction);
        typesRegistry->registerType<buffer<T1, T2>*>("buffer_" + name);
        nodeRegistry->registerModel<router<buffer<T1, T2>*>>("Router", "buffer_" + name, nullptr);
        nodeRegistry->registerModel<portal<buffer<T1, T2>*>>("Portal", "buffer_" + name, nullptr);
        nodeRegistry->registerModel<portalSelector<buffer<T1, T2>*>>("Portal", "buffer_" + name, nullptr);
        nodeRegistry->registerModel<bufferHeader<T1, T2>>("Header", name, defaultValue);
        nodeRegistry->registerModel<bufferHeaderPro<T1, T2>>("HeaderPro", name, defaultValue);
        registerType<std::vector<T1>>("v_" + name, std::vector<T1>(1, defaultValue));
    };
    
    template<typename T>
    shared_ptr<T> addController(){
        return controls->addController<T>();
    }
    
    template<typename T>
    shared_ptr<T> getController(){
        return controls->get<T>();
    }

    std::shared_ptr<ofxOceanodeContainer> getContainer() const { return container; }
    void addController(std::shared_ptr<ofxOceanodeBaseController> controller) {
        controls->getControllers().push_back(std::move(controller));
    }

    void loadPreset(std::string presetPathRelativeToData); //call preset via path
    
    void loadPreset(std::string bank, std::string name); //call preset via bank and name

    ofxOceanodeTimelineManager& getTimelineManager(){ return container->getTimelineManager(); }
    const ofxOceanodeTimelineManager& getTimelineManager() const { return container->getTimelineManager(); }
    
   template<typename T>
    void registerScope(std::function<void(ofxOceanodeAbstractParameter* p, ImVec2 size)> func){
        ofxOceanodeScope::getInstance()->addScopeRenderer(
            typeid(T).name(),
            std::move(func),
            true
        );
    }
    
    void togglePlay(){
        oceanodeTime->togglePlay();
    }
    
    //TODO: Clean
    // From imgui_demo.cpp
    //-----------------------------------------------------------------------------
    // [SECTION] Example App: Docking, DockSpace / ShowExampleAppDockSpace()
    //-----------------------------------------------------------------------------
    
    // Demonstrate using DockSpace() to create an explicit docking node within an existing window.
    // Note that you already dock windows into each others _without_ a DockSpace() by just moving windows
    // from their title bar (or by holding SHIFT if io.ConfigDockingWithShift is set).
    // DockSpace() is only useful to construct to a central location for your application.
    void ShowExampleAppDockSpace(bool* p_open);
    void cycleDockedTabs();
    
    void disableRenderAll();
    void disableRenderHistograms();
    
    void setShowMode(bool b){showMode = b;}
    void toggleShowMode(){showMode = !showMode;}
	
	void saveConfig();
	void loadConfig();
	void saveViewConfig(const std::map<std::string, bool>& visibilityMap);
	void loadViewConfig();
    void loadDefaultGUILayout(); // Existing host API, delegated to the layout manager.
	void saveTheme(const std::string& name);
	void loadTheme(const std::string& name);

private:
    void registerDefaultTypes();      // ofxOceanodeDefaultTypes.cpp
    void registerDefaultNodes();      // ofxOceanodeDefaultNodes.cpp
    void registerDefaultRouting();    // ofxOceanodeDefaultRouting.cpp
    struct MenuItemCallbacks {
        std::function<void()> selected, update, draw;
        std::function<bool()> enabled;
    };
    std::map<std::string, MenuItemCallbacks> configMenuItems, utilityMenuItems;
    ofxOceanodeCanvas canvas;
    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeContainer::UnusedNodePlan unusedNodePlan;
    std::weak_ptr<ofxOceanodeContainer> unusedNodeContainer;
    bool showUnusedNodesWindow = false;
    bool focusUnusedNodesWindow = false;
    std::string unusedNodesSelectedNode;
    void drawUnusedNodesWindow();
    bool unusedNodesPlanChanged = false;
    unique_ptr<ofxOceanodeControls> controls;
    unique_ptr<ofxOceanodeHelp> help;
    
    shared_ptr<ofxOceanodeNodeRegistry> nodeRegistry;
    shared_ptr<ofxOceanodeTypesRegistry> typesRegistry;
    ofxOceanodeScope* scope;
    
    ofxOceanodeTime* oceanodeTime;
	
	int desiredFPS = 60;

    
//    vector<ofxOceanodeAbstractTimeline> timelines;
    
    ofxImGui::Gui gui;
    std::unique_ptr<ofxOceanodeProportionalDocking> proportionalDocking;
    void showManualWindow(bool *b);
    void showHelpPopUp();
    bool showMode;
	bool snapToGrid;
    std::map<std::string, bool> savedViewConfig;

    void drawShowModeWindow();
    
    ofParameter<int> oscReceiverPort;
#ifdef OFXOCEANODE_USE_OSC
    ofxOscReceiver oscReceiver;
    ofEventListener receiverPortChanged;
    
    void receiveOsc();
#endif
    
    ofEventListener presetLoadedListener;
    ofEventListener presetSavedListener;
    bool presetCanvasCenterPending = false;

    // Metadata for the most recently loaded ImGui layout. ImGui restores only
    // windows present in the .ini file, so controllers introduced after the
    // layout was saved dock beside Nodes (legacy layouts also work).
    std::string loadedGUILayoutData;
    ImGuiID loadedGUILayoutLeftDockId = 0;
    bool pendingControllerDockReconciliation = false;
    ImGuiID resolveControllerDockID() const;
    void reconcileMissingControllerWindows();

    void applyGUILayout(const std::string& data);
    std::unique_ptr<ofxOceanodeLayoutManager> layouts;

    // Theme editor state
    bool showThemeEditor = false;
    bool showThemeColorSearch = false;
    char themeColorSearch[128] = "";
    void drawThemeEditorWindow();
    struct ThemeFontFamily {
        std::string name;
        std::string regularPath;
        std::string boldPath;
    };
    std::vector<ThemeFontFamily> themeFontFamilies;
    std::string themeFontFamily; // Empty selects the built-in JetBrains Mono pair.
    std::string themeFontWarning;
    bool pendingThemeFontChange = true;
    void refreshThemeFonts();
    void applyThemeFont();

    // Theme save/load state
    std::string currentThemeName;
    bool openSaveThemePopup = false;
    char themeNameBuf[128] = {};
};

#endif /* ofxOceanode_h */
