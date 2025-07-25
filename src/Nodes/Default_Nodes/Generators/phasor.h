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
    void    setTime(float t) {time = t;};
    float   getTime() {return time;};

private:
    float time;
};



class counter : public timeGenerator{
public:
    counter() : timeGenerator("Counter"){
        description = "Counts the elapsed time since reset has been preset or the app started";
    }
    void setup() override{
        timeGenerator::setup();
        phaseOffset=0;

        addParameter(resetWithPhaseReset.set("RstWPhs",false));
        addParameter(resetCounter.set("Reset"));
        addOutputParameter(output.set("Out", 0, 0, FLT_MAX));        

        listeners.push(resetCounter.newListener([this](){
            rstCounter();
        }));
 
    }
    
    void resetPhase() override
    {
        if(resetWithPhaseReset)
        {
            rstCounter();
        }
    }

    void update(ofEventArgs &a) override
    {
        output = getTime()-phaseOffset;
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
    float phaseOffset;
        
        
};


class ramp : public timeGenerator {
public:
    ramp() : timeGenerator("Ramp") {
        description = "Generates a linear ramp from 0 to 1 over a specified duration in milliseconds. The ramp starts when triggered and holds at 1 until reset by another trigger.";
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
        rampStartTime = 0;
        isRamping = false;
        isRampFinish = false;
        lastTNum = 0;
        
        // Add listener for the trigger
        listeners.push(trigger.newListener([this]() {
            startRamp();
        }));
        // Add listener for the reset
        listeners.push(reset.newListener([this]() {
            stopRamp();
        }));
        // Add listener for the trigger Float
        listeners.push(triggerFloat.newListener([this](float &f) {
            if(triggerFloat==1.0)
            {
                startRamp();
            }
        }));
        listeners.push(forceFinish.newListener([this](){
            finishRamp();
        }));
    }

    void update(ofEventArgs &a) override {
        if (isRamping) {
            isRampFinish=false;
            float elapsedTime = (getTime() - rampStartTime) * 1000.0f;
            if (elapsedTime < rampDurationMs) {
                output = elapsedTime / rampDurationMs;
            } else {
                output = 1;
                isRamping = false; // Ramp completed
                isRampFinish=true;
            }
        }
    }

private:
    void startRamp() {
        rampStartTime = getTime();
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
    
    float rampStartTime; // Time in seconds
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
    
    void setup();
    void update(ofEventArgs &e);
    
    void resetPhase() override;
    void setBpm(float bpm) override;
    
    shared_ptr<basePhasor> getBasePhasor(){return basePh;};
private:
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
    float phaseOffset;
    
    ofEventListeners parameterAutoSettersListeners;
    ofEventListener resetPhaseListener;
    ofEventListener cycleListener;
    bool selfTrigger;
};

#endif /* oscillator_h */
