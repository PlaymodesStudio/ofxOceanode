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

class indexer : public baseIndexer{
public:
    indexer() : baseIndexer(100, "Indexer"){};
    ~indexer(){};
    void setup() override;
    
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    
    void presetHasLoaded() override;

private:
    void indexCountChanged(int &newIndexCount) override;
    
    template <typename T>
    T getValueForPosition(const vector<T> &param, int index){
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    }

    virtual void newIndexs() override;
    
    
    ofParameter<vector<float>>      indexsOut;
    vector<float> result;
    
    ofEventListeners paramListeners;
};

#endif /* indexer_h */
