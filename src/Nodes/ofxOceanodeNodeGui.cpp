//
//  ofxOceanodeNodeGui.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node, shared_ptr<ofAppBaseWindow> window) : container(_container), node(_node){
    color = node.getColor();
    color.setBrightness(255);
    position = glm::vec2(10, 10);
    guiToBeDestroyed = false;
    lastExpandedState = true;
#ifdef OFXOCEANODE_USE_MIDI
    isListeningMidi = false;
#endif
    
    
    createGuiFromParameters(window);
    if(window == nullptr){
        keyAndMouseListeners.push(ofEvents().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
        keyAndMouseListeners.push(ofEvents().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
        keyAndMouseListeners.push(ofEvents().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
        keyAndMouseListeners.push(ofEvents().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
        keyAndMouseListeners.push(ofEvents().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
        keyAndMouseListeners.push(ofEvents().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
        keyAndMouseListeners.push(ofEvents().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
        keyAndMouseListeners.push(ofEvents().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
        keyAndMouseListeners.push(ofEvents().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }else{
        keyAndMouseListeners.push(window->events().keyPressed.newListener(this,&ofxOceanodeNodeGui::keyPressed));
        keyAndMouseListeners.push(window->events().keyReleased.newListener(this,&ofxOceanodeNodeGui::keyReleased));
        keyAndMouseListeners.push(window->events().mouseDragged.newListener(this,&ofxOceanodeNodeGui::mouseDragged));
        keyAndMouseListeners.push(window->events().mouseMoved.newListener(this,&ofxOceanodeNodeGui::mouseMoved));
        keyAndMouseListeners.push(window->events().mousePressed.newListener(this,&ofxOceanodeNodeGui::mousePressed));
        keyAndMouseListeners.push(window->events().mouseReleased.newListener(this,&ofxOceanodeNodeGui::mouseReleased));
        keyAndMouseListeners.push(window->events().mouseScrolled.newListener(this,&ofxOceanodeNodeGui::mouseScrolled));
        keyAndMouseListeners.push(window->events().mouseEntered.newListener(this,&ofxOceanodeNodeGui::mouseEntered));
        keyAndMouseListeners.push(window->events().mouseExited.newListener(this,&ofxOceanodeNodeGui::mouseExited));
    }
}

ofxOceanodeNodeGui::~ofxOceanodeNodeGui(){
    
}

void ofxOceanodeNodeGui::createGuiFromParameters(shared_ptr<ofAppBaseWindow> window){
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    gui = make_unique<ofxDatGui>(0, 0, window);
    
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
            parameterChangedListeners.push(absParam.cast<bool>().newListener([&, toggle](bool &val){
                toggle->setChecked(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<void>).name()){
            gui->addButton(absParam.getName());
        }else if(absParam.type() == typeid(ofParameter<string>).name()){
            auto textInput = gui->addTextInput(absParam.getName(), absParam.cast<string>());
            parameterChangedListeners.push(absParam.cast<string>().newListener([&, textInput](string &val){
                textInput->setText(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<char>).name()){
            gui->addLabel(absParam.getName());
        }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
            auto colorGui = gui->addColorPicker(absParam.getName(), absParam.cast<ofColor>());
            parameterChangedListeners.push(absParam.cast<ofColor>().newListener([&, colorGui](ofColor &val){
                colorGui->setColor(val);
            }));
        }else if(absParam.type() == typeid(ofParameterGroup).name()){
            gui->addLabel(absParam.castGroup().getName());
            auto dropdown = gui->addDropdown(absParam.castGroup().getName(), ofSplitString(absParam.castGroup().getString(0), "-|-"));
            dropdown->select(absParam.castGroup().getInt(1));
            parameterChangedListeners.push(absParam.castGroup().getInt(1).newListener([&, dropdown](int &val){
                dropdown->select(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            gui->addMultiSlider(absParam.cast<vector<float>>())->setPrecision(1000);
        }else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
            gui->addMultiSlider(absParam.cast<vector<int>>());
        }else if(absParam.type() == typeid(ofParameter<pair<int, bool>>).name()){
            auto pairParam = absParam.cast<pair<int, bool>>();
            auto matrix = gui->addMatrix(absParam.getName(), pairParam.get().first, true);
            matrix->setRadioMode(true);
            matrix->setHoldMode(pairParam.get().second);
        }else {
            gui->addLabel(absParam.getName());
        }
    }
    
    gui->addHeader(getParameters()->getName());
    gui->addFooter();
    
    ofxDatGuiTheme* theme = new ofxDatGuiThemeCharcoal;
    theme->color.slider.fill = color;
    theme->color.textInput.text = color;
    theme->color.icons = color;
    theme->layout.width = 290;
    gui->setTheme(theme, true);
   
    if(position == glm::vec2(-1, -1)){
        gui->setPosition(0, 0);
    }else{
        gui->setPosition(position.x, position.y);
    }
    
    //GUIS EVENT LISTERNERS
    gui->onButtonEvent(this, &ofxOceanodeNodeGui::onGuiButtonEvent);
    gui->onToggleEvent(this, &ofxOceanodeNodeGui::onGuiToggleEvent);
    gui->onDropdownEvent(this, &ofxOceanodeNodeGui::onGuiDropdownEvent);
    gui->onTextInputEvent(this, &ofxOceanodeNodeGui::onGuiTextInputEvent);
    gui->onColorPickerEvent(this, &ofxOceanodeNodeGui::onGuiColorPickerEvent);
    gui->onMatrixEvent(this, &ofxOceanodeNodeGui::onGuiMatrixEvent);
    gui->onRightClickEvent(this, &ofxOceanodeNodeGui::onGuiRightClickEvent);
}

void ofxOceanodeNodeGui::updateGui(){
    for(int i=0 ; i<getParameters()->size(); i++){
        ofAbstractParameter &absParam = getParameters()->get(i);
        if(gui->getComponent(absParam.getName()) == NULL){
            if(absParam.type() == typeid(ofParameter<float>).name()){
                gui->addSlider(absParam.cast<float>())->setPrecision(1000);
            }else if(absParam.type() == typeid(ofParameter<int>).name()){
                gui->addSlider(absParam.cast<int>());
            }else if(absParam.type() == typeid(ofParameter<bool>).name()){
                ofxDatGuiToggle* toggle =  gui->addToggle(absParam.getName());
                toggle->setChecked(absParam.cast<bool>().get());
                //Add a listener that automatically puts the parameter to the gui.
                //Best will be to include this to datGui. But this way we can change the gui we use, independent of ofParameter
                parameterChangedListeners.push(absParam.cast<bool>().newListener([&, toggle](bool &val){
                    toggle->setChecked(val);
                }));
            }else if(absParam.type() == typeid(ofParameter<void>).name()){
                gui->addButton(absParam.getName());
            }else if(absParam.type() == typeid(ofParameter<string>).name()){
                auto textInput = gui->addTextInput(absParam.getName(), absParam.cast<string>());
                parameterChangedListeners.push(absParam.cast<string>().newListener([&, textInput](string &val){
                    textInput->setText(val);
                }));
            }else if(absParam.type() == typeid(ofParameter<char>).name()){
                gui->addLabel(absParam.getName());
            }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
                auto colorGui = gui->addColorPicker(absParam.getName(), absParam.cast<ofColor>());
                parameterChangedListeners.push(absParam.cast<ofColor>().newListener([&, colorGui](ofColor &val){
                    colorGui->setColor(val);
                }));
            }else if(absParam.type() == typeid(ofParameterGroup).name()){
                gui->addLabel(absParam.castGroup().getName());
                auto dropdown = gui->addDropdown(absParam.castGroup().getName(), ofSplitString(absParam.castGroup().getString(0), "-|-"));
                dropdown->select(absParam.castGroup().getInt(1));
                parameterChangedListeners.push(absParam.castGroup().getInt(1).newListener([&, dropdown](int &val){
                    dropdown->select(val);
                }));
            }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
                gui->addMultiSlider(absParam.cast<vector<float>>())->setPrecision(1000);
            }else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
                gui->addMultiSlider(absParam.cast<vector<int>>());
            }else {
                gui->addLabel(absParam.getName());
            }
        }
    }
    bool removedComponents = false;
    for(int i = 0; i < gui->getNumComponents();){
        if(gui->getComponent(i) != nullptr){
            if(!getParameters()->contains(gui->getComponent(i)->getName())){
                gui->removeComponent(i);
                removedComponents = true;
            }
            else{
                i++;
            }
        }else{
            i++;
        }
    }
    if(removedComponents){
        for(int i = 0; i < getParameters()->size(); i++){
            auto &p = getParameters()->get(i);
            node.setInConnectionsPositionForParameter(p, getSinkConnectionPositionFromParameter(p));
            node.setOutConnectionsPositionForParameter(p, getSourceConnectionPositionFromParameter(p));
        }
    }
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
    }else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
        auto slider = gui->getMultiSlider(absParam.getName());
        slider->setMin(absParam.cast<vector<int>>().getMin()[0]);
        slider->setMax(absParam.cast<vector<int>>().getMax()[0]);
    }
}

void ofxOceanodeNodeGui::updateDropdown(string &dropdownName){
    auto dropdown = gui->getDropdown(dropdownName);
    dropdown->clear();
    for(auto option : ofSplitString(getParameters()->getGroup(dropdownName).getString(0), "-|-")){
        dropdown->addOption(option);
    }
}

ofParameterGroup* ofxOceanodeNodeGui::getParameters(){
    return node.getParameters();
}

void ofxOceanodeNodeGui::setPosition(glm::vec2 _position){
    gui->setPosition(_position.x, _position.y);
    node.moveConnections(_position - position);
    position = _position;
}

glm::vec2 ofxOceanodeNodeGui::getPosition(){
    return glm::vec2(gui->getPosition().x, gui->getPosition().y);
}

void ofxOceanodeNodeGui::keyPressed(ofKeyEventArgs &args){
    if(args.key == 'r' && !args.isRepeat){
        if(gui->hitTest(ofVec2f(ofGetMouseX(), ofGetMouseY()))){
            guiToBeDestroyed = true;
            gui->setOpacity(0.2);
        }
    }
}

void ofxOceanodeNodeGui::keyReleased(ofKeyEventArgs &args){
    if(args.key == 'r'){
        guiToBeDestroyed = false;
        gui->setOpacity(1);
    }
}

void ofxOceanodeNodeGui::mouseDragged(ofMouseEventArgs &args){
    glm::vec2 guiCurrentPos = glm::vec2(gui->getPosition().x, gui->getPosition().y);
    if(guiCurrentPos != position){
        node.moveConnections(guiCurrentPos - position);
        position = guiCurrentPos;
    }
}

void ofxOceanodeNodeGui::mousePressed(ofMouseEventArgs &args){
    if(gui->hitTest(args) && args.button != OF_MOUSE_BUTTON_RIGHT){
       if(args.hasModifier(OF_KEY_ALT)){
           node.duplicateSelf(toGlm(gui->getPosition() + ofPoint(gui->getWidth() + 10, 0)));
       }
       else if(guiToBeDestroyed){
           node.deleteSelf();
       }
    }
}

void ofxOceanodeNodeGui::mouseReleased(ofMouseEventArgs &args){
    if(gui->hitTest(args)){
        if(!gui->getExpanded() && lastExpandedState){
            auto header = gui->getHeader();
            node.collapseConnections(glm::vec2(header->getX(), header->getY() + header->getHeight()/2), glm::vec2(header->getX() + header->getWidth(), header->getY() + header->getHeight()/2));
            lastExpandedState = gui->getExpanded();
        }
        else if(gui->getExpanded() && !lastExpandedState){
            node.expandConnections();
            lastExpandedState = gui->getExpanded();
        }
    }
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

void ofxOceanodeNodeGui::onGuiMatrixEvent(ofxDatGuiMatrixEvent e){
    getParameters()->get(e.target->getName()).cast<pair<int, bool>>() = make_pair(e.child+1, e.enabled);
}

void ofxOceanodeNodeGui::onGuiRightClickEvent(ofxDatGuiRightClickEvent e){
    ofAbstractParameter *p = &getParameters()->get(e.target->getName());
    if(e.target->getType() == ofxDatGuiType::DROPDOWN){
        p = &getParameters()->getGroup(e.target->getName()).getInt(1);
    }
#ifdef OFXOCEANODE_USE_MIDI
    if(isListeningMidi){
        if(e.down == 1){
            if(ofGetKeyPressed(OF_KEY_SHIFT)){
                container.removeMidiBinding(*p);
            }else{
                container.createMidiBinding(*p);
            }
        }
    }
    else
#endif
    {
        if(e.down == 1){
            auto connection = node.parameterConnectionPress(container, *p);
            if(connection != nullptr){
                connection->setTransformationMatrix(transformationMatrix);
                connection->setSinkPosition(glm::inverse(transformationMatrix->get()) * glm::vec4(ofGetMouseX(), ofGetMouseY(), 0, 1));
            }
        }else{
            auto connection = node.parameterConnectionRelease(container, *p);
            if(connection != nullptr){
                connection->setSinkPosition(getSinkConnectionPositionFromParameter(getParameters()->get(e.target->getName())));
                connection->setTransformationMatrix(transformationMatrix);
            }
        }
    }
}

glm::vec2 ofxOceanodeNodeGui::getSourceConnectionPositionFromParameter(ofAbstractParameter& parameter){
    auto component = gui->getComponent(parameter.getName());
    if(component == NULL){
        component = gui->getComponent(parameter.getName() + " Selector");
    }
    glm::vec2 position;
    position.x = component->getX() + component->getWidth();
    position.y = component->getY() + component->getHeight()/2;
    return position;
}

glm::vec2 ofxOceanodeNodeGui::getSinkConnectionPositionFromParameter(ofAbstractParameter& parameter){
    auto component = gui->getComponent(parameter.getName());
    if(component == NULL){
        component = gui->getComponent(parameter.getName() + " Selector");
    }
    glm::vec2 position;
    position.x = component->getX();
    position.y = component->getY() + component->getHeight()/2;
    return position;
}

void ofxOceanodeNodeGui::setTransformationMatrix(ofParameter<glm::mat4> *mat){
    transformationMatrix = mat;
    gui->setTransformMatrix(ofMatrix4x4(mat->get()));
    transformMatrixListener = transformationMatrix->newListener([&](glm::mat4 &m){
        gui->setTransformMatrix(ofMatrix4x4(transformationMatrix->get()));
    });
}
