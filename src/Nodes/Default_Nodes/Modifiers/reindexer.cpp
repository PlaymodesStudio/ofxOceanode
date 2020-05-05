//
//  reindexer.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "reindexer.h"

#define REINDEX_UNDO_SIZE 20

void reindexer::setup(){
    color = ofColor::orange;
    addParameter(input.set("Input", {0}, {0}, {1}));
    addParameter(outputSize.set("Out Size", 10, 1, 100));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    inputListenerEvent = input.newListener(this, &reindexer::inputListener);
    outputSizeListenerEvent = outputSize.newListener(this, &reindexer::outputSizeListener);
    
    isReindexIdentity = true;
    reindexGrid.resize(10, vector<bool>(1, false));
}

void reindexer::presetSave(ofJson &json){
    string matrixInfo;
    for(int i = 0; i < reindexGrid.size(); i++){
        for(int j = 0; j < reindexGrid[i].size(); j++){
            matrixInfo += ofToString(reindexGrid[i][j]);
        }
    }
    json["reindexGridWidth"] = reindexGrid.size();
    json["reindexGridHeight"] = reindexGrid[0].size();
    json["reindexGrid"] = matrixInfo;
}

void reindexer::presetRecallAfterSettingParameters(ofJson &json){
    if(json.count("reindexGrid") == 1){
        int width = json["reindexGridWidth"];
        int height = json["reindexGridHeight"];
        string matrixInfo = json["reindexGrid"];
        vector<vector<bool>> matrixCopy = vector<vector<bool>>(width, vector<bool>(height, false));
        for(int i = 0; i < width; i++){
            for(int j = 0; j < height; j++){
                matrixCopy[i][j] = matrixInfo[(i*height) + j] == '1' ? true : false;
            }
        }
        reindexGrid = matrixCopy;
        reindexChanged();
//        vector<float> tempInput = input.get();
//        inputListener(tempInput);
    }
}

void reindexer::inputListener(vector<float> &vf){
    if(vf.size() != reindexGrid[0].size()){
        outputSize = vf.size();
        identityReindexMatrix = vector<vector<bool>>(vf.size(), vector<bool>(vf.size(), false));
        for(int i = 0; i < vf.size(); i++){
            identityReindexMatrix[i][i] = true;
        }
        isReindexIdentity = true;
        reindexGrid = identityReindexMatrix;
    }
    if(isReindexIdentity){
        output = vf;
    }else if(outputSize > 0){
        vector<float> tempOutput;
        tempOutput.resize(outputSize, 0);
        for(int i = 0; i < outputSize; i++){
            for(int j = 0; j < input.get().size(); j++){
                if(reindexGrid[i][j]){
                    if(input.get()[j] > tempOutput[i]){
                        tempOutput[i] = input.get()[j];
                    }
                }
            }
        }
        output = tempOutput;
    }
}

void reindexer::outputSizeListener(int &f){
    identityReindexMatrix.resize(f, vector<bool>(input.get().size(), false));
    for(int i = 0; i < min((int)input.get().size(), f); i++){
        identityReindexMatrix[i][i] = true;
    }
    reindexGrid.resize(f, vector<bool>(input.get().size(), false));
    reindexChanged();
    vector<float> tempInput = input.get();
    inputListener(tempInput);
}

void reindexer::reindexChanged(){
    identityStore.push_front(reindexGrid);
    if(identityStore.size() > REINDEX_UNDO_SIZE){
        identityStore.pop_back();
    }
    if(reindexGrid == identityReindexMatrix && input.get().size() == outputSize){
        isReindexIdentity = true;
    }
    else{
        isReindexIdentity = false;
    }
}

