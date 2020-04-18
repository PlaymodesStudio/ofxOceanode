#include "ofApp.h"
#include "testNode.h"
#include "testController.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetVerticalSync(false);
    //ofSetFrameRate(120);
    
    
    oceanode.registerType<customClass*>();
    oceanode.setup();
    auto controllerRef = oceanode.addController<testController>();
    oceanode.registerModel<testNode>("Custom Nodes", controllerRef);
}

//--------------------------------------------------------------
void ofApp::update(){
    oceanode.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
    oceanode.draw();
//    ofDrawBitmapString(ofToString(ofGetFrameRate()), glm::vec2(0,10));
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
