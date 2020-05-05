//
//  phasor.h
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#ifndef phasor_h
#define phasor_h

#include "ofxOceanodeNodeModel.h"
#include "basePhasor.h"

class simpleNumberGenerator : public ofxOceanodeNodeModel{
public:
    simpleNumberGenerator() : ofxOceanodeNodeModel("Simple Number Generator"){
        color = ofColor::red;
    };
    ~simpleNumberGenerator(){};
    
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
    float getPhasor(){return basePh.getPhasor();};
    vector<float> getPhasors(){return basePh.getPhasors();};
    void  resetPhasor(){basePh.resetPhasor();};
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
    
private:
    basePhasor basePh;


    ofParameter<float>  bpm_Param;
    ofParameter<vector<float>>    beatsMult_Param;
    ofParameter<vector<float>>    beatsDiv_Param;
    ofParameter<float>  initPhase_Param;
    ofParameter<vector<float>>  phasorMonitor;
    ofParameter<bool>   loop_Param;
    ofParameter<void>   resetPhase_Param;
    float phaseOffset;
    
    ofEventListeners parameterAutoSettersListeners;
    ofEventListener resetPhaseListener;
    ofEventListener cycleListener;
    bool selfTrigger;
};

#endif /* oscillator_h */
