#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/Widgets/ofxOceanodeCustomGuiSignalWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "Managers/ofxOceanodeContainer.h"
#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>

#if __has_include("scNode.h") && __has_include("ofxSCBus.h") && __has_include("ofxSCSynth.h") && __has_include("ofxSCServer.h") && __has_include("ofxSuperCollider.h")
#define OFXOCEANODE_CUSTOMGUI_HAS_SCBUS 1
#include "scNode.h"
#include "ofxSCBus.h"
#include "ofxSCSynth.h"
#include "ofxSCServer.h"
#include "ofxSuperCollider.h"
#endif

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
constexpr int kScBusWaveformSamplesPerFrame = 64;
constexpr int kScBusWaveformMaxChannels = 128;
constexpr int kScBusFftBins = 128;
constexpr float kScBusFftSampleRate = 44100.0f;
constexpr float kScBusFftFreqMin = 20.0f;
constexpr float kScBusFftFreqMax = 22050.0f;

bool isScBusParameterType(const std::string& type){
    return type == typeid(nodePort).name();
}

class CustomGuiScBusWaveformScope {
public:
    ~CustomGuiScBusWaveformScope(){ clear(); }

    bool sync(const nodePort& port, int channels, float timeWindow){
        ofxSCServer* targetServer = ofxSCServer::local();
        if(targetServer == nullptr || port.getNodeRef() == nullptr){
            clear();
            return false;
        }

        channels = ofClamp(channels, 1, kScBusWaveformMaxChannels);
        const int sourceBus = port.getBusIndex(targetServer);
        if(sourceBus < 0){
            clear();
            return false;
        }

        ensureServerListeners(targetServer);
        if(needsServerRecreate || server != targetServer || inputBus != sourceBus || numChannels != channels){
            recreate(targetServer, sourceBus, channels);
        }

        bool receivedFrame = updateFrame();
        requestedTimeWindow = std::max(0.001f, timeWindow);
        handleStaleData(receivedFrame, targetServer, sourceBus, channels);
        return synth != nullptr && !frameData.empty() && !slidingBuffer.empty();
    }

    const std::vector<float>& getSlidingBuffer() const { return slidingBuffer; }
    int getNumChannels() const { return numChannels; }
    int getMaxBufferSize() const { return maxBufferSize; }
    float getRequestedTimeWindow() const { return requestedTimeWindow; }

private:
    void ensureServerListeners(ofxSCServer* targetServer){
        if(listeningServer == targetServer) return;

        serverBootedListener.unsubscribe();
        serverInitializedListener.unsubscribe();
        listeningServer = targetServer;

        if(listeningServer != nullptr){
            serverBootedListener = listeningServer->serverBootedEvent.newListener([this](){
                invalidateForServerRestart();
            });
            serverInitializedListener = listeningServer->serverInitializedEvent.newListener([this](){
                invalidateForServerRestart();
            });
        }
    }

    void invalidateForServerRestart(){
        needsServerRecreate = true;
        staleFrameCount = 0;
        for(auto& bus : controlBuses){
            if(bus != nullptr && listeningServer != nullptr && bus->index >= 0 && bus->index < (int)listeningServer->controlBusses.size()){
                listeningServer->controlBusses[bus->index] = nullptr;
            }
        }
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        synth.reset();
        controlBuses.clear();
        frameData.clear();
        slidingBuffer.clear();
        maxBufferSize = 0;
    }

    void recreate(ofxSCServer* targetServer, int sourceBus, int channels){
        clear();
        server = targetServer;
        inputBus = sourceBus;
        numChannels = channels;
        lastRecreateAttemptMs = ofGetElapsedTimeMillis();
        needsServerRecreate = false;

        const int totalBuses = kScBusWaveformSamplesPerFrame * numChannels;
        controlBuses.reserve(totalBuses);
        for(int i = 0; i < totalBuses; i++){
            controlBuses.emplace_back(std::make_unique<ofxSCBus>(RATE_CONTROL, 1, server));
        }
        if(controlBuses.empty()){
            clear();
            return;
        }

        int lowestBusIndex = controlBuses.front()->index;
        for(const auto& bus : controlBuses){
            if(bus == nullptr){
                clear();
                return;
            }
            lowestBusIndex = std::min(lowestBusIndex, bus->index);
        }

        synth = std::make_unique<ofxSCSynth>("wavescope_realtime" + ofToString(numChannels), server);
        synth->set("in", inputBus);
        synth->set("out", lowestBusIndex);
        synth->set("refreshRate", 60.0f);
        synth->createAndRun(1, 1, true);

        maxBufferSize = (int)(sampleRate * maxBufferTime);
        frameData.assign(totalBuses, 0.0f);
        slidingBuffer.assign(maxBufferSize * numChannels, 0.0f);
    }

