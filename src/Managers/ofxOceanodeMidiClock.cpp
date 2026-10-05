//
//  ofxOceanodeMidiClock.cpp
//  ofxOceanode
//

#include "ofxOceanodeMidiClock.h"

#ifdef OFXOCEANODE_USE_MIDI

#include <algorithm>

ofxOceanodeMidiClock::ofxOceanodeMidiClock() {
    clock.onEvent = [this](const ofxOceanodeMidiClockEvent& e) {
        // Runs on the thread that delivered the MIDI event.
        std::lock_guard<std::mutex> lock(driveMutex);
        if(!driving || drivenTransport == nullptr) return;
        ofxOceanodeTransport::ExternalClockUpdate u;
        u.beat = e.beat;
        u.steadyTimeUs = e.steadyTimeUs;
        u.bpm = e.bpm;
        u.playing = e.playing;
        u.jump = e.jump;
        u.maxLeadBeats = e.maxLeadBeats;
        drivenTransport->updateExternalClock(this, u);
    };
}

ofxOceanodeMidiClock::~ofxOceanodeMidiClock() {
    setDriveTransport(nullptr, false);
    close();
}

std::vector<std::string> ofxOceanodeMidiClock::availablePorts() {
    // Listing ports creates a MIDI client; share one scan between every
    // receiver and node for up to a second (main thread only).
    static std::vector<std::string> cached;
    static uint64_t cachedAtUs = 0;
    const uint64_t now = ofxOceanodeTransport::steadyNowUs();
    if(cachedAtUs == 0 || now - cachedAtUs > 1000000) {
        ofxMidiIn probe;
        cached = probe.getInPortList();
        cachedAtUs = now;
    }
    return cached;
}

bool ofxOceanodeMidiClock::open(const std::string& name) {
    close();
    portName = name;
    wantOpen = !name.empty();
    if(name.empty()) return false;
    const auto ports = availablePorts();
    if(std::find(ports.begin(), ports.end(), name) == ports.end()) return false; // retried in update()
    if(!midiIn.openPort(name)) return false;
    portOpen = true;
    midiIn.ignoreTypes(true,   // sysex
                       false,  // timing: we need clock messages
                       true);  // active sensing
    midiIn.addListener(this);
    clock.reset();
    return true;
}

void ofxOceanodeMidiClock::close() {
    wantOpen = false;
    if(portOpen) {
        midiIn.removeListener(this);
        midiIn.closePort();
        portOpen = false;
    }
}

bool ofxOceanodeMidiClock::isOpen() const {
    return portOpen;
}

void ofxOceanodeMidiClock::setDriveTransport(std::shared_ptr<ofxOceanodeTransport> transport, bool drive) {
    std::lock_guard<std::mutex> lock(driveMutex);
    if(drivenTransport != nullptr && (!drive || transport != drivenTransport)) {
        drivenTransport->releaseExternalClock(this);
    }
    driving = drive && transport != nullptr;
    drivenTransport = driving ? transport : nullptr;
    if(driving) drivenTransport->claimExternalClock(this);
}

bool ofxOceanodeMidiClock::isDrivingTransport() const {
    std::lock_guard<std::mutex> lock(driveMutex);
    return driving && drivenTransport != nullptr && drivenTransport->getExternalClockOwner() == this;
}

void ofxOceanodeMidiClock::update() {
    const uint64_t now = ofxOceanodeTransport::steadyNowUs();
    clock.poll(now);
    if(wantOpen && !portName.empty() && now - lastPortScanUs > 1000000) {
        lastPortScanUs = now;
        if(!portOpen) {
            // Not present yet, or unplugged: try again (once a second).
            const std::string name = portName;
            open(name);
        } else {
            // The device went away while open: close it (keeping the wish to
            // reopen) so it is picked up again when it comes back.
            const auto ports = availablePorts();
            if(std::find(ports.begin(), ports.end(), portName) == ports.end()) {
                midiIn.removeListener(this);
                midiIn.closePort();
                portOpen = false;
            }
        }
    }
}

void ofxOceanodeMidiClock::newMidiMessage(ofxMidiMessage& message) {
    const uint64_t now = ofxOceanodeTransport::steadyNowUs();
    switch(message.status) {
        case MIDI_TIME_CLOCK: clock.tick(now); break;
        case MIDI_START:      clock.start(now); break;
        case MIDI_CONTINUE:   clock.resume(now); break;
        case MIDI_STOP:       clock.stop(now); break;
        case MIDI_SONG_POS_POINTER: {
            // ofxMidi keeps the status byte: bytes = [0xF2, LSB, MSB].
            const auto& b = message.bytes;
            if(b.size() >= 3) {
                clock.songPosition(ofxOceanodeMidiClockCore::decodeSongPosition(b[1], b[2]), now);
            } else if(b.size() == 2 && (b[0] & 0x80) == 0) {
                // Data bytes only (no status byte).
                clock.songPosition(ofxOceanodeMidiClockCore::decodeSongPosition(b[0], b[1]), now);
            }
            break;
        }
        default: break;
    }
}

#endif // OFXOCEANODE_USE_MIDI