void reindexer::drawInExternalWindow(ofEventArgs &e){
    ofBackground(127);
    ofSetColor(255);
    
    int xSize = outputSize;
    int ySize = input.get().size();
    
    //Draw red lines when quqntize
//    vector<int> indexQuantize(indexCount, 1);
//    if(indexQuant_Param != indexCount){
//        int oldVal = -1;
//        for(int i = 0; i < indexCount; i++){
//            int newVal = floor(i/((float)indexCount/(float)indexQuant_Param));
//            if(newVal != oldVal)
//                indexQuantize[i] = 1;
//            else
//                indexQuantize[i] = 0;
//            oldVal = newVal;
//        }
//    }

    int x_margin = 10;
    int y_margin = 10;
    int x_labelWidth = 20;
    int y_labelHeight = 20;
    float x_step = (ofGetWidth() - x_margin*2 - x_labelWidth)/xSize;
    float y_step = (ofGetHeight() - y_margin*2 - y_labelHeight)/ySize;
    for(int i = 0; i < xSize; i++){
        ofDrawBitmapString(ofToString(i), (i+0.5)*x_step + x_margin + x_labelWidth, 10);
        for(int j = 0; j < ySize; j++){
            if(i == 0){
                ofDrawBitmapString(ofToString(j), 5, (j+0.5)*y_step + y_margin + y_labelHeight);
            }
            ofPushStyle();
            ofSetRectMode(OF_RECTMODE_CORNER);

//            if(indexQuantize[i] == 1){
//                ofFill();
//                ofSetColor(255, 0, 0, 127);
//                ofDrawRectangle(i*x_step + x_margin + x_labelWidth, j*y_step + y_margin + y_labelHeight, x_step, y_step);
//            }
            ofNoFill();
            ofSetColor(255);
            ofDrawRectangle(i*x_step + x_margin + x_labelWidth, j*y_step + y_margin + y_labelHeight, x_step, y_step);
            ofPopStyle();
            if(reindexGrid[i][j]){
                ofDrawLine((i+0.25)*x_step + x_margin + x_labelWidth
                           , (j+0.25)*y_step + y_margin + y_labelHeight
                           , (i+0.75)*x_step + x_margin + x_labelWidth
                           , (j+0.75)*y_step + y_margin + y_labelHeight);
                ofDrawLine((i+0.75)*x_step + x_margin + x_labelWidth
                           , (j+0.25)*y_step + y_margin + y_labelHeight
                           , (i+0.25)*x_step + x_margin + x_labelWidth
                           , (j+0.75)*y_step + y_margin + y_labelHeight);
            }
        }
    }
}

void reindexer::mousePressed(ofMouseEventArgs &a){
    int x_margin = 10;
    int y_margin = 10;
    int x_labelWidth = 20;
    int y_labelHeight = 20;
    float x_step = (ofGetWidth() - x_margin*2 - x_labelWidth)/outputSize;
    float y_step = (ofGetHeight() - y_margin*2 - y_labelHeight)/input.get().size();
    for(int i = 0; i < outputSize; i++){
        for(int j = 0; j < input.get().size(); j++){
            ofSetRectMode(OF_RECTMODE_CORNER);
            ofRectangle rect(i*x_step + x_margin + x_labelWidth, j*y_step + y_margin + y_labelHeight, x_step, y_step);
            if(rect.inside(a)){
                reindexGrid[i][j] = !reindexGrid[i][j];
                reindexChanged();
                break;
            }
        }
    }
}

void reindexer::keyPressed(ofKeyEventArgs &a){
    if(a.key == 'c'){
        reindexGrid.assign(outputSize, vector<bool>(input.get().size(), false));
        reindexChanged();
    }else if(a.key == 'r'){
        reindexGrid = identityReindexMatrix;
        reindexChanged();
#ifdef TARGET_OSX
    }else if(a.key == 'z' &&  ofGetKeyPressed(OF_KEY_COMMAND)){
#else
    }else if(a.key == 'z' &&  ofGetKeyPressed(OF_KEY_CONTROL)){
#endif
        if(identityStore.size() > 1){
            reindexGrid = identityStore[1];
            identityStore.pop_front();
        }
    }
}