    bool updateFrame(){
        if(synth == nullptr || controlBuses.empty()) return false;

        for(const auto& bus : controlBuses){
            if(bus != nullptr) bus->requestValues();
        }

        int lowestBusIndex = controlBuses.front()->index;
        for(const auto& bus : controlBuses){
            if(bus != nullptr) lowestBusIndex = std::min(lowestBusIndex, bus->index);
        }

        std::fill(frameData.begin(), frameData.end(), 0.0f);
        bool receivedAnyValue = false;
        for(const auto& bus : controlBuses){
            if(bus == nullptr || bus->readValues.empty()) continue;
            receivedAnyValue = true;
            const int position = bus->index - lowestBusIndex;
            if(position >= 0 && position < (int)frameData.size()){
                frameData[position] = bus->readValues[0];
            }
        }

        if(maxBufferSize <= 0 || slidingBuffer.empty()) return receivedAnyValue;
        for(int ch = 0; ch < numChannels; ch++){
            const int channelOffset = ch * maxBufferSize;
            std::memmove(&slidingBuffer[channelOffset],
                         &slidingBuffer[channelOffset + kScBusWaveformSamplesPerFrame],
                         sizeof(float) * std::max(0, maxBufferSize - kScBusWaveformSamplesPerFrame));
            for(int i = 0; i < kScBusWaveformSamplesPerFrame; i++){
                const int frameIndex = ch * kScBusWaveformSamplesPerFrame + i;
                if(frameIndex >= (int)frameData.size()) break;
                slidingBuffer[channelOffset + maxBufferSize - kScBusWaveformSamplesPerFrame + i] = frameData[frameIndex];
            }
        }
        return receivedAnyValue;
    }

    void handleStaleData(bool receivedFrame, ofxSCServer* targetServer, int sourceBus, int channels){
        if(receivedFrame){
            staleFrameCount = 0;
            return;
        }

        staleFrameCount++;
        const uint64_t nowMs = ofGetElapsedTimeMillis();
        if(staleFrameCount >= kStaleFrameThreshold && nowMs - lastRecreateAttemptMs >= kRecreateRetryIntervalMs){
            recreate(targetServer, sourceBus, channels);
        }
    }

    void clear(){
        if(synth != nullptr){
            synth->free();
            synth.reset();
        }
        for(auto& bus : controlBuses){
            if(bus != nullptr) bus->free();
        }
        controlBuses.clear();
        frameData.clear();
        slidingBuffer.clear();
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        maxBufferSize = 0;
        staleFrameCount = 0;
        needsServerRecreate = false;
    }

    static constexpr int kStaleFrameThreshold = 8;
    static constexpr uint64_t kRecreateRetryIntervalMs = 300;

    ofxSCServer* server = nullptr;
    int inputBus = -1;
    int numChannels = 0;
    std::unique_ptr<ofxSCSynth> synth;
    std::vector<std::unique_ptr<ofxSCBus>> controlBuses;
    std::vector<float> frameData;
    std::vector<float> slidingBuffer;
    int maxBufferSize = 0;
    float sampleRate = 44100.0f;
    float maxBufferTime = 10.0f;
    float requestedTimeWindow = 0.02f;
    int staleFrameCount = 0;
    uint64_t lastRecreateAttemptMs = 0;
    bool needsServerRecreate = false;
    ofxSCServer* listeningServer = nullptr;
    ofEventListener serverBootedListener;
    ofEventListener serverInitializedListener;
};

std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusWaveformScope>>& getScBusWaveformScopes(){
    static std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusWaveformScope>> scopes;
    return scopes;
}

void cleanupScBusWaveform(const std::string& panelId, const std::string& parameterPath){
    getScBusWaveformScopes().erase(panelId + "::" + parameterPath);
}

class CustomGuiScBusVUMeterScope {
public:
    ~CustomGuiScBusVUMeterScope(){ clear(); }

    bool sync(const nodePort& port, int channels){
        ofxSCServer* targetServer = ofxSCServer::local();
        if(targetServer == nullptr || port.getNodeRef() == nullptr){
            clear();
            return false;
        }

        channels = ofClamp(channels, 1, kScBusWaveformMaxChannels);
        const int sourceBus = port.getBusIndex(targetServer);
        if(sourceBus < 0){
            clear();
            return false;
        }

        ensureServerListeners(targetServer);
        if(needsServerRecreate || server != targetServer || inputBus != sourceBus || numChannels != channels){
            recreate(targetServer, sourceBus, channels);
        }

        bool receivedLevels = update();
        handleStaleData(receivedLevels, targetServer, sourceBus, channels);
        return synth != nullptr && !levels.empty();
    }

    const std::vector<float>& getLevels() const { return levels; }

private:
    void ensureServerListeners(ofxSCServer* targetServer){
        if(listeningServer == targetServer) return;

        serverBootedListener.unsubscribe();
        serverInitializedListener.unsubscribe();
        listeningServer = targetServer;

        if(listeningServer != nullptr){
            serverBootedListener = listeningServer->serverBootedEvent.newListener([this](){
                invalidateForServerRestart();
            });
            serverInitializedListener = listeningServer->serverInitializedEvent.newListener([this](){
                invalidateForServerRestart();
            });
        }
    }

    void invalidateForServerRestart(){
        needsServerRecreate = true;
        staleFrameCount = 0;
        for(auto& bus : buses){
            if(bus != nullptr && listeningServer != nullptr && bus->index >= 0 && bus->index < (int)listeningServer->controlBusses.size()){
                listeningServer->controlBusses[bus->index] = nullptr;
            }
        }
        if(outputBus != nullptr && listeningServer != nullptr && outputBus->index >= 0 && outputBus->index < (int)listeningServer->audioBusses.size()){
            listeningServer->audioBusses[outputBus->index] = nullptr;
        }
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        synth.reset();
        outputBus.reset();
        buses.clear();
        levels.clear();
    }

