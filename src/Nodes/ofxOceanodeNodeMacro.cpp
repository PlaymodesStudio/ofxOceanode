//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#include "ofxOceanodeNodeMacro.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModelExternalWindow("Macro"){
    canvas = nullptr;
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
    registry = container->getRegistry();
}

void ofxOceanodeNodeMacro::setup(){
    auto treg = make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<inlet<vector<float>>>("I/O");
    registry->registerModel<outlet<vector<float>>>("I/O");
    container = make_shared<ofxOceanodeContainer>(registry, treg);
    container->setWindow(nullptr);
    newNodeListener = container->newNodeCreated.newListener([this](ofxOceanodeNode* node){
        bool inletOutletCreated = true;
        string nodeName = node->getParameters()->getName();
        if(node->getNodeModel().nodeName() == "Inlet"){
            if(node->getParameters()->get("Input").type() == typeid(ofParameter<vector<float>>).name()){
                ofParameter<vector<float>> *input = new ofParameter<vector<float>>();
                paramsStore[nodeName] = input;
                parameters->add(input->set(nodeName, {0}, {0}, {1}));
                inoutListeners[node->getParameters()->getName()].push(input->newListener([this, node](vector<float> &f){
                    node->getParameters()->get<vector<float>>("Input") = f;
                }));
                inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Min").newListener([this, node](string &s){
                    float f = ofToFloat(s);
                    node->getParameters()->get<vector<float>>("Input").setMin(vector<float>(1, f));
                    string parameterName = "Input";
                    ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                    
                    parameters->get<vector<float>>(node->getParameters()->getName()).setMin(vector<float>(1, f));
                    parameterName = node->getParameters()->getName();
                    ofNotifyEvent(parameterChangedMinMax, parameterName);
                }));
                inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Max").newListener([this, node](string &s){
                    float f = ofToFloat(s);
                    node->getParameters()->get<vector<float>>("Input").setMax(vector<float>(1, f));
                    string parameterName = "Input";
                    ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                    
                    parameters->get<vector<float>>(node->getParameters()->getName()).setMax(vector<float>(1, f));
                    parameterName = node->getParameters()->getName();
                    ofNotifyEvent(parameterChangedMinMax, parameterName);
                }));
            }
        }else if(node->getNodeModel().nodeName() == "Outlet"){
            if(node->getParameters()->get("Output").type() == typeid(ofParameter<vector<float>>).name()){
                ofParameter<vector<float>> *output = new ofParameter<vector<float>>();
                paramsStore[nodeName] = output;
                parameters->add(output->set(node->getParameters()->getName(), {0}, {0}, {1}));
                inoutListeners[node->getParameters()->getName()].push(node->getParameters()->get<vector<float>>("Output").newListener([this, output](vector<float> &f){
                    parameters->get<vector<float>>(output->getName()) = f;
                }));
                inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Min").newListener([this, node](string &s){
                    float f = ofToFloat(s);
                    node->getParameters()->get<vector<float>>("Output").setMin(vector<float>(1, f));
                    string parameterName = "Output";
                    ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                    
                    parameters->get<vector<float>>(node->getParameters()->getName()).setMin(vector<float>(1, f));
                    parameterName = node->getParameters()->getName();
                    ofNotifyEvent(parameterChangedMinMax, parameterName);
                }));
                inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Max").newListener([this, node](string &s){
                    float f = ofToFloat(s);
                    node->getParameters()->get<vector<float>>("Output").setMax(vector<float>(1, f));
                    string parameterName = "Output";
                    ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                    
                    parameters->get<vector<float>>(node->getParameters()->getName()).setMax(vector<float>(1, f));
                    parameterName = node->getParameters()->getName();
                    ofNotifyEvent(parameterChangedMinMax, parameterName);
                }));
            }
        }else{
            inletOutletCreated = false;
        }
        
        if(inletOutletCreated){
            parameterGroupChanged.notify(this);
            deleteListeners.push(node->deleteModuleAndConnections.newListener([this, node](vector<ofxOceanodeAbstractConnection*> &c){
                string nodeName = node->getParameters()->getName();
                disconnectConnectionsForParameter.notify(nodeName);
                parameters->remove(nodeName);
                inoutListeners.erase(nodeName);
                parameterGroupChanged.notify(this);
                delete paramsStore[nodeName];
                paramsStore.erase(nodeName);
            }, 0));
        }
    });
}

void ofxOceanodeNodeMacro::setupForExternalWindow(){
    container->setWindow(externalWindow);
    canvas = new ofxOceanodeCanvas;
    canvas->setContainer(container);
    canvas->setup(externalWindow);
}

void ofxOceanodeNodeMacro::closeExternalWindow(ofEventArgs &e){
    ofxOceanodeNodeModelExternalWindow::closeExternalWindow(e);
    delete canvas;
    canvas = nullptr;
}

void ofxOceanodeNodeMacro::presetSave(ofJson &json){
    ofDirectory dir;
    if(!dir.doesDirectoryExist("CapsulePresets")){
        dir.createDirectory("CapsulePresets");
    }
    dir.open("CapsulePresets");
    dir.sort();
    vector<int> presets = {0};
    for(int i = 0; i < dir.listDir(); i++){
        presets.push_back(ofToInt(dir.getName(i)));
    }
    std::sort(presets.begin(), presets.end(), [](int &left, int &right) {
        return left < right;
    });
    
    string path = "CapsulePresets/" + ofToString(presets.back()+1);
    json["preset"] = path;
    container->savePreset(path);
}

void ofxOceanodeNodeMacro::loadBeforeConnections(ofJson &json){
    if(json.count("preset") != 0){
        string path = json["preset"];
        container->loadPreset(path);
    }
}
