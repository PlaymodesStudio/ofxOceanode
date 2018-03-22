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

void basePhasor::update(ofEventArgs &e){
    //getPhasor();
}

float basePhasor::getPhasor(){
    if(!offlineMode_Param){
        double momentaryPhasor;
        while(phasorToSend.tryReceive(momentaryPhasor));
        phasorMonitor = ofMap(momentaryPhasor, 0, 1, phasorMonitor, 1.0);
        
        return momentaryPhasor;
    }
//    return (float)ofMap(phasorMod, 0, 1, minVal_Param, maxVal_Param);
    return phasorMod;
}

void basePhasor::resetPhasor(){
    phasor = 0;
//    phasorMod = ofMap(initPhase_Param, minVal_Param, maxVal_Param, 0, 1, true);;
    if(initPhase_Param>1.0) phasorMod=1;
    else phasorMod = initPhase_Param;
}

void basePhasor::threadedFunction(){
    while(isThreadRunning()){
        timer.waitNext();
        if(loop_Param && !offlineMode_Param){
            //tue phasor that goes from 0 to 1 at desired frequency
            double freq = (double)bpm_Param/(double)60;
            freq = freq * (double)beatsMult_Param;
            //TODO: This is not working
            freq = (double)freq / (double)beatsDiv_Param;
            
            double increment = (1.0f/(double)((double)(1000.0)/(double)freq));
            
            // We want to use half speed with bounce param,
            // becouse we will make a triangle wave and it
            // will go and return with the same period
            // it we don't change it
            
            phasor = bounce_Param ? phasor + increment/2 : phasor + increment;
            phasor -= (int)phasor;
            
            //Assign a copy of the phasor to make some modifications
            phasorMod = phasor;
            
            //We make a triangle Wave
            if(bounce_Param)
                phasorMod = 1-(fabs((phasor * (-2))+ 1));
            
            //take the initPhase_Param as a phase offset param
            phasorMod += ofMap(initPhase_Param, 0, 1, 0, 1, true);
            if(phasorMod >= 1.0)
                phasorMod -= 1.0;
            
            
            //Quantization -- only get the values we are interested
            if(quant_Param != 40){
                phasorMod = (int)(phasorMod*quant_Param);
                phasorMod /= quant_Param;
            }
            
            phasorToSend.send(std::move(phasorMod));
        }
    }
}

void basePhasor::nextFrame(){
//    if(offlineMode_Param){
//        //tue phasor that goes from 0 to 1 at desired frequency
//        double freq = (double)bpm_Param/(double)60;
//        freq = freq * (double)beatsMult_Param;
//        //TODO: This is not working
//        freq = (double)freq / (double)beatsDiv_Param;
//
//        double increment = (1.0f/(double)(((double)60.0)/(double)freq));
//
//        // We want to use half speed with bounce param,
//        // becouse we will make a triangle wave and it
//        // will go and return with the same period
//        // it we don't change it
//
//        phasor = bounce_Param ? phasor + increment/2 : phasor + increment;
//        phasor -= (int)phasor;
//
//        //Assign a copy of the phasor to make some modifications
//        phasorMod = phasor;
//
//        //We make a triangle Wave
//        if(bounce_Param)
//            phasorMod = 1-(fabs((phasor * (-2))+ 1));
//
//        //take the initPhase_Param as a phase offset param
//        phasorMod += ofMap(initPhase_Param, minVal_Param, maxVal_Param, 0, 1, true);
//        if(phasorMod >= 1.0)
//            phasorMod -= 1.0;
//
//
//        //Quantization -- only get the values we are interested
//        if(quant_Param != 40){
//            phasorMod = (int)(phasorMod*quant_Param);
//            phasorMod /= quant_Param;
//        }
//        parameters->getFloat("Phasor Monitor") = ofMap(phasorMod, 0, 1, minVal_Param, maxVal_Param);
//    }
}

void basePhasor::loopChanged(bool &val){
    if(!val){
        //phasorMod = ofMap(initPhase_Param, 0, 1, 0, 1, true);
        if(phasorMod>1.0) phasorMod = 1.0;
    }else{
        phasor = 0;
    }
}

void basePhasor::initPhaseChanged(float &f){
    if(loop_Param == false){
        if(phasorMod>1.0) phasorMod = 1.0;
    }

}