    void recreate(ofxSCServer* targetServer, int sourceBus, int channels){
        clear();
        server = targetServer;
        inputBus = sourceBus;
        numChannels = channels;
        lastRecreateAttemptMs = ofGetElapsedTimeMillis();
        needsServerRecreate = false;

        buses.reserve(numChannels);
        for(int i = 0; i < numChannels; i++){
            buses.emplace_back(std::make_unique<ofxSCBus>(RATE_CONTROL, 1, server));
        }
        if(buses.empty()){
            clear();
            return;
        }

        outputBus = std::make_unique<ofxSCBus>(RATE_AUDIO, numChannels, server);
        if(outputBus == nullptr){
            clear();
            return;
        }

        int lowestBusIndex = buses.front()->index;
        for(const auto& bus : buses){
            if(bus == nullptr){
                clear();
                return;
            }
            lowestBusIndex = std::min(lowestBusIndex, bus->index);
        }

        synth = std::make_unique<ofxSCSynth>("vumeter" + ofToString(numChannels), server);
        synth->set("in", inputBus);
        synth->set("out", outputBus->index);
        synth->set("vubus", lowestBusIndex);
        synth->set("vuattacktime", 10.0f);
        synth->set("vureleasetime", 300.0f);
        synth->createAndRun(1, 1, true);

        levels.assign(numChannels, 0.0f);
    }

    bool update(){
        if(synth == nullptr) return false;
        for(const auto& bus : buses){
            if(bus != nullptr) bus->requestValues();
        }

        int lowestBusIndex = buses.empty() ? 0 : buses.front()->index;
        for(const auto& bus : buses){
            if(bus != nullptr) lowestBusIndex = std::min(lowestBusIndex, bus->index);
        }

        std::fill(levels.begin(), levels.end(), 0.0f);
        bool receivedAnyValue = false;
        for(const auto& bus : buses){
            if(bus == nullptr || bus->readValues.empty()) continue;
            receivedAnyValue = true;
            const int position = bus->index - lowestBusIndex;
            if(position >= 0 && position < (int)levels.size()){
                levels[position] = bus->readValues[0];
            }
        }
        return receivedAnyValue;
    }

    void handleStaleData(bool receivedLevels, ofxSCServer* targetServer, int sourceBus, int channels){
        if(receivedLevels){
            staleFrameCount = 0;
            return;
        }

        staleFrameCount++;
        const uint64_t nowMs = ofGetElapsedTimeMillis();
        if(staleFrameCount >= kStaleFrameThreshold && nowMs - lastRecreateAttemptMs >= kRecreateRetryIntervalMs){
            recreate(targetServer, sourceBus, channels);
        }
    }

    void clear(){
        if(synth != nullptr){
            synth->free();
            synth.reset();
        }
        if(outputBus != nullptr){
            outputBus->free();
            outputBus.reset();
        }
        for(auto& bus : buses){
            if(bus != nullptr) bus->free();
        }
        buses.clear();
        levels.clear();
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        staleFrameCount = 0;
        needsServerRecreate = false;
    }

    static constexpr int kStaleFrameThreshold = 8;
    static constexpr uint64_t kRecreateRetryIntervalMs = 300;

    ofxSCServer* server = nullptr;
    int inputBus = -1;
    int numChannels = 0;
    std::unique_ptr<ofxSCSynth> synth;
    std::unique_ptr<ofxSCBus> outputBus;
    std::vector<std::unique_ptr<ofxSCBus>> buses;
    std::vector<float> levels;
    int staleFrameCount = 0;
    uint64_t lastRecreateAttemptMs = 0;
    bool needsServerRecreate = false;
    ofxSCServer* listeningServer = nullptr;
    ofEventListener serverBootedListener;
    ofEventListener serverInitializedListener;
};

std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusVUMeterScope>>& getScBusVUMeterScopes(){
    static std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusVUMeterScope>> scopes;
    return scopes;
}

void cleanupScBusVUMeter(const std::string& panelId, const std::string& parameterPath){
    getScBusVUMeterScopes().erase(panelId + "::" + parameterPath);
}

class CustomGuiScBusFftScope {
public:
    ~CustomGuiScBusFftScope(){ clear(); }

    bool sync(const nodePort& port, int channels, float smoothing){
        ofxSCServer* targetServer = ofxSCServer::local();
        if(targetServer == nullptr || port.getNodeRef() == nullptr){
            clear();
            return false;
        }

        channels = ofClamp(channels, 1, kScBusWaveformMaxChannels);
        const int sourceBus = port.getBusIndex(targetServer);
        if(sourceBus < 0){
            clear();
            return false;
        }

        ensureServerListeners(targetServer);
        if(needsServerRecreate || server != targetServer || inputBus != sourceBus || numChannels != channels){
            recreate(targetServer, sourceBus, channels);
        }

        bool receivedSpectrum = update(std::clamp(smoothing, 0.0f, 0.99f));
        handleStaleData(receivedSpectrum, targetServer, sourceBus, channels);
        return synth != nullptr && !displayMagnitudes.empty();
    }

    const std::vector<float>& getMagnitudes() const { return displayMagnitudes; }

private:
    void ensureServerListeners(ofxSCServer* targetServer){
        if(listeningServer == targetServer) return;

        serverBootedListener.unsubscribe();
        serverInitializedListener.unsubscribe();
        listeningServer = targetServer;

        if(listeningServer != nullptr){
            serverBootedListener = listeningServer->serverBootedEvent.newListener([this](){
                invalidateForServerRestart();
            });
            serverInitializedListener = listeningServer->serverInitializedEvent.newListener([this](){
                invalidateForServerRestart();
            });
        }
    }

