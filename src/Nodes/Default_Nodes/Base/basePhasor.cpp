//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "basePhasor.h"


void basePhasor::setup(){
    phasor = vector<double>(1, 0);
    phasorMod = vector<double>(1, 0);
    momentaryPhasor = vector<double>(1, 0);
    bpm_Param = 120.00;
    beatsMult_Param = vector<float>(1, 1);
    beatsDiv_Param = vector<float>(1, 1);
    initPhase_Param = vector<float>(1, 0);
    loop_Param = true;
    multiTrigger = false;
    audioRate = false;
    bpm_Param_inThread = 120.00;
    beatsMult_Param_inThread = vector<float>(1, 1);
    beatsDiv_Param_inThread = vector<float>(1, 1);
    initPhase_Param_inThread = vector<float>(1, 0);;
    loop_Param_inThread = true;
    multiTrigger_inThread = false;
	reset_inThread = false;
    numPhasors = 1;
	resize = -1;
    stopPhasor = vector<bool>(1, true);
}

basePhasor::~basePhasor(){
    phasorToSend.close();
}

vector<float> basePhasor::getPhasors(){
    while(phasorToSend.tryReceive(momentaryPhasor));
	reset_channel.send(false);
    return vector<float>(momentaryPhasor.begin(), momentaryPhasor.end());
}

void basePhasor::resetPhasor(bool global){
    if(multiTrigger && !global){
        if(stopPhasor.back() == true){
            stopPhasor.back() = false;
        }else{
            resizePhasors(numPhasors+1);
            stopPhasor.back() = false;
        }
    }else if((loop_Param && global) || !global){
        fill(stopPhasor.begin(), stopPhasor.end(), false);
        fill(phasor.begin(), phasor.end(), 0);
		reset_channel.send(true);
    }
    momentaryPhasor = phasor;
}

void basePhasor::threadedFunction(double microseconds){
		if (resize > 0) {
			numPhasors = resize;
			phasor.resize(resize, 0);
            std::fill(phasor.begin(), phasor.end(), 0.0);
			phasorMod.resize(resize, 0);
			stopPhasor.resize(resize, !loop_Param);
			resize = -1;
		}
        while(bpm_Param_channel.tryReceive(bpm_Param_inThread));
        while(beatsMult_Param_channel.tryReceive(beatsMult_Param_inThread));
        while(beatsDiv_Param_channel.tryReceive(beatsDiv_Param_inThread));
        while(initPhase_Param_channel.tryReceive(initPhase_Param_inThread));
        while(loop_Param_channel.tryReceive(loop_Param_inThread));
        while(multiTrigger_channel.tryReceive(multiTrigger_inThread));
		while(reset_channel.tryReceive(reset_inThread));
        vector<float> removePhasors;
        for(int i = 0; i < numPhasors; i++){
            if(reset_inThread){
                phasor[i] = 0;
            }else{
                //tue phasor that goes from 0 to 1 at desired frequency
                double freq = (double)bpm_Param_inThread/(double)60;
                freq = freq * (double)getValueForIndex(beatsMult_Param_inThread, i);
                auto beats_div_val = (double)getValueForIndex(beatsDiv_Param_inThread, i);
                if(beats_div_val == 0) beats_div_val = 0.0001;
                freq = (double)freq / beats_div_val;
                double increment = (1.0f/(double)((double)(microseconds)/(double)freq));
                
                phasor[i] = phasor[i] + increment;
                if(i == 0 && (phasor[0] >= 1 || phasor[0] < 0)){
                    ofNotifyEvent(phasorCycle);
                }
                if(phasor[i] >= 1 || phasor[i] < 0){
                    ofNotifyEvent(phasorCycleIndex, i);
                    if(!loop_Param_inThread){
                        stopPhasor[i] = true;
                        if(multiTrigger_inThread && numPhasors > 1 && removePhasors.size() < numPhasors-1){
                            removePhasors.push_back(i);
                        }
                    }
                }
                if(loop_Param_inThread){
                    stopPhasor[i] = false;
                }
                
                if(stopPhasor[i]) phasor[i] = 0;
                
                if(phasor[i] < 0) phasor[i] += 1.0f;
                phasor[i] -= (int)phasor[i];
                
            }
            
            //Assign a copy of the phasor to add initPhase
            phasorMod[i] = phasor[i];
            
            //take the initPhase_Param as a phase offset param
            phasorMod[i] += getValueForIndex(initPhase_Param_inThread, i);
            phasorMod[i] -= (int)phasorMod[i];
        }
        
        for(int i = removePhasors.size()-1; i >= 0; i--){
            phasor.erase(phasor.begin() + removePhasors[i]);
            phasorMod.erase(phasorMod.begin() + removePhasors[i]);
            stopPhasor.erase(stopPhasor.begin() + removePhasors[i]);
            numPhasors--;
        }
        
		//Clear threadChannel before filling it.
		vector<double> oldPhasor;
		while(phasorToSend.tryReceive(oldPhasor));
        phasorToSend.send((phasorMod));
}

void basePhasor::advanceForFrameRate(float framerate){
    if (resize > 0) {
        numPhasors = resize;
        phasor.resize(resize, 0);
        std::fill(phasor.begin(), phasor.end(), 0.0);
        phasorMod.resize(resize, 0);
        stopPhasor.resize(resize, !loop_Param);
        resize = -1;
    }
    vector<float> removePhasors;
    for(int i = 0; i < numPhasors; i++){
        if(false){
//            phasor[i] = 0;
        }else{
            //tue phasor that goes from 0 to 1 at desired frequency
            double freq = (double)bpm_Param/(double)60;
            freq = freq * (double)getValueForIndex(beatsMult_Param, i);
            auto beats_div_val = (double)getValueForIndex(beatsDiv_Param, i);
            if(beats_div_val == 0) beats_div_val = 0.0001;
            freq = (double)freq / beats_div_val;
            double increment = (1.0f/(double)((double)(framerate)/(double)freq));
            
            phasor[i] = phasor[i] + increment;
            if(i == 0 && (phasor[0] >= 1 || phasor[0] < 0)){
                ofNotifyEvent(phasorCycle);
            }
            if(phasor[i] >= 1 || phasor[i] < 0){
                ofNotifyEvent(phasorCycleIndex, i);
                if(!loop_Param){
                    stopPhasor[i] = true;
                    if(multiTrigger && numPhasors > 1 && removePhasors.size() < numPhasors-1){
                        removePhasors.push_back(i);
                    }
                }
            }
            if(loop_Param){
                stopPhasor[i] = false;
            }
            
            if(stopPhasor[i]) phasor[i] = 0;
            
            if(phasor[i] < 0) phasor[i] += 1.0f;
            phasor[i] -= (int)phasor[i];
            
        }
        
        //Assign a copy of the phasor to add initPhase
        phasorMod[i] = phasor[i];
        
        //take the initPhase_Param as a phase offset param
        phasorMod[i] += getValueForIndex(initPhase_Param, i);
        phasorMod[i] -= (int)phasorMod[i];
    }
    
    for(int i = removePhasors.size()-1; i >= 0; i--){
        phasor.erase(phasor.begin() + removePhasors[i]);
        phasorMod.erase(phasorMod.begin() + removePhasors[i]);
        stopPhasor.erase(stopPhasor.begin() + removePhasors[i]);
        numPhasors--;
    }
    momentaryPhasor = phasorMod;
    if(isAudio()){
        vector<float> tvec = vector<float>(phasorMod.begin(), phasorMod.end());
        audioUpdate.notify(this, tvec);
    }
}
