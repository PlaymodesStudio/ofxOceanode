//
//  reindexer.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#include "reindexer.h"
#include <GLFW/glfw3.h>

#define REINDEX_UNDO_SIZE 20

void window_no_close_indexer(GLFWwindow* window){
    glfwSetWindowShouldClose(window, GL_FALSE);
};

reindexer::reindexer() : ofxOceanodeNodeModel("Reindexer"){
    parameters->add(input.set("Input", {0}, {0}, {1}));
    parameters->add(showGui.set("Show Gui", false));
    parameters->add(outputSize.set("Output Size", 10, 1, 100));
    parameters->add(output.set("Outpu", {0}, {0}, {1}));
    
    inputListenerEvent = input.newListener(this, &reindexer::inputListener);
    outputSizeListenerEvent = outputSize.newListener(this, &reindexer::outputSizeListener);
    windowListenerEvents.push_back(showGui.newListener(this, &reindexer::showReindexWindow));
    
    isReindexIdentity = true;
    reindexWindowRect.setPosition(-1, -1);
    reindexGrid.resize(1);
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
    }else{
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
}

void reindexer::showReindexWindow(bool &b){
    if(b){
        ofAppBaseWindow* mainWindow = ofGetWindowPtr();
        
        ofGLFWWindowSettings prevSettings;
        if(reindexWindowRect.getPosition() == glm::vec3(-1, -1, 0)){
            prevSettings.setSize(1024, 1024);
            prevSettings.setPosition(ofVec2f(ofGetScreenWidth()-1024, ofGetScreenHeight()-1024));
        }
        else{
            prevSettings.setSize(reindexWindowRect.width, reindexWindowRect.height);
            prevSettings.setPosition(reindexWindowRect.position);
        }
        prevSettings.windowMode = OF_WINDOW;
        prevSettings.resizable = true;
        reindexWindow = ofCreateWindow(prevSettings);
        reindexWindow->setWindowTitle("Reindex");
        windowListenerEvents.push_back(reindexWindow->events().draw.newListener(this, &reindexer::drawCustomWindow));
        windowListenerEvents.push_back(reindexWindow->events().keyPressed.newListener(this, &reindexer::keyPressed));
        windowListenerEvents.push_back(reindexWindow->events().mouseMoved.newListener(this, &reindexer::mouseMoved));
        windowListenerEvents.push_back(reindexWindow->events().mousePressed.newListener(this, &reindexer::mousePressed));
        windowListenerEvents.push_back(reindexWindow->events().mouseReleased.newListener(this, &reindexer::mouseReleased));
        windowListenerEvents.push_back(reindexWindow->events().mouseDragged.newListener(this, &reindexer::mouseDragged));
        
        ofAppGLFWWindow * ofWindow = (ofAppGLFWWindow*)reindexWindow.get();
        GLFWwindow * glfwWindow = ofWindow->getGLFWWindow();
        glfwSetWindowCloseCallback(glfwWindow, window_no_close_indexer);
    }
    else if(reindexWindow != nullptr){
        reindexWindowRect.setPosition(glm::vec3(reindexWindow->getWindowPosition(), 0));
        reindexWindowRect.setSize(reindexWindow->getWidth(), reindexWindow->getHeight());
        reindexWindow->setWindowShouldClose();
        reindexWindow = nullptr;
    }
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

void reindexer::drawCustomWindow(ofEventArgs &e){
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



void reindexer::mouseMoved(ofMouseEventArgs &a){
    
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

void reindexer::mouseReleased(ofMouseEventArgs &a){
    
}

void reindexer::mouseDragged(ofMouseEventArgs &a){
    
}


void reindexer::keyPressed(ofKeyEventArgs &a){
    if(a.key == 'c'){
        reindexGrid.assign(outputSize, vector<bool>(input.get().size(), false));
        reindexChanged();
    }else if(a.key == 'r'){
        reindexGrid = identityReindexMatrix;
        reindexChanged();
    }else if(a.key == 'z' &&  ofGetKeyPressed(OF_KEY_COMMAND)){
        if(identityStore.size() > 1){
            reindexGrid = identityStore[1];
            identityStore.pop_front();
        }
    }
}
