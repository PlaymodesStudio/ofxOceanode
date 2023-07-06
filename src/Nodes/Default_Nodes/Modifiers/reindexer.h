//
//  reindexer.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#ifndef reindexer_h
#define reindexer_h

#include "ofxOceanodeNodeModel.h"

class reindexer : public ofxOceanodeNodeModel{
public:
    reindexer() : ofxOceanodeNodeModel("Reindexer"){};
    ~reindexer(){};
    void setup() override;
	
	void update(ofEventArgs &a) override;
    
private:
    ofEventListeners eventListeners;
	
	void calculateReindex(vector<float> &vf);
    
	
	vector<float> tempOutput;
	bool alreadyCalculated;
	
    ofParameter<bool> normIndexs;
    ofParameter<vector<float>> input;
	ofParameter<vector<float>> indexs;
    ofParameter<vector<float>> output;
    ofParameter<int> mode;
    ofParameter<bool> allowEmpty;
};

#endif /* reindexer_h */
