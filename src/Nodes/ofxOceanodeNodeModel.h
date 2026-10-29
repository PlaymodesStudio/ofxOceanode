//
//  ofxOceanodeNodeModel.hpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanodeNodeModel_h
#define ofxOceanodeNodeModel_h

#include "ofMain.h"
#include "ofxOceanodeTransport.h"
#include "ofxOceanodeTime.h"
#include "ofxOceanodeParameter.h"

class ofxOceanodeContainer;
class ofxOscMessage;

// A node that plays notes (see ofxOceanodeNodeModel::getTimelineInstrument):
// the parameters a timeline piano roll drives. Gate is a level (velocity rides
// on it when velocity is null); voices is the width the node reads, one note
// per slot. A kit plays a fixed slot per key instead: key 36 (C1) is slot 0.
#define OFXOCEANODE_HAS_TIMELINE_INSTRUMENT 1
struct ofxOceanodeTimelineInstrument {
    ofxOceanodeAbstractParameter* gate = nullptr;
    ofxOceanodeAbstractParameter* pitch = nullptr;
    ofxOceanodeAbstractParameter* velocity = nullptr;
    int voices = 0;
    bool kit = false;
};

typedef int ofxOceanodeNodeModelFlags;

enum ofxOceanodeNodeModelFlags_
{
    ofxOceanodeNodeModelFlags_None              = 0,
    ofxOceanodeNodeModelFlags_WaitForFrame      = 1 << 0,   //
    ofxOceanodeNodeModelFlags_FrameDone         = 1 << 1,   //
    ofxOceanodeNodeModelFlags_ForceFrameMode    = 1 << 2,
	ofxOceanodeNodeModelFlags_TransparentNode   = 1 << 3,   //
    // Visual nodes keep their GUI/layout at low zoom and handle text visibility themselves.
    ofxOceanodeNodeModelFlags_KeepGuiVisibleAtLowZoom = 1 << 4

};

class ofxOceanodeNodeModel {
public:
    using customGuiRegion = ofParameter<std::function<void()>>;
    
    ofxOceanodeNodeModel(string _name);
    virtual ~ofxOceanodeNodeModel(){ profilingRecord->setEnabled(false); profilingRecord->identity = {}; };
    const std::shared_ptr<oceanodeProfiling::Record>& getProfilingRecord() const { return profilingRecord; }
    
    virtual void setup(){};
	virtual void setup(string additionalInfo){};
    virtual void update(ofEventArgs &e){};
    virtual void draw(ofEventArgs &e){};
    // Stable capability metadata, available before setup. MacroMixer analyses
    // this declaration recursively and rejects the Macro before constructing it.
    // Ordinary Macro loading does not apply this restriction.
    virtual std::string macroMixerBlocker() const { return {}; }
    // MacroMixer morphs between saved patches. By default it interpolates the
    // parameter group and treats any other differing saved key as a
    // configuration change, which replaces the node. A node that blends its
    // own saved state names those keys (blended), names saved keys that never
    // affect the sound (ignored), and implements morphCheck/Begin/Apply/End.
    // Blended keys may include registered parameters: the node owns their
    // interpolation too, so MacroMixer never writes them a second time.
    struct MorphKeys { std::vector<std::string> blended, ignored; };
    // Interpolate: the default. Switch: jumps half way. Skip: the node drives
    // it (or it is not sound). Structural: a different value replaces the node.
    enum class MorphHint { Interpolate, Switch, Skip, Structural };
    virtual MorphKeys morphKeys() const { return {}; }
    virtual MorphHint morphHint(const ofxOceanodeAbstractParameter&) const { return MorphHint::Interpolate; }
    // Whether the node can blend into these saved settings, without side effects.
    virtual bool morphCheck(const ofJson& target, std::string& reason) const { return true; }
    // Captures the current state as the start. Called once, before morphApply.
    virtual bool morphBegin(const ofJson& target, std::string& reason) { reason = "Node does not morph its saved state"; return false; }
    // t from 0 (start) to 1 (target); switched: discrete values take the target's.
    virtual void morphApply(float t, bool switched) {}
    // completed: the target was reached; otherwise restore the start.
    virtual void morphEnd(bool completed) {}
    // A sink does useful work on its own: it renders, sends, records, plays,
    // drives hardware or changes global state. Dead-node cleanup keeps sinks
    // and everything that feeds them. Declare one with the Sink registration
    // flag, or override this when the status is fixed for the class.
    // By default a node that publishes no output pin exists for its effect, so
    // it is a sink; this errs toward keeping. Override and return false for a
    // pure model whose only output is an ordinary parameter (a number box).
    virtual bool isSink() const { return declaredSink || !hasOutputPin(); }
    // True when the node publishes at least one pin it writes.
    bool hasOutputPin() const {
        for(const auto& abstract : parameters){
            auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
            if(parameter != nullptr && parameter->isNodeOutput()) return true;
        }
        return false;
    }
    void setDeclaredSink(bool sink){ declaredSink = sink; }