    void invalidateForServerRestart(){
        needsServerRecreate = true;
        staleFrameCount = 0;
        if(fftBus != nullptr && listeningServer != nullptr && fftBus->index >= 0 && fftBus->index < (int)listeningServer->controlBusses.size()){
            listeningServer->controlBusses[fftBus->index] = nullptr;
        }
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        synth.reset();
        fftBus.reset();
        displayMagnitudes.clear();
    }

    void recreate(ofxSCServer* targetServer, int sourceBus, int channels){
        clear();
        server = targetServer;
        inputBus = sourceBus;
        numChannels = channels;
        lastRecreateAttemptMs = ofGetElapsedTimeMillis();
        needsServerRecreate = false;

        fftBus = std::make_unique<ofxSCBus>(RATE_CONTROL, kScBusFftBins, server);
        if(fftBus == nullptr || fftBus->index < 0){
            clear();
            return;
        }

        synth = std::make_unique<ofxSCSynth>("fftanalyzer" + ofToString(numChannels), server);
        synth->set("in", inputBus);
        synth->set("fftbus", fftBus->index);
        synth->createAndRun(1, 1, true);

        displayMagnitudes.assign(kScBusFftBins, 0.0f);
    }

    bool update(float smoothing){
        if(synth == nullptr || fftBus == nullptr) return false;
        fftBus->requestValues();
        const auto& raw = fftBus->readValues;
        if((int)raw.size() != kScBusFftBins) return false;

        for(int i = 0; i < kScBusFftBins; i++){
            displayMagnitudes[i] = displayMagnitudes[i] * smoothing + raw[i] * (1.0f - smoothing);
        }
        return true;
    }

    void handleStaleData(bool receivedSpectrum, ofxSCServer* targetServer, int sourceBus, int channels){
        if(receivedSpectrum){
            staleFrameCount = 0;
            return;
        }

        staleFrameCount++;
        const uint64_t nowMs = ofGetElapsedTimeMillis();
        if(staleFrameCount >= kStaleFrameThreshold && nowMs - lastRecreateAttemptMs >= kRecreateRetryIntervalMs){
            recreate(targetServer, sourceBus, channels);
        }
    }

    void clear(){
        if(synth != nullptr){
            synth->free();
            synth.reset();
        }
        if(fftBus != nullptr){
            fftBus->free();
            fftBus.reset();
        }
        displayMagnitudes.clear();
        server = nullptr;
        inputBus = -1;
        numChannels = 0;
        staleFrameCount = 0;
        needsServerRecreate = false;
    }

    static constexpr int kStaleFrameThreshold = 8;
    static constexpr uint64_t kRecreateRetryIntervalMs = 300;

    ofxSCServer* server = nullptr;
    int inputBus = -1;
    int numChannels = 0;
    std::unique_ptr<ofxSCSynth> synth;
    std::unique_ptr<ofxSCBus> fftBus;
    std::vector<float> displayMagnitudes;
    int staleFrameCount = 0;
    uint64_t lastRecreateAttemptMs = 0;
    bool needsServerRecreate = false;
    ofxSCServer* listeningServer = nullptr;
    ofEventListener serverBootedListener;
    ofEventListener serverInitializedListener;
};

std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusFftScope>>& getScBusFftScopes(){
    static std::unordered_map<std::string, std::unique_ptr<CustomGuiScBusFftScope>> scopes;
    return scopes;
}

void cleanupScBusFft(const std::string& panelId, const std::string& parameterPath){
    getScBusFftScopes().erase(panelId + "::" + parameterPath);
}
#else
bool isScBusParameterType(const std::string&){
    return false;
}

void cleanupScBusWaveform(const std::string&, const std::string&){}
void cleanupScBusVUMeter(const std::string&, const std::string&){}
void cleanupScBusFft(const std::string&, const std::string&){}
#endif

bool supportsWaveformWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) ||
           isFloatVectorParameter(parameter) ||
           isTextureParameter(parameter) ||
           isScBusParameterType(parameter.valueType());
}

bool supportsVUMeterWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) ||
           isFloatVectorParameter(parameter) ||
           isScBusParameterType(parameter.valueType());
}

bool supportsFFTWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatVectorParameter(parameter) ||
           isScBusParameterType(parameter.valueType());
}

bool supportsTextureWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isTextureParameter(parameter);
}

void initializeTextureWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 3;
    widget.spanH = 2;
}

void initializeWaveformWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    if(isTextureParameter(parameter) || isScBusParameterType(parameter.valueType())){
        widget.spanW = 3;
        widget.spanH = 2;
    }
    if(isScBusParameterType(parameter.valueType())){
        widget.config["channels"] = 1;
        widget.config["timeWindow"] = 0.02f;
        widget.config["gain"] = 1.0f;
        widget.color = ofColor::white;
    }
}

void initializeVUMeterWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    widget.spanW = 3;
    widget.spanH = 2;
    if(isScBusParameterType(parameter.valueType())){
        widget.config["channels"] = 2;
    }
}

void initializeFFTWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    widget.spanW = 3;
    widget.spanH = 2;
    widget.config["logScale"] = true;
    widget.config["dbScale"] = true;
    widget.config["dbFloor"] = -80.0f;
    widget.config["lineMode"] = false;
    widget.config["smoothing"] = 0.7f;
    if(isScBusParameterType(parameter.valueType())){
        widget.config["channels"] = 1;
    }
}

float ampToDb(float amp)
{
    if(amp <= 0.0f) return -60.0f;
    return 20.0f * std::log10(amp);
}

float dbToVUPosition(float db, float minDb = -60.0f, float maxDb = 6.0f)
{
    if(db <= minDb) return 0.0f;
    if(db >= maxDb) return 1.0f;
    return (db - minDb) / (maxDb - minDb);
}

