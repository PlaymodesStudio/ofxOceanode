//
//  phasorClass.cpp
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#include "basePhasor.h"


basePhasor::basePhasor(){
    phasor = 0;    
    timer.setPeriodicEvent(1000000);
    startThread();
}

basePhasor::~basePhasor(){
    stopThread();
    phasorToSend.close();
    waitForThread(true);
}

float basePhasor::getPhasor(){
    if(loop_Param){
        double momentaryPhasor;
        while(phasorToSend.tryReceive(momentaryPhasor));
        return momentaryPhasor;
    }else{
        return initPhase_Param;
    }
}

void basePhasor::resetPhasor(){
    phasor = 0;
}

void basePhasor::threadedFunction(){
    while(isThreadRunning()){
        timer.waitNext();
        
        //tue phasor that goes from 0 to 1 at desired frequency
        double freq = (double)bpm_Param/(double)60;
        freq = freq * (double)beatsMult_Param;
        freq = (double)freq / (double)beatsDiv_Param;
        double increment = (1.0f/(double)((double)(1000.0)/(double)freq));
        
        phasor = phasor + increment;
        phasor -= (int)phasor;
        
        //Assign a copy of the phasor to add initPhase
        phasorMod = phasor;
        
        //take the initPhase_Param as a phase offset param
        phasorMod += initPhase_Param;
        phasorMod -= (int)phasorMod;
        
        if(loop_Param){
            phasorToSend.send(std::move(phasorMod));
        }
    }
}

