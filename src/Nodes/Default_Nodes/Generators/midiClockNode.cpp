//
//  midiClockNode.cpp
//  ofxOceanode
//

#include "midiClockNode.h"

#ifdef OFXOCEANODE_USE_MIDI

#include "ofxOceanodeMidiClock.h"

midiClockNode::midiClockNode() : ofxOceanodeNodeModel("MIDI Clock") {
    clock = std::make_unique<ofxOceanodeMidiClock>();
}

midiClockNode::~midiClockNode() {
    // ofxOceanodeMidiClock's destructor releases the transport (it holds its own
    // reference) and closes the port; the host container may already be gone here.
}

void midiClockNode::setup() {
    color = ofColor::red;
    description = "Reads an external MIDI clock (ticks, Start/Stop/Continue, Song Position) and outputs "
                  "its play state, triggers, beat position (stepped and smooth), 24 PPQ position and tempo. "
                  "Drive Transport makes it the clock of the global transport (like Sync: MIDI Clock).";

    portNames = {"None"};
    portSelectorParam = addParameterDropdown(portSelector, "Port", 0, portNames,
        ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
    addParameter(enable.set("Enable", true));
    addParameter(driveTransport.set("Drive Transport", false));
    addParameter(reaperMode.set("REAPER", true));
    addParameter(clockOnly.set("Clock Only", false));
    addParameter(stopOnLoss.set("Stop On Loss", true));
    addParameter(offsetMs.set("Offset ms", 0, -250, 250));
    addParameter(tempoWindow.set("Tempo Win", 24, 2, 96));
    addParameter(bpmDecimals.set("BPM Dec", 1, 0, 2));

    addOutputParameter(playingOut.set("Playing", false));
    addOutputParameter(startOut.set("Start"));
    addOutputParameter(stopOut.set("Stop"));
    addOutputParameter(continueOut.set("Continue"));
    addOutputParameter(jumpOut.set("Jump"));
    addOutputParameter(beatOut.set("Beat", 0, 0, FLT_MAX));
    addOutputParameter(smoothBeatOut.set("Beat Smooth", 0, 0, FLT_MAX));
    addOutputParameter(ppqOut.set("PPQ 24", 0, 0, INT_MAX));
    addOutputParameter(bpmOut.set("BPM", 120, 1, 999));
    addOutputParameter(receivingOut.set("Receiving", false));

    listeners.push(portSelector.newListener([this](int &index) {
        if(updatingSelector) return;
        selectedPort = (index > 0 && index < static_cast<int>(portNames.size())) ? portNames[index] : "";
        applyPort();
    }));
    listeners.push(enable.newListener([this](bool &) { applyPort(); applyDrive(); }));
    listeners.push(driveTransport.newListener([this](bool &) { applyDrive(); }));
    listeners.push(reaperMode.newListener([this](bool &b) { clock->core().setReaperMode(b); }));
    listeners.push(clockOnly.newListener([this](bool &b) { clock->core().setClockOnly(b); }));
    listeners.push(stopOnLoss.newListener([this](bool &b) { clock->core().setStopOnClockLoss(b); }));
    listeners.push(offsetMs.newListener([this](float &ms) { clock->core().setOffsetMs(ms); }));
    listeners.push(tempoWindow.newListener([this](int &w) { clock->core().setTempoWindow(w); }));

    refreshPorts(true);
}

void midiClockNode::refreshPorts(bool force) {
    if(!force && --framesToPortScan > 0) return;
    framesToPortScan = 60; // about once a second
    std::vector<std::string> names = {"None"};
    for(const auto &p : ofxOceanodeMidiClock::availablePorts()) names.push_back(p);
    if(names != portNames) {
        portNames = names;
        portSelector.setMax(static_cast<int>(portNames.size()) - 1);
        if(portSelectorParam) portSelectorParam->setDropdownOptions(portNames);
    }
    int index = 0;
    for(size_t i = 1; i < portNames.size(); ++i) if(portNames[i] == selectedPort) index = static_cast<int>(i);
    if(portSelector.get() != index) {
        updatingSelector = true;
        portSelector = index;
        updatingSelector = false;
    }
}

void midiClockNode::applyPort() {
    if(enable && !selectedPort.empty()) clock->open(selectedPort);
    else clock->close();
}

void midiClockNode::applyDrive() {
    clock->setDriveTransport(getTransport(), enable && driveTransport);
}

void midiClockNode::update(ofEventArgs &a) {
    refreshPorts(false);
    clock->update();
    const auto s = clock->status();

    if(playingOut.get() != s.playing) playingOut = s.playing;
    if(receivingOut.get() != s.receiving) receivingOut = s.receiving;
    if(ppqOut.get() != static_cast<int>(s.tickPosition)) ppqOut = static_cast<int>(s.tickPosition);
    beatOut = static_cast<float>(s.beat);
    smoothBeatOut = static_cast<float>(s.smoothBeat);
    const float scale = bpmDecimals.get() <= 0 ? 1.0f : (bpmDecimals.get() == 1 ? 10.0f : 100.0f);
    bpmOut = ofClamp(std::round(s.bpm * scale) / scale, 1.0f, 999.0f);

    if(s.startCount != lastStart) { lastStart = s.startCount; startOut.trigger(); }
    if(s.stopCount != lastStop) { lastStop = s.stopCount; stopOut.trigger(); }
    if(s.continueCount != lastContinue) { lastContinue = s.continueCount; continueOut.trigger(); }
    if(s.jumpCount != lastJump) { lastJump = s.jumpCount; jumpOut.trigger(); }
}

void midiClockNode::presetSave(ofJson &json) {
    json["MidiClockPort"] = selectedPort;
}

void midiClockNode::presetRecallBeforeSettingParameters(ofJson &json) {
    if(json.contains("MidiClockPort") && json["MidiClockPort"].is_string()) {
        selectedPort = json["MidiClockPort"].get<std::string>();
        refreshPorts(true);
        applyPort();
    }
}

#endif // OFXOCEANODE_USE_MIDI
