//
//  ofxOceanodeNodeGui.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node) : container(_container), node(_node){
    color = ofColor::red;
    position = glm::vec2(10, 10);
    isDraggingGui = false;
    
    createGuiFromParameters();
    ofRegisterMouseEvents(this);
}

ofxOceanodeNodeGui::~ofxOceanodeNodeGui(){
    ofUnregisterMouseEvents(this);
}

void ofxOceanodeNodeGui::createGuiFromParameters(){
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    gui = make_unique<ofxDatGui>();
    //        gui->setAutoDraw(false);
    
    //    gui->setTransformMatrix(transformMatrix);
    
    ofxDatGuiTheme* theme = new ofxDatGuiThemeCharcoal;
    theme->color.slider.fill = color;
    theme->color.textInput.text = color;
    theme->color.icons = color;
    gui->setTheme(theme);
    
    gui->setWidth(290);
    gui->addHeader(getParameters()->getName());
    gui->addFooter();
    if(position == glm::vec2(-1, -1)){
        gui->setPosition(0, 0);
    }else{
        gui->setPosition(position.x, position.y);
    }
    
    for(int i=0 ; i<getParameters()->size(); i++){
        ofAbstractParameter &absParam = getParameters()->get(i);
        if(absParam.type() == typeid(ofParameter<float>).name()){
            gui->addSlider(absParam.cast<float>())->setPrecision(1000);
        }else if(absParam.type() == typeid(ofParameter<int>).name()){
            gui->addSlider(absParam.cast<int>());
        }else if(absParam.type() == typeid(ofParameter<bool>).name()){
            ofxDatGuiToggle* toggle =  gui->addToggle(absParam.getName());
            toggle->setChecked(absParam.cast<bool>().get());
            //Add a listener that automatically puts the parameter to the gui.
            //Best will be to include this to datGui. But this way we can change the gui we use, independent of ofParameter
            parameterChangedListeners.push_back(absParam.cast<bool>().newListener([&, toggle](bool &val){
                toggle->setChecked(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<void>).name()){
            gui->addButton(absParam.getName());
        }else if(absParam.type() == typeid(ofParameter<string>).name()){
            auto textInput = gui->addTextInput(absParam.getName(), absParam.cast<string>());
            parameterChangedListeners.push_back(absParam.cast<string>().newListener([&, textInput](string &val){
                textInput->setText(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<char>).name()){
            gui->addLabel(absParam.getName());
        }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
            auto colorGui = gui->addColorPicker(absParam.getName(), absParam.cast<ofColor>());
            parameterChangedListeners.push_back(absParam.cast<ofColor>().newListener([&, colorGui](ofColor &val){
                colorGui->setColor(val);
            }));
        }else if(absParam.type() == typeid(ofParameterGroup).name()){
            gui->addLabel(absParam.castGroup().getName() + " Selector");
            auto dropdown = gui->addDropdown(absParam.castGroup().getName(), ofSplitString(absParam.castGroup().getString(0), "-|-"));
            dropdown->select(absParam.castGroup().getInt(1));
            parameterChangedListeners.push_back(absParam.castGroup().getInt(1).newListener([&, dropdown](int &val){
                dropdown->select(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            gui->addMultiSlider(absParam.cast<vector<float>>());
        }else {
            gui->addLabel(absParam.getName());
        }
    }
    
    //GUIS EVENT LISTERNERS
    gui->onButtonEvent(this, &ofxOceanodeNodeGui::onGuiButtonEvent);
    gui->onToggleEvent(this, &ofxOceanodeNodeGui::onGuiToggleEvent);
    gui->onDropdownEvent(this, &ofxOceanodeNodeGui::onGuiDropdownEvent);
    gui->onTextInputEvent(this, &ofxOceanodeNodeGui::onGuiTextInputEvent);
    gui->onColorPickerEvent(this, &ofxOceanodeNodeGui::onGuiColorPickerEvent);
    gui->onRightClickEvent(this, &ofxOceanodeNodeGui::onGuiRightClickEvent);
}

void ofxOceanodeNodeGui::updateGuiForParameter(string &parameterName){
    ofAbstractParameter &absParam = getParameters()->get(parameterName);
    if(absParam.type() == typeid(ofParameter<float>).name()){
        auto slider = gui->getSlider(absParam.getName());
        slider->setMin(absParam.cast<float>().getMin());
        slider->setMax(absParam.cast<float>().getMax());
    }else if(absParam.type() == typeid(ofParameter<int>).name()){
        auto slider = gui->getSlider(absParam.getName());
        slider->setMin(absParam.cast<int>().getMin());
        slider->setMax(absParam.cast<int>().getMax());
//    }else if(absParam.type() == typeid(ofParameterGroup).name()){
//        gui->addLabel(parameters->getGroup(i).getName());
//        gui->addDropdown(parameters->getGroup(i).getName(), ofSplitString(parameters->getGroup(i).getString(0), "-|-"))->select(parameters->getGroup(i).getInt(1));
    }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
        auto slider = gui->getMultiSlider(absParam.getName());
        slider->setMin(absParam.cast<vector<float>>().getMin()[0]);
        slider->setMax(absParam.cast<vector<float>>().getMax()[0]);
    }
}

ofParameterGroup* ofxOceanodeNodeGui::getParameters(){
    return node.getParameters();
}

void ofxOceanodeNodeGui::setPosition(glm::vec2 _position){
    gui->setPosition(_position.x, _position.y);
    position = _position;
}

void ofxOceanodeNodeGui::mouseDragged(ofMouseEventArgs &args){
    glm::vec2 guiCurrentPos = glm::vec2(gui->getPosition().x, gui->getPosition().y);
    if(guiCurrentPos != position){
        isDraggingGui = true;
        node.moveConnections(guiCurrentPos - position);
        position = guiCurrentPos;
    }else{
        isDraggingGui = false;
    }
}

void ofxOceanodeNodeGui::mouseReleased(ofMouseEventArgs &args){
    if(isDraggingGui && !ofGetWindowRect().inside(args)){
        node.deleteSelf();
    }
    isDraggingGui = false;
}

void ofxOceanodeNodeGui::onGuiButtonEvent(ofxDatGuiButtonEvent e){
    getParameters()->getVoid(e.target->getName()).trigger();
}
void ofxOceanodeNodeGui::onGuiToggleEvent(ofxDatGuiToggleEvent e){
    getParameters()->getBool(e.target->getName()) = e.checked;
}

void ofxOceanodeNodeGui::onGuiDropdownEvent(ofxDatGuiDropdownEvent e){
    getParameters()->getGroup(e.target->getName()).getInt(1) = e.child;
}

void ofxOceanodeNodeGui::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    getParameters()->getString(e.target->getName()) = e.text;
}

void ofxOceanodeNodeGui::onGuiColorPickerEvent(ofxDatGuiColorPickerEvent e){
    getParameters()->getColor(e.target->getName()) = e.color;
}

void ofxOceanodeNodeGui::onGuiRightClickEvent(ofxDatGuiRightClickEvent e){
    if(e.down == 1){
        node.parameterConnectionPress(container, getParameters()->get(e.target->getName()));
    }else{
        node.parameterConnectionRelease(container, getParameters()->get(e.target->getName()));
    }
}

glm::vec2 ofxOceanodeNodeGui::getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter){
    auto component = gui->getComponent(parameter.getName());
    glm::vec2 position;
    position.x = component->getX() + component->getWidth();
    position.y = component->getY() + component->getHeight()/2;
    return position;
}

glm::vec2 ofxOceanodeNodeGui::getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter){
    auto component = gui->getComponent(parameter.getName());
    glm::vec2 position;
    position.x = component->getX();
    position.y = component->getY() + component->getHeight()/2;
    return position;
}

