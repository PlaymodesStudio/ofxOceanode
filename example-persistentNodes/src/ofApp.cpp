#include "ofApp.h"
#include "staticTestModule.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofDisableArbTex();
    ofSetVerticalSync(true);
    ofSetEscapeQuitsApp(false);
    
    auto reg = make_shared<ofxOceanodeNodeRegistry>();
    auto treg = make_shared<ofxOceanodeTypesRegistry>();
    
    container = make_shared<ofxOceanodeContainer>(reg, treg);
    reg->registerModel<staticTestModule>();
    canvas.setContainer(container);
    canvas.setup();
    
    container->loadPersistent();
//    container->createPersistentNode<staticTestModule>();
//    container->createPersistentNode<staticTestModule>();

    controls = make_unique<ofxOceanodeControls>(container);
}

//--------------------------------------------------------------
void ofApp::update(){
    
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofDrawBitmapString(ofGetFrameRate(), 10, 10);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == 's' && ofGetKeyPressed(OF_KEY_COMMAND)){
        container->savePersistent();
    }
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

