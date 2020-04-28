//
//  indexer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 02/03/2020.
//
//

#include "indexer.h"

void indexer::setup() {
    color = ofColor::orange;
    result.resize(indexs.size());
    
    putParametersInParametersGroup();

    addOutputParameter(indexsOut.set("Output", {0}, {0}, {1}));
    indexsOut = indexs;
}

void indexer::update(ofEventArgs &e){
    //indexsOut = indexs;
}

void indexer::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Size") == 1){
		//TODO: FIX size loading
        //parameters->getInt("Size") = ofToInt(json["Size"]);
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
    indexsOut = indexs;
}
