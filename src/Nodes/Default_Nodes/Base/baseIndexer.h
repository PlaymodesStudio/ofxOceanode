//
//  baseIndexer.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#ifndef baseIndexer_h
#define baseIndexer_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"

class baseIndexer : public ofxOceanodeNodeModel{
public:
    baseIndexer(int numIndexs, string name);
    ~baseIndexer(){};
    
    void putParametersInParametersGroup(ofParameterGroup* pg);
    
    bool areNewIndexs(){
        return newIndexsFlag;
        newIndexsFlag = false;
    };
    vector<float> getIndexs(){return indexs;};;
    
    virtual void indexCountChanged(int &newIndexCount);
    
protected:
    vector<float>       indexs;
    virtual void        newIndexs(){
        newIndexsFlag = true;
    };
    
    ofParameter<int>  indexCount;
    ofParameter<float>  numWaves_Param; //Desphase Quantity
    ofParameter<float>   indexInvert_Param;
    ofParameter<int>    symmetry_Param;
    ofParameter<float>  indexRand_Param;
    ofParameter<float>    indexOffset_Param;
    ofParameter<int>    indexQuant_Param;
    ofParameter<float>  combination_Param;
    ofParameter<int>    modulo_Param;
    
private:
    void parameterBoolListener(bool &b){recomputeIndexs();};
    void parameterFloatListener(float &f){recomputeIndexs();};
    void parameterIntListener(int &i){recomputeIndexs();};
    void recomputeIndexs();
    void indexRandChanged(float &val);
    
    
    vector<int>         indexRand;
    float               indexRand_Param_previous;
    bool newIndexsFlag;
    int previousIndexCount;
};

#endif /* baseIndexer_h */
