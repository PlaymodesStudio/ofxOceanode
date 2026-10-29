//
//  router.h
//  example-basic
//
//  Created by Eduard Frigola on 21/04/2020.
//

#ifndef router_h
#define router_h

#include "ofxOceanodeNodeModel.h"
#include <type_traits>
#include "imgui.h"

class abstractRouter : public ofxOceanodeNodeModel{
public:
    abstractRouter(string typelabel) : ofxOceanodeNodeModel("Router " + typelabel){
        description = "To Send " + typelabel + " values to and from a macro and it's parent";
        addParameter(nameParam.set("Name", typelabel));
        addInspectorParameter(snapshotExclude.set("Exclude From Snapshots", false));
    };
    ~abstractRouter(){};
    
    // A router is the macro's interface pin: removing it changes the parent's pins.
    bool isSink() const override { return true; }
    
    ofParameter<string>& getNameParam(){
        return nameParam;
    }
    
    void loadBeforeConnections(ofJson &json){
        if(!json.count(nameParam.getEscapedName())) return;
        if(json[nameParam.getEscapedName()].is_string()){
            nameParam = json[nameParam.getEscapedName()].get<std::string>();
        }else if(json[nameParam.getEscapedName()].is_number_integer() || json[nameParam.getEscapedName()].is_number_unsigned() || json[nameParam.getEscapedName()].is_number_float() || json[nameParam.getEscapedName()].is_boolean()){
            nameParam = json[nameParam.getEscapedName()].dump();
        }else{
            ofLogWarning("abstractRouter") << "Failed to deserialize preset value for " << nameParam.getName() << ": invalid json type for string parameter";
        }
    }
    
    string type(){
        return typeid(*this).name();
    }
    
    bool isExcludeFromSnapshot(){return snapshotExclude;}
    ofParameter<bool>& getSnapshotExcludeParam(){ return snapshotExclude; }
protected:
    ofParameter<string> nameParam;
    ofParameter<bool> snapshotExclude;
};

