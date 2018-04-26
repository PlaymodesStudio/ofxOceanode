//
//  ofxOceanodeNodeModelLocalPreset.cpp
//  VJYourself_OCEAN
//
//  Created by Eduard Frigola on 25/04/2018.
//

#include "ofxOceanodeNodeModelLocalPreset.h"


ofxOceanodeNodeModelLocalPreset::ofxOceanodeNodeModelLocalPreset(string name) : ofxOceanodeNodeModel(name){
    parameters->add(presetControl.set("Preset", 0));
    listener = presetControl.newListener(this, &ofxOceanodeNodeModelLocalPreset::presetListener);
}

void ofxOceanodeNodeModelLocalPreset::presetListener(short int &preset){
    if(preset < 0){ //save preset
        ofJson json;
        for(int i = 0; i < parameters->size(); i++){
            ofAbstractParameter& p = parameters->get(i);
            if(getParameterInfo(p).isSavePreset){
                if(p.type() == typeid(ofParameter<float>).name()){
                    ofSerialize(json, p);
                }else if(p.type() == typeid(ofParameter<int>).name()){
                    ofSerialize(json, p);
                }
                else if(p.type() == typeid(ofParameter<bool>).name()){
                    ofSerialize(json, p);
                }
                else if(p.type() == typeid(ofParameter<ofColor>).name()){
                    ofSerialize(json, p);
                }
                else if(p.type() == typeid(ofParameter<vector<float>>).name()){
                    auto vecF = p.cast<vector<float>>().get();
                    if(vecF.size() == 1){
                        json[p.getEscapedName()] = vecF[0];
                    }
                }
                else if(p.type() == typeid(ofParameter<vector<int>>).name()){
                    auto vecI = p.cast<vector<int>>().get();
                    if(vecI.size() == 1){
                        json[p.getEscapedName()] = vecI[0];
                    }
                }
                else if(p.type() == typeid(ofParameterGroup).name()){
                    ofSerialize(json, p.castGroup().getInt(1));
                }
            }
        }
        ofSavePrettyJson("LocalPresets/" + nameIdentifier + "/" + ofToString(-preset) + ".json", json);
    }
    else if(preset > 0){ //load Preset
        ofJson json = ofLoadJson("LocalPresets/" + nameIdentifier + "/" + ofToString(preset) + ".json");
        
        if(json.empty()) return;
        
        for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
            if(parameters->contains(it.key())){
                ofAbstractParameter& p = parameters->get(it.key());
                if(getParameterInfo(p).isSavePreset){
                    if(p.type() == typeid(ofParameter<float>).name()){
                        ofDeserialize(json, p);
                    }else if(p.type() == typeid(ofParameter<int>).name()){
                        ofDeserialize(json, p);
                    }
                    else if(p.type() == typeid(ofParameter<bool>).name()){
                        ofDeserialize(json, p);
                    }
                    else if(p.type() == typeid(ofParameter<ofColor>).name()){
                        ofDeserialize(json, p);
                    }
                    else if(p.type() == typeid(ofParameter<vector<float>>).name()){
                        float value = it.value();
                        p.cast<vector<float>>() = vector<float>(1, value);
                    }
                    else if(p.type() == typeid(ofParameter<vector<int>>).name()){
                        int value = it.value();
                        p.cast<vector<int>>() = vector<int>(1, value);
                    }
                    else if(p.type() == typeid(ofParameterGroup).name()){
                        ofDeserialize(json, p.castGroup().getInt(1));
                    }
                }
            }
        }
    }
}
