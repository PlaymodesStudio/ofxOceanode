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
