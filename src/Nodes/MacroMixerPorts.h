#pragma once
#include "ofxOceanodeNodeModel.h"
#include "MacroMixerRuntime.h"

// Publishes a MacroMixerContract's routers as parameters of a node and moves
// their values to and from a MacroMixerRuntime: Input routers are the node's
// inputs, Output routers its outputs. The ports go after the node's other
// parameters, in contract order. Several nodes may each hold one of these for
// the same runtime.
class MacroMixerPorts {
public:
    using Values = std::map<std::string, MacroMixerMixing::Value>; // By router ID.
    using Parameters = std::map<std::string, std::shared_ptr<ofxOceanodeAbstractParameter>>; // By router ID.
    struct Port { std::string id, name, type, direction; };

    // separator: label of a separator kept just before the ports while any
    // install is in place; empty: none.
    explicit MacroMixerPorts(ofxOceanodeNodeModel& node, std::string separator = "");
    MacroMixerPorts(const MacroMixerPorts&) = delete;
    MacroMixerPorts& operator=(const MacroMixerPorts&) = delete;

    // Called when a Trigger Input port fires (forward to runtime.triggerInput).
    std::function<void(const std::string& routerID)> onTriggerInput;

    // Makes the node's ports match the contract. A router whose ID, type and
    // direction are unchanged keeps its parameter (and cables), renamed if
    // needed; the others are removed with their cables, timeline and Custom GUI
    // bindings. inputs: initial values of new Input ports (the runtime's
    // inputs()); without one they start neutral. Types must exist in the
    // mixing registry (a contract accepted by a runtime does).
    void install(const MacroMixerContract& contract, const Values* inputs = nullptr);
    void install(const MacroMixerRuntime& runtime) { install(runtime.contract(), &runtime.inputs()); }
    // Removes the ports the predicate picks, as install removes them. Does not
    // notify parameterGroupChanged: follow with install or clear.
    void removeIf(const std::function<bool(const std::string& id, ofxOceanodeAbstractParameter& parameter)>& remove);
    // Removes every port and the separator.
    void clear();

    // Installed ports in contract order, and their parameters.
    const std::vector<Port>& ports() const { return list; }
    const Parameters& parameters() const { return installed; }
    ofxOceanodeAbstractParameter* parameter(const std::string& id) const;
    // Whether a parameter can stay as this router's port (type and direction).
    static bool matches(ofxOceanodeAbstractParameter& parameter, const MacroMixerContract::Router& router);

    // Writes the Input ports into the runtime. onlyChanged: only ports whose
    // value changed since this object's last sync are written, over the
    // runtime's current inputs, so the last change of any node wins; the first
    // sync after a port is installed (or forgetInputs) writes it. Values of
    // registered (non built-in) types cannot be compared: such a port is
    // written while it has an incoming cable and once after losing it; Trigger
    // inputs carry no value. onlyChanged false writes every Input port, which
    // must then cover the runtime's inputs (the MacroMixer node). False when
    // the runtime refused the values (see its diagnostics()).
    bool syncInputs(MacroMixerRuntime& runtime, bool onlyChanged = true);
    void forgetInputs();

    // Writes the Output ports. From a runtime: each is read with takeOutput, so
    // Trigger events are consumed here. From values: an owner feeding several
    // nodes reads once (takeOutputs) and hands the same map to each. Ports
    // without a value of their type are left alone.
    void publish(MacroMixerRuntime& runtime);
    void publish(const Values& outputs);
    // Every Output router of the runtime's contract, read once with takeOutput.
    static Values takeOutputs(MacroMixerRuntime& runtime);

    // The port list alone (IDs, names, types, directions, in order), so a node
    // can re-create its ports in loadBeforeConnections, before the contract is
    // known, and let its cables reconnect. restorePorts installs it (neutral
    // inputs, types missing in this host skipped); false on malformed JSON.
    ofJson savePorts() const;
    bool restorePorts(const ofJson& saved);

private:
    std::string separatorName() const;
    void write(const Port& port, const MacroMixerMixing::Value& value);
    ofxOceanodeNodeModel& node;
    std::string separator;
    Parameters installed;
    std::vector<Port> list;
    std::map<std::string, ofEventListener> triggerInputs; // Trigger Input routers, by router ID.
    Values lastInputs;                 // What syncInputs last wrote from each port.
    std::set<std::string> cabledInputs; // Uncomparable inputs that had a cable at the last sync.
};
