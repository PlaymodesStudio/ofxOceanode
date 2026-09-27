//
//  ofxOceanodeMidiClock.h
//  ofxOceanode
//
//  MIDI clock receiver: opens a MIDI input port, feeds ofxOceanodeMidiClockCore
//  and (optionally) drives the global transport as its external clock.
//  Used by the built-in "Sync: MIDI Clock" setting and by the MIDI Clock node.
//  Only compiled in OFXOCEANODE_USE_MIDI builds (requires ofxMidi).
//

#pragma once

#ifdef OFXOCEANODE_USE_MIDI

#include "ofxOceanodeMidiClockCore.h"
#include "ofxOceanodeTransport.h"
#include "ofxMidi.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class ofxOceanodeMidiClock : public ofxMidiListener {
public:
    ofxOceanodeMidiClock();
    ~ofxOceanodeMidiClock() override;

    static std::vector<std::string> availablePorts();

    // Opens (and keeps retrying, e.g. after unplugging) the named port. "" closes.
    bool open(const std::string& portName);
    // Closes the port but remembers its name.
    void close();
    // Sets the port name without opening it.
    void rememberPort(const std::string& name) { portName = name; }
    bool isOpen() const;
    const std::string& getPortName() const { return portName; }

    // Drive (or stop driving) the transport. Claiming takes over from any other
    // source; releasing only releases if this receiver still owns it.
    void setDriveTransport(std::shared_ptr<ofxOceanodeTransport> transport, bool drive);
    bool isDrivingTransport() const;

    ofxOceanodeMidiClockCore& core() { return clock; }
    const ofxOceanodeMidiClockCore& core() const { return clock; }
    ofxOceanodeMidiClockStatus status() const { return clock.status(ofxOceanodeTransport::steadyNowUs()); }

    // Main thread, once per frame (clock-only auto-stop, port hot-plug re-open).
    void update();

    void newMidiMessage(ofxMidiMessage& message) override;

private:
    ofxMidiIn midiIn;
    std::string portName;        // requested port (kept while unplugged)
    ofxOceanodeMidiClockCore clock;
    bool portOpen = false;
    bool wantOpen = false;       // keep retrying portName until it opens
    mutable std::mutex driveMutex; // drivenTransport is read on the MIDI thread
    std::shared_ptr<ofxOceanodeTransport> drivenTransport;
    bool driving = false;
    uint64_t lastPortScanUs = 0;
};

#endif // OFXOCEANODE_USE_MIDI
