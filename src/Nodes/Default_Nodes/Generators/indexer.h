//
//  indexer.h
//  example-basic
//
//  Created by Eduard Frigola on 02/03/2020.
//
//

#ifndef indexer_h
#define indexer_h

#include "baseIndexer.h"
#include "ofxOceanodeNodeModel.h"

class indexer : public ofxOceanodeNodeModel{
public:
    indexer() : ofxOceanodeNodeModel("Indexer"){};
    ~indexer(){};
    
    void setup() override;
    void update(ofEventArgs &e) override;
    
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    
    void presetHasLoaded() override;

private:
    void indexCountChanged(int &newIndexCount);
    
    ofParameter<int>  indexCount;
    ofParameter<float>  numWaves_Param; //Desphase Quantity
    ofParameter<float>   indexInvert_Param;
    ofParameter<int>    symmetry_Param;
    ofParameter<bool>   wrapShuffle_Param;
    ofParameter<float>   indexShuffle_Param;
    ofParameter<float>  indexRand_Param;
    ofParameter<float>    indexOffset_Param;
    ofParameter<int>    indexQuant_Param;
    ofParameter<float>  combination_Param;
    ofParameter<int>    modulo_Param;
    ofParameter<bool>   normalize_Param;
    ofParameter<bool>    discrete_Param;
    
    ofParameter<vector<float>>      indexsOut;
    
    ofEventListeners paramListeners;
    
    baseIndexer base;
};

#endif /* indexer_h */
