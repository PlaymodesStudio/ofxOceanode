//
//  indexer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 02/03/2020.
//
//

#include "indexer.h"

void indexer::setup() {
    color = ofColor::blue;
    result.resize(indexs.size());
    
    putParametersInParametersGroup(parameters);
    
//#ifdef OFXOCEANODE_USE_RANDOMSEED
//    parameters->add(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
//    paramListeners.push(seed.newListener([this](vector<int> &s){
//        if(s.size() == 1){
//            if(s[0] == 0){
//                for(int i = 0; i < oscillators.size(); i++){
//                    oscillators[i].deactivateSeed();
//                }
//            }else{
//                for(int i = 0; i < oscillators.size(); i++){
//                    oscillators[i].setSeed(s[0] + i);
//                }
//            }
//        }else{
//            for(int i = 0; i < oscillators.size(); i++){
//                oscillators[i].setSeed(getValueForPosition(s, i));
//            }
//        }
//    }));
//#endif

    addOutputParameterToGroupAndInfo(indexsOut.set("Indexs", {0}, {0}, {1}));
}

void indexer::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Size") == 1){
        parameters->getInt("Size") = ofToInt(json["Size"]);
    }
}

void indexer::presetRecallAfterSettingParameters(ofJson &json){

}

void indexer::presetHasLoaded(){
    indexsOut = indexs;
}

void indexer::indexCountChanged(int &newIndexCount){
    baseIndexer::indexCountChanged(newIndexCount);
    result.resize(newIndexCount);
}

void indexer::newIndexs(){
//    for(int i=0 ; i < oscillators.size() ; i++){
//        oscillators[i].setIndexNormalized(indexs[i]);
//    }
    indexsOut = indexs;
}