template<typename T>
class router : public abstractRouter{
public:
    router(string typelabel, T val, T _min, T _max) : abstractRouter(typelabel){
        addParameter(value.set("Value", val, _min, _max));
        addParameter(min.set("Min", _min, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
        addParameter(max.set("Max", _max, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
    };
    
    router(string typelabel, T val) : abstractRouter(typelabel){
        addParameter(value.set("Value", val));
        if constexpr(std::is_same_v<T,bool>) {
            // Macro's existing type adapter reads these even though Boolean
            // routers expose no Min/Max controls. Never pass indeterminate bools.
            min.set("Min",false); max.set("Max",true);
            value.setMin(false); value.setMax(true);
        } else if constexpr(std::is_pointer_v<T>) {
            // Type adapters also read the hidden bounds of Texture routers.
            // They have no numeric range; never copy uninitialized addresses.
            min.set("Min",nullptr); max.set("Max",nullptr);
        }
    };
    
    ofParameter<T> &getValue(){return value;};
    ofParameter<T> &getMin(){return min;};
    ofParameter<T> &getMax(){return max;};
    
protected:
    ofParameter<T> value;
    ofParameter<T> min;
    ofParameter<T> max;
};

template<typename T>
class router<std::vector<T>> : public abstractRouter{
public:
    router(string typelabel, T val, T _min, T _max) : abstractRouter(typelabel){
        addParameter(value.set("Value", std::vector<T>(1, val), std::vector<T>(1, _min), std::vector<T>(1, _max)));
        addParameter(min.set("Min", std::vector<T>(1, _min), std::vector<T>(1, std::numeric_limits<T>::lowest()), std::vector<T>(1, std::numeric_limits<T>::max())));
        addParameter(max.set("Max", std::vector<T>(1, _max), std::vector<T>(1, std::numeric_limits<T>::lowest()), std::vector<T>(1, std::numeric_limits<T>::max())));
    };
    
    router(string typelabel, T val) : abstractRouter(typelabel){
        addParameter(value.set("Value", std::vector<T>(1, val)));
    };
    
    router(string typelabel, vector<T> val) : abstractRouter(typelabel){
        addParameter(value.set("Value", val));
    };
    
    ofParameter<std::vector<T>> &getValue(){return value;};
    ofParameter<std::vector<T>> &getMin(){return min;};
    ofParameter<std::vector<T>> &getMax(){return max;};
    
protected:
    ofEventListeners listeners;
    ofParameter<std::vector<T>> value;
    ofParameter<std::vector<T>> min;
    ofParameter<std::vector<T>> max;
};

template<>
class router<void> : public abstractRouter{
public:
    router(string typelabel) : abstractRouter(typelabel){
        addParameter(value.set("Val"));
    };
    
    ofParameter<void> &getValue(){return value;};
    
protected:
    ofParameter<void> value;
};

class routerDropdown : public abstractRouter {
public:
    routerDropdown() : abstractRouter("Dropdown") {
        description = "Inherits dropdown labels from connected Value parameters. Options is an optional override. Conflicting lists can be resolved with Options Reference in the inspector.";
        addParameterDropdown(value, "Value", 0, {"No dropdown connected"});
        value.setMax(std::numeric_limits<int>::max());
        getOceanodeParameter(value).setDropdownRelay(true);
        addParameter(options.set("Options", {}));
        addInspectorParameter(reference.set("Options Reference", ""));
        optionsListener = options.newListener([this](vector<string>&) {
            ofxOceanodeAbstractParameter::touchDropdownGraph();
        });
        referenceListener = reference.newListener([this](string&) {
            ofxOceanodeAbstractParameter::touchDropdownGraph();
        });
    }

    void update(ofEventArgs&) override { refreshOptions(); }

    void refreshOptions(){
        auto& parameter = getOceanodeParameter(value);
        // Coalesce graph/metadata edits and resolve on the normal node update,
        // after connection constructors and preset restoration have finished.
        const auto revision = ofxOceanodeAbstractParameter::getDropdownGraphRevision();
        if(observedRevision == revision) return;
        const bool explicitOptions = parameterForOptions().hasInConnection() || !options.get().empty();
        parameter.setDropdownRelay(!explicitOptions);
        vector<string> resolved;
        candidates.clear();
        status.clear();
        if(explicitOptions){
            resolved = options.get();
        }else{
            auto sources = parameter.getConnectedDropdownSources();
            vector<string> common;
            bool first = true, conflict = false, selected = false;
            for(auto* source : sources){
                const string identity = sourceIdentity(*source);
                candidates.push_back(identity);
                auto labels = source->getDropdownOptions();
                if(first){ common = labels; first = false; }
                else if(common != labels) conflict = true;
                if(reference.get() == identity){ resolved = labels; selected = true; }
            }
            std::sort(candidates.begin(), candidates.end());
            if(!selected){
                if(conflict) status = "Conflicting dropdowns: choose Options Reference in inspector";
                else resolved = common;
            }
        }
        if(resolved.empty() && status.empty()) status = "No dropdown connected";
        currentOptions = resolved;
        parameter.setDropdownSourceAvailable(!resolved.empty());
        // Missing metadata during preset loading must not reset the saved index.
        value.setMin(0);
        value.setMax(resolved.empty() ? std::numeric_limits<int>::max() : (int)resolved.size() - 1);
        if(!resolved.empty() && (value.get() < 0 || value.get() >= (int)resolved.size())) value = 0;
        parameter.setDropdownOptions(resolved.empty() ? vector<string>{status} : resolved);
        observedRevision = ofxOceanodeAbstractParameter::getDropdownGraphRevision();
    }

    bool drawInspectorParameter(ofAbstractParameter& parameter) override {
        if(!parameter.isReferenceTo(reference)) return false;
        refreshOptions();
        ImGui::TextUnformatted("Options Reference");
        ImGui::SameLine();
        ImGui::PushID(this);
        if(ImGui::BeginCombo("##optionsReference", reference.get().empty() ? "Automatic" : reference.get().c_str())){
            if(ImGui::Selectable("Automatic", reference.get().empty())) reference = "";
            for(const auto& candidate : candidates){
                if(ImGui::Selectable(candidate.c_str(), reference.get() == candidate)) reference = candidate;
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        if(!status.empty()) ImGui::TextUnformatted(status.c_str());
        return true;
    }

    string getNodeTitleSuffix() const override { return status.empty() ? "" : "(?)"; }
    string getNodeTitleTooltip() const override { return status; }
    ofParameter<int>& getValue(){return value;}
    ofParameter<int>& getMin(){return dummyMin;}
    ofParameter<int>& getMax(){return dummyMax;}
    vector<string> getCurrentOptions(){ refreshOptions(); return currentOptions; }

private:
    string sourceIdentity(ofxOceanodeAbstractParameter& source){
        auto* model = source.getNodeModel();
        if(!model) return source.getName();
        // Relative canvas paths keep inspector choices valid when a preset or
        // a macro instance is loaded under a different parent canvas.
        auto ownPath = ofSplitString(getParents(), " / ");
        auto sourcePath = ofSplitString(model->getParents(), " / ");
        size_t common = 0;
        while(common < ownPath.size() && common < sourcePath.size() && ownPath[common] == sourcePath[common]) ++common;
        string identity;
        for(size_t i = common; i < ownPath.size(); ++i) identity += "../";
        for(size_t i = common; i < sourcePath.size(); ++i) identity += sourcePath[i] + "/";
        return identity + model->getParameterGroup().getName() + "/" + source.getName();
    }

    ofxOceanodeParameter<vector<string>>& parameterForOptions(){return getOceanodeParameter(options);}
    ofParameter<int> value;
    ofParameter<vector<string>> options;
    ofParameter<string> reference;
    vector<string> currentOptions, candidates;
    string status;
    uint64_t observedRevision = 0;
    ofParameter<int> dummyMin{ofParameter<int>("Min", 0, 0, 0)};
    ofParameter<int> dummyMax{ofParameter<int>("Max", 0, 0, 0)};
    ofEventListener optionsListener, referenceListener;
};

#endif /* router_h */
