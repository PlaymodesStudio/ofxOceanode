//
//  basePhasor.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#ifndef basePhasor_h
#define basePhasor_h
#include "ofMain.h"
#include "ofTimer.h"

class basePhasor{
public:
	basePhasor() {};
    ~basePhasor();

	void setup();
    
    void setBpm(float bpm){
        bpm_Param = bpm;
        bpm_Param_channel.send(bpm_Param);
    }
    
    void setBeatsMult(vector<float> beatsMult){
        beatsMult_Param = beatsMult;
        beatsMult_Param_channel.send(beatsMult_Param);
        checkChangedSize();
    }
    
    void setBeatsDiv(vector<float> beatsDiv){
        beatsDiv_Param = beatsDiv;
        beatsDiv_Param_channel.send(beatsDiv_Param);
        checkChangedSize();
    }
    
    void setBeatsMult(float beatsMult){
        beatsMult_Param = vector<float>(1, beatsMult);
        beatsMult_Param_channel.send(beatsMult_Param);
        checkChangedSize();
    }
    
    void setBeatsDiv(float beatsDiv){
        beatsDiv_Param = vector<float>(1, beatsDiv);
        beatsDiv_Param_channel.send(beatsDiv_Param);
        checkChangedSize();
    }
    
    void setInitPhase(vector<float> initPhase){
        initPhase_Param = initPhase;
        initPhase_Param_channel.send(initPhase_Param);
		checkChangedSize();
    }
    
    void setLoop(bool loop){
        loop_Param = loop;
        loop_Param_channel.send(loop_Param);
    }
    
    void setMultiTrigger(bool mTrigger){
        multiTrigger = mTrigger;
        multiTrigger_channel.send(multiTrigger);
    }
    void setAudioRate(bool aRate){
        audioRate = aRate;
    }

    vector<float> getPhasors();
    float getPhasor(){
        return getPhasors()[0];
    }
    void  resetPhasor(bool global = false);
    
    ofEvent<void> phasorCycle;
    ofEvent<int> phasorCycleIndex;
    ofEvent<vector<float>> audioUpdate;
    
    void checkChangedSize(){
        if(beatsMult_Param.size() != 1 && beatsMult_Param.size() != numPhasors){
            resizePhasors(beatsMult_Param.size());
        }else if(beatsDiv_Param.size() != 1 && beatsDiv_Param.size() != numPhasors){
            resizePhasors(beatsDiv_Param.size());
        }else if(initPhase_Param.size() != 1 && initPhase_Param.size() != numPhasors){
            resizePhasors(initPhase_Param.size());
        }else if(beatsDiv_Param.size() == 1 && beatsMult_Param.size() == 1 && initPhase_Param.size() == 1 && numPhasors != 1){
            resizePhasors(1);
        }
    }
    
    void threadedFunction(double microseconds = 1000);
    void advanceForFrameRate(float framerate);
    bool isAudio(){return audioRate;};
    
private:
    
	void resizePhasors(int n) {
		if (n > 0) {
			resize = n;
		}
    }
    
    float getValueForIndex(vector<float> &vf, int i){
        if(i < vf.size()){
            return vf[i];
        }else{
            return vf[0];
        }
    }
    
    int numPhasors;
    
    ofTimer timer;

    vector<double>  phasor;
    vector<double>  phasorMod;
    ofThreadChannel<vector<double>> phasorToSend;
    vector<double> momentaryPhasor;
    vector<bool> stopPhasor;
    
    
    float    bpm_Param;
    vector<float>    beatsMult_Param;
    vector<float>    beatsDiv_Param;
    vector<float>  initPhase_Param;
    bool   loop_Param;
    bool    multiTrigger;
    bool    audioRate;
    
    ofThreadChannel<float>    bpm_Param_channel;
    ofThreadChannel<vector<float>>    beatsMult_Param_channel;
    ofThreadChannel<vector<float>>    beatsDiv_Param_channel;
    ofThreadChannel<vector<float>>  initPhase_Param_channel;
    ofThreadChannel<bool>   loop_Param_channel;
    ofThreadChannel<bool>   multiTrigger_channel;
	ofThreadChannel<bool>	reset_channel;
    
    float    bpm_Param_inThread;
    vector<float>    beatsMult_Param_inThread;
    vector<float>    beatsDiv_Param_inThread;
    vector<float>  initPhase_Param_inThread;
    bool   loop_Param_inThread;
    bool    multiTrigger_inThread;
	bool reset_inThread;

	int resize;
};

#endif /* basePhasor_h */
