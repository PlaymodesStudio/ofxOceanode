#pragma once

#include "ofxOceanodeWindowVisibility.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"

namespace oceanodeLayouts {
// Use model identity, never a user-editable node label or a retained pointer.
inline std::string nodeKey(ofxOceanodeNodeModel& model){
    return ofJson::array({model.nodeName(), model.getNumIdentifier()}).dump();
}
inline void windowToggles(ofParameterGroup& group, ofJson& state, bool restore){
    for(auto& parameter : group){
        if(!oceanodeWindowVisibility::isWindowToggle(parameter->getName())) continue;
        ofParameter<bool>* toggle = nullptr;
        if(auto* pin = dynamic_cast<ofxOceanodeParameter<bool>*>(parameter.get())) toggle = &pin->getParameter();
        else toggle = dynamic_cast<ofParameter<bool>*>(parameter.get());
        if(!toggle) continue;
        const auto& name = parameter->getName();
        if(!restore) state[name] = toggle->get();
        else if(state.contains(name) && state[name].is_boolean()) toggle->set(state[name].get<bool>());
    }
}
inline void windows(ofxOceanodeContainer* owner, ofJson& state, bool restore){
    if(!state.is_object()) return;
    for(auto* node : owner->getAllModules()){
        // The deprecated persistent-node feature stays outside preset views,
        // including windows inside a persistent Macro.
        if(node->getIsPersistent()) continue;
        auto& model = node->getNodeModel();
        const auto key = nodeKey(model);
        if(restore && (!state.contains("nodes") || !state["nodes"].is_object() ||
                       !state["nodes"].contains(key))) continue;
        auto& saved = state["nodes"][key];
        if(!restore) saved = ofJson::object();
        if(!saved.is_object()) continue;
        windowToggles(node->getParameters(), saved["parameters"], restore);
        windowToggles(node->getInspectorParameters(), saved["inspector"], restore);
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
            if(!restore){
                saved["open"] = macro->isWindowVisible();
                saved["children"] = ofJson::object();
            }else if(saved.contains("open") && saved["open"].is_boolean()){
                // Restoring visibility must not force a canvas into a new dock.
                macro->setWindowVisible(saved["open"].get<bool>(), false);
            }
            windows(macro->getContainer().get(), saved["children"], restore);
        }
    }
    if(auto* extension = owner->getCustomGuiExtension()) extension->windowState(state, restore);
}

inline ofJson captureWindows(ofxOceanodeContainer* container, ofxOceanodeControls& controls){
    ofJson state = {{"controllers", ofJson::object()}, {"windows", ofJson::object()}};
    const auto& visibility = controls.getControllersVisibility();
    for(const auto& controller : controls.getControllers()){
        if(controller->isMenuController()) continue;
        const auto& name = controller->getControllerName();
        const auto found = visibility.find(name);
        // Optional addon controllers can be drawn before the View menu has
        // initialized their entry. Match Controls::draw's visible default.
        state["controllers"][name] = found == visibility.end() || found->second;
    }
    windows(container, state["windows"], false);
    state["scopes"] = ofxOceanodeScope::getInstance()->isVisible();
    return state;
}
inline void restoreWindows(ofxOceanodeContainer* container, ofxOceanodeControls& controls, ofJson state){
    auto& visibility = controls.getControllersVisibility();
    for(const auto& controller : controls.getControllers()){
        const auto& key = controller->getControllerName();
        if(!controller->isMenuController() && state["controllers"].contains(key) && state["controllers"][key].is_boolean())
            visibility[key] = state["controllers"][key].get<bool>();
    }
    windows(container, state["windows"], true);
    if(state.contains("scopes") && state["scopes"].is_boolean())
        ofxOceanodeScope::getInstance()->setVisible(state["scopes"].get<bool>());
}
} // namespace oceanodeLayouts
