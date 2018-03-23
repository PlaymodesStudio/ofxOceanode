//
//  phasor.cpp
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#include "phasor.h"

phasor::phasor() : ofxOceanodeNodeModel("Phasor2")
{
    parameterAutoSettersListeners.push_back(bpm_Param.newListener([&](float &val){
        basePh.bpm_Param = val;
    }));
    parameterAutoSettersListeners.push_back(initPhase_Param.newListener([&](float &val){
        basePh.initPhase_Param = val;
    }));
    parameterAutoSettersListeners.push_back(phasorMonitor.newListener([&](float &val){
        basePh.phasorMonitor = val;
    }));
    parameterAutoSettersListeners.push_back(beatsMult_Param.newListener([&](int &val){
        basePh.beatsMult_Param = val;
    }));
    parameterAutoSettersListeners.push_back(beatsDiv_Param.newListener([&](int &val){
        basePh.beatsDiv_Param = val;
    }));
    parameterAutoSettersListeners.push_back(quant_Param.newListener([&](int &val){
        basePh.quant_Param = val;
    }));
    parameterAutoSettersListeners.push_back(loop_Param.newListener([&](bool &val){
        basePh.loop_Param = val;
    }));
    parameterAutoSettersListeners.push_back(bounce_Param.newListener([&](bool &val){
        basePh.bounce_Param = val;
    }));
    parameterAutoSettersListeners.push_back(offlineMode_Param.newListener([&](bool &val){
        basePh.offlineMode_Param = val;
    }));

//    parameterAutoSettersListeners.push_back(resetPhase_Param.newListener(v{
//        basePh.resetPhasor();
//    }));


    parameters->add(bpm_Param.set("BPM", 120, 0, 999));
    parameters->add(beatsDiv_Param.set("Beats Div", 2, 1, 512));
    parameters->add(beatsMult_Param.set("Beats Mult", 1, 1, 512));
    parameters->add(quant_Param.set("Quantization", 40, 1, 40));
    parameters->add(initPhase_Param.set("Initial Phase", 0, 0, 1));
    parameters->add(resetPhase_Param.set("Reset Phase"));
    parameters->add(loop_Param.set("Loop", true));
    parameters->add(bounce_Param.set("Bounce", false));
    parameters->add(offlineMode_Param.set("Offline Mode", false));
    parameters->add(phasorMonitor.set("Phasor Output", 0, 0, 1));
    
    resetPhaseListener = resetPhase_Param.newListener([&](){
        basePh.resetPhasor();
    });
    //resetPhase_Param.addListener(this, &basePhasor::resetPhasor);
    //loop_Param.addListener(this, &basePhasor::loopChanged);
    //initPhase_Param.addListener(this, &basePhasor::initPhaseChanged);

}
void phasor::update(ofEventArgs &e)
{
    basePh.update(e);
    phasorMonitor = basePh.getPhasor();
}