ImU32 getVUMeterColorDB(float dbLevel)
{
    if(dbLevel < -18.0f) {
        float greenFactor = (dbLevel + 60.0f) / 42.0f;
        greenFactor = ofClamp(greenFactor, 0.0f, 1.0f);
        int green = (int)(50 + (200 * greenFactor));
        return IM_COL32(0, green, 0, 255);
    } else if(dbLevel < -6.0f) {
        float yellowFactor = (dbLevel + 18.0f) / 12.0f;
        yellowFactor = ofClamp(yellowFactor, 0.0f, 1.0f);
        int red = (int)(200 * yellowFactor);
        int green = 250;
        return IM_COL32(red, green, 0, 255);
    } else if(dbLevel < 0.0f) {
        float orangeFactor = (dbLevel + 6.0f) / 6.0f;
        orangeFactor = ofClamp(orangeFactor, 0.0f, 1.0f);
        int red = (int)(200 + (55 * orangeFactor));
        int green = (int)(250 - (100 * orangeFactor));
        return IM_COL32(red, green, 0, 255);
    } else {
        float redFactor = ofClamp(dbLevel / 6.0f, 0.0f, 1.0f);
        int green = (int)(150 - (150 * redFactor));
        return IM_COL32(255, green, 0, 255);
    }
}

void drawVUMeterDisplay(const std::vector<float>& levels, const ImVec2& itemSize, ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
{
    const int numChans = std::max(1, (int)levels.size());
    const float leftMargin = std::min(20.0f, itemSize.x * 0.12f);
    const float meterWidth = std::max(1.0f, itemSize.x - leftMargin);
    const float separatorHeight = 1.0f;
    const float totalSeparatorHeight = std::max(0, numChans - 1) * separatorHeight;
    const float channelH = std::max(8.0f, (itemSize.y - totalSeparatorHeight) / numChans);

    drawList->AddRectFilled(min, max, IM_COL32(15, 15, 15, 255), 2.0f);
    drawList->AddRect(min, max, IM_COL32(100, 100, 100, 255), 2.0f);

    float currentY = min.y;
    for(int ch = 0; ch < numChans; ch++){
        const float dbLevel = ampToDb(ofClamp(levels[ch], 0.0f, 2.0f));
        const ImVec2 channelStart(min.x + leftMargin, currentY);
        const ImVec2 channelEnd(max.x, currentY + channelH);
        drawList->AddRectFilled(channelStart, channelEnd, IM_COL32(25, 25, 25, 255));

        if(dbLevel > -60.0f){
            const float meterPosition = dbToVUPosition(dbLevel, -60.0f, 6.0f);
            const float meterW = meterWidth * meterPosition;
            drawList->AddRectFilled(channelStart, ImVec2(channelStart.x + meterW, channelEnd.y), getVUMeterColorDB(dbLevel));
        }

        const float zeroDbPosition = dbToVUPosition(0.0f, -60.0f, 6.0f);
        if(zeroDbPosition > 0.01f && zeroDbPosition < 0.99f){
            const float zeroDbX = channelStart.x + meterWidth * zeroDbPosition;
            drawList->AddLine(ImVec2(zeroDbX, channelStart.y), ImVec2(zeroDbX, channelEnd.y), IM_COL32(255, 255, 255, 220), 1.0f);
        }

        const char* channelLabel = ch == 0 ? "L" : (ch == 1 ? "R" : nullptr);
        const std::string label = channelLabel != nullptr ? channelLabel : ofToString(ch + 1);
        drawList->AddText(ImVec2(min.x + 3.0f, currentY + 2.0f), IM_COL32(180, 180, 180, 255), label.c_str());

        currentY += channelH;
        if(ch < numChans - 1) currentY += separatorHeight;
    }
}

void drawSpectrumDisplay(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const std::vector<float>& magnitudes,
                         bool useLog, bool useDb, float floorVal, bool lineMode)
{
    const float labelH = 14.0f;
    const float specH = std::max(1.0f, (max.y - min.y) - labelH);
    const float xS = min.x;
    const float yS = min.y;
    const float xE = max.x;
    const float yE = yS + specH;
    const float yLabelTop = yE + 1.0f;
    const float width = std::max(1.0f, xE - xS);
    const float nyquist = kScBusFftSampleRate * 0.5f;
    const float logRatio = std::log(kScBusFftFreqMax / kScBusFftFreqMin);

    drawList->PushClipRect(ImVec2(xS, yS), ImVec2(xE, max.y), true);
    drawList->AddRectFilled(ImVec2(xS, yS), ImVec2(xE, yE), IM_COL32(10, 12, 18, 255));
    drawList->AddRectFilled(ImVec2(xS, yE), ImVec2(xE, max.y), IM_COL32(8, 10, 16, 255));
    drawList->AddRect(ImVec2(xS, yS), ImVec2(xE, max.y), IM_COL32(60, 60, 80, 255));

    static const float gridFreqs[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    static const char* gridLabels[] = {"50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"};
    for(int g = 0; g < 9; g++){
        if(gridFreqs[g] > kScBusFftFreqMax) break;
        float gx = useLog
            ? xS + width * (std::log(gridFreqs[g] / kScBusFftFreqMin) / logRatio)
            : xS + width * (gridFreqs[g] / nyquist);
        drawList->AddLine(ImVec2(gx, yS), ImVec2(gx, yE), IM_COL32(35, 35, 50, 200));
        drawList->AddText(ImVec2(gx + 2.0f, yLabelTop), IM_COL32(90, 90, 110, 210), gridLabels[g]);
    }

    auto bandToX = [&](float b){
        const float t = magnitudes.empty() ? 0.0f : b / (float)magnitudes.size();
        if(useLog) return xS + width * t;
        const float fc = kScBusFftFreqMin * std::pow(kScBusFftFreqMax / kScBusFftFreqMin, t);
        return xS + width * (fc / nyquist);
    };

    auto magToY = [&](float mag){
        if(useDb){
            const float db = 20.0f * std::log10(std::max(mag, 1e-12f));
            return yE - specH * ofClamp((db - floorVal) / (-floorVal), 0.0f, 1.0f);
        }
        return yE - specH * ofClamp(mag, 0.0f, 1.0f);
    };

    auto bandColor = [&](int b, int alpha){
        const float t = magnitudes.size() <= 1 ? 0.0f : (float)b / (float)(magnitudes.size() - 1);
        return IM_COL32((int)(40 + 200 * t), (int)(200 - 160 * t), (int)(255 - 220 * t), alpha);
    };

    if(lineMode){
        for(int b = 0; b < (int)magnitudes.size() - 1; b++){
            float x1 = bandToX(b + 0.5f), x2 = bandToX(b + 1.5f);
            float y1 = magToY(magnitudes[b]), y2 = magToY(magnitudes[b + 1]);
            drawList->AddQuadFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x2, yE), ImVec2(x1, yE), IM_COL32(60, 180, 220, 40));
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), bandColor(b, 230), 1.5f);
        }
    }else{
        for(int b = 0; b < (int)magnitudes.size(); b++){
            const float t1 = (float)b / (float)magnitudes.size();
            const float t2 = (float)(b + 1) / (float)magnitudes.size();
            float x1, x2;
            if(useLog){
                x1 = xS + width * t1;
                x2 = xS + width * t2;
            }else{
                const float fc1 = kScBusFftFreqMin * std::pow(kScBusFftFreqMax / kScBusFftFreqMin, t1);
                const float fc2 = kScBusFftFreqMin * std::pow(kScBusFftFreqMax / kScBusFftFreqMin, t2);
                x1 = xS + width * (fc1 / nyquist);
                x2 = xS + width * (fc2 / nyquist);
            }
            if(x2 <= x1 + 0.3f) continue;
            const float barTop = magToY(magnitudes[b]);
            drawList->AddRectFilled(ImVec2(x1, barTop), ImVec2(x2 - 0.5f, yE), bandColor(b, 210));
        }
    }

    drawList->PopClipRect();
}

