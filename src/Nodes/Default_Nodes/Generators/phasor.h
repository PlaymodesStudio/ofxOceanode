//
//  phasor.h
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#ifndef phasor_h
#define phasor_h

#include "ofxOceanodeNodeModel.h"
#include "basePhasor.h"

class timeGenerator : public ofxOceanodeNodeModel{
public:
    timeGenerator(string s) : ofxOceanodeNodeModel(s)
    {
        color = ofColor::red;
    }
    
    void setup() override{
        time = 0;
    }
    void    setTime(double t) {time = t;};
    double  getTime() {return time;};

private:
    double time;
};



class counter : public timeGenerator{
public:
    counter() : timeGenerator("Counter"){
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        description = "Counts elapsed time since Reset. With Sync To Transport enabled, "
                      "Out is the transport position in seconds and follows play, pause, "
                      "seek, loops and frame-stepped rendering.";
#else
        description = "Counts the elapsed time since Reset or the app started.";
#endif
    }
    void setup() override{
        timeGenerator::setup();
        phaseOffset=0;

        addParameter(resetWithPhaseReset.set("RstWPhs",false));
        addParameter(resetCounter.set("Reset"));
        addOutputParameter(output.set("Out", 0, 0, FLT_MAX));        

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        addInspectorParameter(syncToTransport.set("Sync To Transport", false));
#endif

        listeners.push(resetCounter.newListener([this](){
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
            if(!syncToTransport) {
                rstCounter();
            }
#else
            rstCounter();
#endif
        }));
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        listeners.push(syncToTransport.newListener([this](bool &enabled){
            if(enabled) {
                output = static_cast<float>(getFrameTransportState().current.seconds);
            } else {
                // Resume the free-running clock without changing the visible value.
                phaseOffset = getTime() - static_cast<double>(output.get());
            }
        }));
#endif
 
    }
    
    void resetPhase() override
    {
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        if(resetWithPhaseReset && !syncToTransport)
#else
        if(resetWithPhaseReset)
#endif
        {
            rstCounter();
        }
    }

    void update(ofEventArgs &a) override
    {
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        if(syncToTransport) {
            output = static_cast<float>(getFrameTransportState().current.seconds);
        } else {
            output = static_cast<float>(getTime()-phaseOffset);
        }
#else
        output = static_cast<float>(getTime()-phaseOffset);
#endif
    }
    
    void rstCounter()
    {
        phaseOffset=getTime();
    }
    
    
private:
    ofEventListeners listeners;

    ofParameter<float> output;
    ofParameter<bool> resetWithPhaseReset;
    ofParameter<void> resetCounter;
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
    ofParameter<bool> syncToTransport;
#endif
    double phaseOffset;
        
        
};


class ramp : public timeGenerator {
public:
    ramp() : timeGenerator("Ramp") {
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        description = "Generates a linear ramp from 0 to 1 over a specified duration in milliseconds. "
                      "By default it runs independently of playback. With Sync To Transport enabled, "
                      "it follows transport play/pause and frame-stepped rendering.";
#else
        description = "Generates a linear ramp from 0 to 1 over a specified duration in milliseconds. The ramp starts when triggered and holds at 1 until reset by another trigger.";
#endif
    }

    void setup() override {
        timeGenerator::setup();
        addParameter(trigger.set("Trigger"));
        addParameter(triggerFloat.set("Trigger_F",0,0,1));
        addParameter(reset.set("Reset"));
        addParameter(forceFinish.set("Force Fin"));
        addParameter(rampDurationMs.set("Ms", 1000, 0, FLT_MAX));
        addOutputParameter(output.set("Out", 0, 0, 1));
        addParameter(isRamping.set("IsRamping",false));
        addOutputParameter(isRampFinish.set("Finish",false));
        addParameter(enable.set("Enable", true));
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        addInspectorParameter(syncToTransport.set("Sync To Transport", false));
#endif
        rampStartTime = 0;
        isRamping = false;
        isRampFinish = false;
        lastTNum = 0;
        
        // Add listener for the trigger
        listeners.push(trigger.newListener([this]() {
            if(enable) startRamp();
        }));
        // Add listener for the reset
        listeners.push(reset.newListener([this]() {
            stopRamp();
        }));
        // Add listener for the trigger Float
        listeners.push(triggerFloat.newListener([this](float &f) {
            if(enable && triggerFloat==1.0)
            {
                startRamp();
            }
        }));
        listeners.push(forceFinish.newListener([this](){
            finishRamp();
        }));
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        listeners.push(syncToTransport.newListener([this](bool &){
            if(isRamping) {
                // Keep the current value continuous when changing clock source.
                rampStartTime = getRampTime() -
                    static_cast<double>(output.get()) * static_cast<double>(rampDurationMs.get()) / 1000.0;
            }
        }));
#endif
    }