    // Return true when a model renders an Inspector row itself, or hides it.
    // Registered parameters still keep their normal preset/undo behavior.
    virtual bool drawInspectorParameter(ofAbstractParameter &parameter){ return false; }
    // Optional node-row renderer; the parameter keeps its pins and value tracking.
    virtual bool drawNodeParameter(ofxOceanodeAbstractParameter&, float, float){ return false; }
    // Optional display text beside the node title; does not change its identity.
    virtual std::string getNodeTitleSuffix() const { return {}; }
    // Hover text for the title suffix, which may add detail the header has no room for.
    virtual std::string getNodeTitleTooltip() const { return getNodeTitleSuffix(); }
    
    //get parameterGroup
    ofParameterGroup &getParameterGroup(){return parameters;};
    ofParameterGroup &getInspectorParameterGroup(){return inspectorParameters;};
    
    //getters
    string nodeName(){return nameIdentifier;};
    unsigned int getNumIdentifier(){return numIdentifier;};
    void setNumIdentifier(unsigned int num);
        
    ofEvent<string> parameterChangedMinMax;
    ofEvent<string> dropdownChanged;
    ofEvent<void> parameterGroupChanged;
    
    virtual void setBpm(float _bpm){};
    virtual void resetPhase(){};
    
    virtual void presetSave(ofJson &json){};
    // Models with computed presetSave fields can require an explicit user edit.
    virtual bool shouldReviewPresetSaveField(const std::string&, const ofJson&) const { return true; }
    virtual void clearUserEditMarkers() {};
	virtual void macroSave(ofJson &json, string path){};
    virtual void presetRecallBeforeSettingParameters(ofJson &json){};
    virtual void presetRecallAfterSettingParameters(ofJson &json){};
    virtual void loadCustomPersistent(ofJson &json){};
	virtual void macroLoad(ofJson &json, string path){};
    
    virtual void presetWillBeLoaded(){};
    virtual void presetHasLoaded(){};
    virtual void loadBeforeConnections(ofJson &json){};
    
    virtual void activateConnections(){};
    virtual void deactivateConnections(){};

    // Sink pins included by "Encapsulate All Subgraphs".  Every connected
    // input can be encapsulated on its own; models such as mixers narrow the
    // "all" action to their relevant inputs.
    virtual bool canEncapsulateSubgraphFrom(ofxOceanodeAbstractParameter&) const { return true; }
    // How the timeline should scale a parameter's curves: "dB" for a gain
    // (amplitude, 0 = silence) drawn and edited in decibels; "" = linear.
    // Read when the parameter is added to a timeline track.
    virtual std::string getTimelineValueScale(ofxOceanodeAbstractParameter&) const { return {}; }
    // An instrument: its note inputs, so a piano roll binds to the node as a
    // whole. Asked when a piano roll is set up and on every timeline update
    // (voice count, kit), so keep it cheap.
    virtual bool getTimelineInstrument(ofxOceanodeTimelineInstrument&) { return false; }
    // Optional native/dockable editor navigation, shared by timeline and other UIs.
    // Existing SC nodes expose these title/toggle methods through scNode.
    virtual std::string getWindowTitle() { return {}; }
    virtual ofParameter<bool>* getWindowToggle() { return nullptr; }
    virtual bool canShowNodeWindow() { return getWindowToggle() != nullptr && !getWindowTitle().empty(); }
    virtual void showNodeWindow() {
        if(auto* toggle = getWindowToggle()) toggle->set(true);
    }
    // The ImGui window name to draw that editor under (Begin, FindWindowByName).
    // A host running the node outside its patch (a MacroMixer source) sets a
    // label naming the source: the window stays apart from the patch's own and
    // docks by the same name across sessions. Otherwise the window title.
    std::string getWindowId() { return windowHostLabel.empty() ? getWindowTitle() : windowHostLabel; }
    void setWindowHostLabel(const std::string& label) { windowHostLabel = label; }

    // Query without changing the node; bind=true may publish its note inputs.
    // Reuse the same instrument/voice metadata for a hardware MIDI keyboard.
    virtual bool getMidiKeyboardInstrument(ofxOceanodeTimelineInstrument& out, bool bind = false) {
        return getTimelineInstrument(out);
    }
    