bool renderTextureLikeWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofTexture* texture, const char* unavailableLabel)
{
    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    const ImVec2 itemSize = widgetItemSize(context);
    ImGui::InvisibleButton("##texture", itemSize);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    if(texture != nullptr && texture->isAllocated()){
        ImTextureID textureID = (ImTextureID)(uintptr_t)texture->texData.textureID;
        drawList->AddImage(textureID, min, max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, widget.color.a));
    }else{
        drawList->AddRect(min, max, IM_COL32(160, 160, 160, 180), 2.0f);
        drawList->AddText(ImVec2(min.x + 6.0f, min.y + 6.0f), IM_COL32(200, 200, 200, 220), unavailableLabel);
    }
    ImGui::EndGroup();
    return true;
}

bool renderWaveformWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    if(isFloatParameter(*parameter)){
        auto& param = parameter->cast<float>().getParameter();
        const float value = param.get();
        const float min = floatRangeMin(widget, param.getMin());
        const float max = floatRangeMax(widget, param.getMax());
        const float fraction = max != min ? (value - min) / (max - min) : 0.0f;
        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), widgetItemSize(context), context.showValue ? ofToString(value, 3).c_str() : "");
        ImGui::EndGroup();
        return true;
    }

    if(isTextureParameter(*parameter)){
        return renderTextureLikeWidget(context, widget, parameter->cast<ofTexture*>().getParameter().get(), "Texture unavailable");
    }

    if(isFloatVectorParameter(*parameter)){
        auto value = parameter->cast<std::vector<float>>().getParameter().get();
        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        if(!value.empty()) ImGui::PlotLines("##value", value.data(), (int)value.size(), 0, nullptr, FLT_MAX, FLT_MAX, widgetItemSize(context));
        ImGui::EndGroup();
        return true;
    }

    if(isScBusParameterType(parameter->valueType())){
#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
        auto port = parameter->cast<nodePort>().getParameter().get();
        const int channels = ofClamp(widget.config.value("channels", 1), 1, kScBusWaveformMaxChannels);
        const float timeWindow = std::max(0.001f, widget.config.value("timeWindow", 0.02f));
        const float gain = std::max(0.01f, widget.config.value("gain", 1.0f));
        const std::string scopeKey = context.panelId + "::" + widget.parameterRef.parameterPath;
        auto& scopes = getScBusWaveformScopes();
        auto& scope = scopes[scopeKey];
        if(scope == nullptr) scope = std::make_unique<CustomGuiScBusWaveformScope>();

        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        const ImVec2 itemSize = widgetItemSize(context);
        ImGui::InvisibleButton("##scwaveform", itemSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        drawList->AddRectFilled(min, max, IM_COL32(12, 12, 16, 255), 2.0f);
        drawList->AddRect(min, max, IM_COL32(70, 70, 80, 255), 2.0f);

        const bool ready = scope->sync(port, channels, timeWindow);
        if(ready){
            const auto& slidingBuffer = scope->getSlidingBuffer();
            const int actualChannels = std::max(1, scope->getNumChannels());
            const int maxBufferSize = std::max(1, scope->getMaxBufferSize());
            int samplesToDisplay = std::max(1, (int)std::round(scope->getRequestedTimeWindow() * 44100.0f));
            samplesToDisplay = std::min(samplesToDisplay, maxBufferSize);
            const int startSample = maxBufferSize - samplesToDisplay;
            const float channelHeight = itemSize.y / (float)actualChannels;
            for(int ch = 0; ch < actualChannels; ch++){
                const float y0 = min.y + ch * channelHeight;
                const float centerY = y0 + channelHeight * 0.5f;
                if(ch > 0) drawList->AddLine(ImVec2(min.x, y0), ImVec2(max.x, y0), IM_COL32(50, 50, 58, 255));
                drawList->AddLine(ImVec2(min.x, centerY), ImVec2(max.x, centerY), IM_COL32(40, 40, 46, 220));

                for(int i = 0; i < itemSize.x - 1.0f; i++){
                    const float progress1 = itemSize.x <= 1.0f ? 0.0f : (float)i / std::max(1.0f, itemSize.x - 1.0f);
                    const float progress2 = itemSize.x <= 1.0f ? 0.0f : (float)(i + 1) / std::max(1.0f, itemSize.x - 1.0f);
                    const int sampleIndex1 = startSample + (int)std::round(progress1 * (samplesToDisplay - 1));
                    const int sampleIndex2 = startSample + (int)std::round(progress2 * (samplesToDisplay - 1));
                    const int index1 = ch * maxBufferSize + ofClamp(sampleIndex1, startSample, maxBufferSize - 1);
                    const int index2 = ch * maxBufferSize + ofClamp(sampleIndex2, startSample, maxBufferSize - 1);
                    if(index1 < 0 || index2 < 0 || index1 >= (int)slidingBuffer.size() || index2 >= (int)slidingBuffer.size()) break;
                    const float x1 = min.x + (float)i;
                    const float x2 = min.x + (float)(i + 1);
                    const float s1 = ofClamp(slidingBuffer[index1] * gain, -1.0f, 1.0f);
                    const float s2 = ofClamp(slidingBuffer[index2] * gain, -1.0f, 1.0f);
                    const float py1 = centerY - s1 * channelHeight * 0.45f;
                    const float py2 = centerY - s2 * channelHeight * 0.45f;
                    drawList->AddLine(ImVec2(x1, py1), ImVec2(x2, py2), IM_COL32(widget.color.r, widget.color.g, widget.color.b, 235), 1.5f);
                }
            }
        }else{
            drawList->AddText(ImVec2(min.x + 6.0f, min.y + 6.0f), IM_COL32(180, 180, 180, 220), "SC bus unavailable");
        }
        ImGui::EndGroup();
        return true;
#else
        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        ImGui::TextDisabled("SC bus support unavailable");
        ImGui::EndGroup();
        return true;
#endif
    }

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    ImGui::TextDisabled("Unsupported type");
    ImGui::EndGroup();
    return true;
}