    void update(ofEventArgs &a) override {
        if (isRamping) {
            isRampFinish=false;
            const double elapsedTimeMs = std::max(0.0, (getRampTime() - rampStartTime) * 1000.0);
            const double durationMs = static_cast<double>(rampDurationMs.get());
            if (durationMs > 0.0 && elapsedTimeMs < durationMs) {
                output = static_cast<float>(elapsedTimeMs / durationMs);
            } else {
                output = 1;
                isRamping = false; // Ramp completed
                isRampFinish=true;
            }
        }
    }

private:
    double getRampTime() {
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
        return syncToTransport
            ? getFrameTransportState().current.seconds
            : getFrameGlobalTimeState().current.time;
#else
        return getTime();
#endif
    }

    void startRamp() {
        rampStartTime = getRampTime();
        isRamping = true;
        isRampFinish = false;
        output = 0;
    }
    void stopRamp() {
        isRamping = false;
        isRampFinish = false;
        output = 0;
    }
    void finishRamp() {
        isRamping = false;
        isRampFinish = true;
        output = 1;
    }

    ofEventListeners listeners;
    ofParameter<void> trigger;
    ofParameter<float> triggerFloat;
    ofParameter<void> reset;
    ofParameter<float> rampDurationMs; // Duration in milliseconds
    ofParameter<float> output;
    ofParameter<bool> isRamping;
    ofParameter<bool> isRampFinish;
    ofParameter<void> forceFinish;
    ofParameter<bool> enable;
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
    ofParameter<bool> syncToTransport;
#endif
    
    double rampStartTime; // Time in seconds
    float lastTNum;
};





class simpleNumberGenerator : public ofxOceanodeNodeModel{
public:
    simpleNumberGenerator() : ofxOceanodeNodeModel("Number"){
        color = ofColor::red;
        description = "Sends a value every frame";
    };
    ~simpleNumberGenerator(){};
    
    void setup(){
        addParameter(value.set("Value", 0, -FLT_MAX, FLT_MAX));
    }
    
    void update(ofEventArgs &a){
        value = value;
    }
    
private:
    ofParameter<float> value;
};



class simpleNormalizedNumberGenerator : public ofxOceanodeNodeModel{
public:
    simpleNormalizedNumberGenerator() : ofxOceanodeNodeModel("Number01"){
        color = ofColor::red;
        description = "Sends a normalized [0..1] value every frame";
    };
    ~simpleNormalizedNumberGenerator(){};
    
    void setup(){
        addParameter(value.set("Value", 0, 0, 1));
    }
    
    void update(ofEventArgs &a){
        value = value;
    }
    
private:
    ofParameter<float> value;
};



class phasor : public ofxOceanodeNodeModel{
public:
    phasor();
    ~phasor(){};
    float getPhasor(){return basePh->getPhasor();};
    vector<float> getPhasors(){return basePh->getPhasors();};
    void  resetPhasor(){basePh->resetPhasor();};
    float getBpm(){return bpm_Param;};
    float getBeatsMult(){return beatsMult_Param->at(0);};
    float getBeatsDiv(){return beatsDiv_Param->at(0);};
    
    vector<float> getBeatsMults(){return beatsMult_Param;};
    vector<float> getBeatsDivs(){return beatsDiv_Param;};
    
    void setBeatMult(vector<float> i){beatsMult_Param=i;};
    void setBeatDiv(vector<float> i){beatsDiv_Param=i;};
    void setBeatMult(float i){beatsMult_Param = vector<float>(1, i);};
    void setBeatDiv(float i){beatsDiv_Param = vector<float>(1, i);};
    
    void setup() override;
    void update(ofEventArgs &e) override;
    
    void resetPhase() override;
    void setBpm(float bpm) override;
    void loadBeforeConnections(ofJson &json) override;
    
    shared_ptr<basePhasor> getBasePhasor(){return basePh;};
private:
    void handleSyncToTransportChanged(bool syncEnabled);
    vector<float> calculateTransportLockedRawPhasors(double beatPosition) const;
    vector<float> calculateTransportLockedPhasors(double beatPosition) const;
    double calculateTransportLockedCycles(double beatPosition, size_t index) const;
    void checkTransportLockedCycle(const ofxOceanodeFrameTransportState &frameState);
    vector<float> calculateTransportLockedCycleCounts(double beatPosition) const;
    void updateTransportLockedCycleOutput(double beatPosition, bool force);
    void setCycleOutputVisible(bool visible);
    size_t getTransportLockedPhasorCount() const;
    float getValueForIndex(const vector<float> &values, size_t index) const;

    shared_ptr<basePhasor> basePh;


    ofParameter<float>  bpm_Param;
    ofParameter<vector<float>>    beatsMult_Param;
    ofParameter<vector<float>>    beatsDiv_Param;
    ofParameter<vector<float>>  initPhase_Param;
    ofParameter<vector<float>>  phasorMonitor;
    ofParameter<bool>   loop_Param;
    ofParameter<bool>   multiTrigger_Param;
    ofParameter<void>   resetPhase_Param;
    ofParameter<bool>   audioRate_Param;
    ofParameter<bool>   syncToTransport_Param;
    ofParameter<vector<float>>  cycle_Param; // only present in Sync To Transport mode
    float phaseOffset;
    
    ofEventListeners parameterAutoSettersListeners;
    ofEventListener resetPhaseListener;
    ofEventListener cycleListener;
    bool selfTrigger;
};

#endif /* oscillator_h */