    virtual void deactivate(){};
    virtual void activate(){};
    void setActive(bool _active){active = _active;};
    
	void deserializeParameter(ofJson &json, ofAbstractParameter &p);
    
    ofColor getColor(){return color;};
    string getDescription();
    
    //For Macro
    virtual bool receiveOscMessage(ofxOscMessage &m){return false;};
	virtual void setContainer(ofxOceanodeContainer* container);
    ofxOceanodeContainer* getHostContainer() const { return hostContainer; }
    std::shared_ptr<ofxOceanodeTransport> getTransport() const;
    ofxOceanodeTransportState getTransportState() const;
    ofxOceanodeFrameTransportState getFrameTransportState() const;
    ofxOceanodeTimeState getGlobalTimeState() const;
    ofxOceanodeFrameTimeState getFrameGlobalTimeState() const;
    
    void addInspectorParameter(ofAbstractParameter& p){
        inspectorParameters.add(p);
    }
    template<typename T> void addInspectorParameter(ofParameter<T>& p){
        auto hook = oceanodeProfiling::ParameterHook::bind(p);
        hook->setRecord(profilingRecord);
        inspectorProfilingHooks.push_back(std::move(hook));
        inspectorParameters.add(p);
    }
    
    shared_ptr<ofxOceanodeAbstractParameter> addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags = 0);
	
	template<typename ParameterType>
	shared_ptr<ofxOceanodeParameter<ParameterType>> addParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
		//TODO: Review if we loose the data?
		auto oceaParam = make_shared<ofxOceanodeParameter<ParameterType>>();
		oceaParam->bindParameter(p);
		oceaParam->setFlags(flags);
        oceaParam->setNodeModel(this);
		parameters.add(*oceaParam);
		return dynamic_pointer_cast<ofxOceanodeParameter<ParameterType>>(*(parameters.end()-1));
	}
    
    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addParameter(std::shared_ptr<ofParameter<ParameterType>> p, ofxOceanodeParameterFlags flags = 0){
        //TODO: Review if we loose the data?
        auto oceaParam = make_shared<ofxOceanodeParameter<ParameterType>>();
        oceaParam->bindParameter(p);
        oceaParam->setFlags(flags);
        oceaParam->setNodeModel(this);
        parameters.add(*oceaParam);
        return dynamic_pointer_cast<ofxOceanodeParameter<ParameterType>>(*(parameters.end()-1));
    }
	
    // Pin roles: who writes the pin. An input keeps both ports, so its value
    // can be passed on, and is never written by the node (same as addParameter).
    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addInputParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
        return addParameter(p, flags);
    }

    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addInputParameter(std::shared_ptr<ofParameter<ParameterType>> p, ofxOceanodeParameterFlags flags = 0){
        return addParameter(p, flags);
    }

    // An input the node also writes for downstream nodes (Phasor fires Reset at
    // each cycle). Both ports; held back like an output by Process on update?.
    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addInputAndOutputParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
        return addParameter(p, flags | ofxOceanodeParameterFlags_Output);
    }

    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addInputAndOutputParameter(std::shared_ptr<ofParameter<ParameterType>> p, ofxOceanodeParameterFlags flags = 0){
        return addParameter(p, flags | ofxOceanodeParameterFlags_Output);
    }

	template<typename ParameterType>
	shared_ptr<ofxOceanodeParameter<ParameterType>> addOutputParameter(ofParameter<ParameterType>& p, ofxOceanodeParameterFlags flags = 0){
		auto oceaParam = addParameter(p, flags | outputParameterFlags);
		return oceaParam;
	}

    template<typename ParameterType>
    shared_ptr<ofxOceanodeParameter<ParameterType>> addOutputParameter(std::shared_ptr<ofParameter<ParameterType>> p, ofxOceanodeParameterFlags flags = 0){
        return addParameter(p, flags | outputParameterFlags);
    }

    // Dropdown variants of the pin roles; same overloads as addParameterDropdown.
    template<typename ParameterType>
    auto addInputParameterDropdown(ofParameter<ParameterType> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        return addParameterDropdown(dropdownSelector, name, defaultPos, options, flags);
    }

    template<typename ParameterType>
    auto addInputAndOutputParameterDropdown(ofParameter<ParameterType> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        return addParameterDropdown(dropdownSelector, name, defaultPos, options, flags | ofxOceanodeParameterFlags_Output);
    }

    template<typename ParameterType>
    auto addOutputParameterDropdown(ofParameter<ParameterType> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        return addParameterDropdown(dropdownSelector, name, defaultPos, options, flags | outputParameterFlags);
    }

    shared_ptr<ofxOceanodeParameter<int>> addParameterDropdown(ofParameter<int> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        dropdownSelector.set(name, defaultPos, 0, options.size()-1);
		auto op = addParameter(dropdownSelector, flags);
		op->setDropdownOptions(options);
        return op;
    }
    
    shared_ptr<ofxOceanodeParameter<vector<int>>> addParameterDropdown(ofParameter<vector<int>> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        dropdownSelector.set(name, {defaultPos}, {0}, vector<int>(1, options.size()-1));
        auto op = addParameter(dropdownSelector, flags);
        op->setDropdownOptions(options);
        return op;
    }
    
    shared_ptr<ofxOceanodeParameter<float>> addParameterDropdown(ofParameter<float> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        dropdownSelector.set(name, defaultPos, 0, options.size()-1);
        auto op = addParameter(dropdownSelector, flags);
        op->setDropdownOptions(options);
        return op;
    }
    
    shared_ptr<ofxOceanodeParameter<vector<float>>> addParameterDropdown(ofParameter<vector<float>> &dropdownSelector, string name, int defaultPos, vector<string> options, ofxOceanodeParameterFlags flags = 0){
        dropdownSelector.set(name, {(float)defaultPos}, {0}, vector<float>(1, options.size()-1));
        auto op = addParameter(dropdownSelector, flags);
        op->setDropdownOptions(options);
        return op;
    }
    
    shared_ptr<ofxOceanodeParameter<std::function<void()>>> addCustomRegion(customGuiRegion &p, std::function<void()> func){
        return addParameter(p.set(func), ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisableOutConnection | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
    }
    
	// SEPARATORS FOR THE NODE GUI
    void addSeparator(string name = "", ofColor color = ofColor(200, 200, 200, 255));
	void removeSeparator(string name);

	// SEPARATORS FOR THE INSPECTOR GUI
	void addInspectorSeparator(string name = "", ofColor color = ofColor(200, 200, 200, 255));
	// Label on a solid bar: labelColor text over an opaque background.
	void addInspectorSeparator(string name, ofColor labelColor, ofColor backgroundColor);
	
    template<typename ParameterType>
    ofParameter<ParameterType>& getParameter(string name){
        return static_cast<ofxOceanodeAbstractParameter&>(parameters.get(name)).cast<ParameterType>().getParameter();
    }
    
    template<typename ParameterType>
    ofParameter<ParameterType>& getInspectorParameter(string name){
        return inspectorParameters.get(name).cast<ParameterType>();
    }
    
    template<typename ParameterType>
    ofxOceanodeParameter<ParameterType>& getOceanodeParameter(ofParameter<ParameterType> p){
        return static_cast<ofxOceanodeAbstractParameter&>(parameters.get(p.getName())).cast<ParameterType>();
    }
    
    bool removeParameter(string parameterName){
        parameters.remove(parameterName);
		return true;
    }
    
    bool removeInspectorParameter(string parameterName){
        inspectorParameters.remove(parameterName);
        return true;
    }
    
    ofEvent<std::pair<ofJson, string>> deserializeParameterEvent;
    
    ofxOceanodeNodeModelFlags getFlags(){return flags;};
    void setFlags(ofxOceanodeNodeModelFlags f){flags = f;};

    // Transparent nodes may override this to supply a fill colour for the minimap.
    // The default returns hasHint=false, which means "skip me on the minimap".
    struct MiniMapHint {
        bool    hasHint   = false;
        ofColor fillColor;
    };
    virtual MiniMapHint getMiniMapHint() const { return {}; }
    
    string getParents();
    
    ofEvent<void> deleteModule;
    
    void deleteSelf(){
        deleteModule.notify();
    }
    
    bool getActive(){
        return active;
    }
    
protected:
    static constexpr ofxOceanodeParameterFlags outputParameterFlags =
        ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized | ofxOceanodeParameterFlags_Output;

    ofColor color;
	string canvasID;
    string description;
    ofxOceanodeContainer* hostContainer = nullptr;
    
private:
    std::string windowHostLabel;
    std::shared_ptr<oceanodeProfiling::Record> profilingRecord = std::make_shared<oceanodeProfiling::Record>();
    std::vector<std::shared_ptr<oceanodeProfiling::ParameterHook>> inspectorProfilingHooks;
    string nameIdentifier;
    unsigned int numIdentifier;
	ofParameterGroup parameters;
    ofParameterGroup inspectorParameters;
    ofEventListeners eventListeners;
    ofxOceanodeNodeModelFlags flags;
    bool active;
    bool declaredSink = false;
};

#endif /* ofxOceanodeNodeModel_h */