bool renderTextureWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;
    return renderTextureLikeWidget(context, widget, parameter->cast<ofTexture*>().getParameter().get(), "Texture unavailable");
}

bool renderVUMeterWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    std::vector<float> levels;
    if(isFloatParameter(*parameter)){
        levels = { parameter->cast<float>().getParameter().get() };
    }else if(isFloatVectorParameter(*parameter)){
        levels = parameter->cast<std::vector<float>>().getParameter().get();
    }else if(isScBusParameterType(parameter->valueType())){
#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
        auto port = parameter->cast<nodePort>().getParameter().get();
        const int channels = ofClamp(widget.config.value("channels", 2), 1, kScBusWaveformMaxChannels);
        const std::string scopeKey = context.panelId + "::" + widget.parameterRef.parameterPath;
        auto& scopes = getScBusVUMeterScopes();
        auto& scope = scopes[scopeKey];
        if(scope == nullptr) scope = std::make_unique<CustomGuiScBusVUMeterScope>();
        if(!scope->sync(port, channels)){
            ImGui::BeginGroup();
            drawWidgetLabel(widget, context.label);
            ImGui::TextDisabled("SC bus unavailable");
            ImGui::EndGroup();
            return true;
        }
        levels = scope->getLevels();
#else
        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        ImGui::TextDisabled("SC bus support unavailable");
        ImGui::EndGroup();
        return true;
#endif
    }else{
        return false;
    }

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    const ImVec2 itemSize = widgetItemSize(context);
    ImGui::InvisibleButton("##vumeter", itemSize);
    drawVUMeterDisplay(levels, itemSize, ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGui::EndGroup();
    return true;
}

bool renderFFTWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    std::vector<float> magnitudes;
    if(isFloatVectorParameter(*parameter)){
        magnitudes = parameter->cast<std::vector<float>>().getParameter().get();
    }else if(isScBusParameterType(parameter->valueType())){
#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
        auto port = parameter->cast<nodePort>().getParameter().get();
        const int channels = ofClamp(widget.config.value("channels", 1), 1, kScBusWaveformMaxChannels);
        const float smoothing = ofClamp(widget.config.value("smoothing", 0.7f), 0.0f, 0.99f);
        const std::string scopeKey = context.panelId + "::" + widget.parameterRef.parameterPath;
        auto& scopes = getScBusFftScopes();
        auto& scope = scopes[scopeKey];
        if(scope == nullptr) scope = std::make_unique<CustomGuiScBusFftScope>();
        if(!scope->sync(port, channels, smoothing)){
            ImGui::BeginGroup();
            drawWidgetLabel(widget, context.label);
            ImGui::TextDisabled("SC bus unavailable");
            ImGui::EndGroup();
            return true;
        }
        magnitudes = scope->getMagnitudes();
#else
        ImGui::BeginGroup();
        drawWidgetLabel(widget, context.label);
        ImGui::TextDisabled("SC bus support unavailable");
        ImGui::EndGroup();
        return true;
#endif
    }else{
        return false;
    }

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    const ImVec2 itemSize = widgetItemSize(context);
    ImGui::InvisibleButton("##fft", itemSize);
    drawSpectrumDisplay(ImGui::GetWindowDrawList(),
                        ImGui::GetItemRectMin(),
                        ImGui::GetItemRectMax(),
                        magnitudes,
                        widget.config.value("logScale", true),
                        widget.config.value("dbScale", true),
                        widget.config.value("dbFloor", -80.0f),
                        widget.config.value("lineMode", false));
    ImGui::EndGroup();
    return true;
}

void drawWaveformProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr || !isScBusParameterType(parameter->valueType())) return;

#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
    int channels = ofClamp(widget.config.value("channels", 1), 1, kScBusWaveformMaxChannels);
    if(ImGui::SliderInt("Channels", &channels, 1, kScBusWaveformMaxChannels)){
        widget.config["channels"] = channels;
        context.container.markCustomGuisDirty();
    }
#endif

    float timeWindow = std::max(0.001f, widget.config.value("timeWindow", 0.02f));
    if(ImGui::InputFloat("Time Window", &timeWindow, 0.001f, 0.01f, "%.3f")){
        widget.config["timeWindow"] = std::max(0.001f, timeWindow);
        context.container.markCustomGuisDirty();
    }

    float gain = std::max(0.01f, widget.config.value("gain", 1.0f));
    if(ImGui::InputFloat("Gain", &gain, 0.05f, 0.25f, "%.2f")){
        widget.config["gain"] = std::max(0.01f, gain);
        context.container.markCustomGuisDirty();
    }
}

void drawVUMeterProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr || !isScBusParameterType(parameter->valueType())) return;
#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
    int channels = ofClamp(widget.config.value("channels", 2), 1, kScBusWaveformMaxChannels);
    if(ImGui::SliderInt("Channels", &channels, 1, kScBusWaveformMaxChannels)){
        widget.config["channels"] = channels;
        context.container.markCustomGuisDirty();
    }
#endif
}

void drawFFTProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter != nullptr && isScBusParameterType(parameter->valueType())){
#ifdef OFXOCEANODE_CUSTOMGUI_HAS_SCBUS
        int channels = ofClamp(widget.config.value("channels", 1), 1, kScBusWaveformMaxChannels);
        if(ImGui::SliderInt("Channels", &channels, 1, kScBusWaveformMaxChannels)){
            widget.config["channels"] = channels;
            context.container.markCustomGuisDirty();
        }
#endif
    }

    bool logScale = widget.config.value("logScale", true);
    if(ImGui::Checkbox("Log X", &logScale)){
        widget.config["logScale"] = logScale;
        context.container.markCustomGuisDirty();
    }

    bool dbScale = widget.config.value("dbScale", true);
    if(ImGui::Checkbox("dB Y", &dbScale)){
        widget.config["dbScale"] = dbScale;
        context.container.markCustomGuisDirty();
    }

    float dbFloor = widget.config.value("dbFloor", -80.0f);
    if(ImGui::InputFloat("dB Floor", &dbFloor, 1.0f, 5.0f, "%.1f")){
        widget.config["dbFloor"] = ofClamp(dbFloor, -120.0f, -20.0f);
        context.container.markCustomGuisDirty();
    }

    float smoothing = widget.config.value("smoothing", 0.7f);
    if(ImGui::SliderFloat("Smooth", &smoothing, 0.0f, 0.99f)){
        widget.config["smoothing"] = smoothing;
        context.container.markCustomGuisDirty();
    }

    bool lineMode = widget.config.value("lineMode", false);
    if(ImGui::Checkbox("Line Mode", &lineMode)){
        widget.config["lineMode"] = lineMode;
        context.container.markCustomGuisDirty();
    }
}

} // namespace

namespace ofxOceanodeCustomGuiSignalWidgets {

void registerWidgets(ofxOceanodeCustomGuiWidgetRegistry& registry)
{
    registerWidget(registry, CustomGuiWidgetType::Waveform,
                   supportsWaveformWidget,
                   initializeWaveformWidget,
                   renderWaveformWidget,
                   drawWaveformProperties,
                   cleanupScBusWaveform);

    registerWidget(registry, CustomGuiWidgetType::VUMeter,
                   supportsVUMeterWidget,
                   initializeVUMeterWidget,
                   renderVUMeterWidget,
                   drawVUMeterProperties,
                   cleanupScBusVUMeter);

    registerWidget(registry, CustomGuiWidgetType::FFT,
                   supportsFFTWidget,
                   initializeFFTWidget,
                   renderFFTWidget,
                   drawFFTProperties,
                   cleanupScBusFft);

    registerWidget(registry, CustomGuiWidgetType::Texture,
                   supportsTextureWidget,
                   initializeTextureWidget,
                   renderTextureWidget);
}

} // namespace ofxOceanodeCustomGuiSignalWidgets

#endif
