//
//  ofxOceanodeTypesRegistry.h
//  example-basic
//
//  Created by Eduard Frigola on 19/04/2018.
//

#ifndef ofxOceanodeTypesRegistry_h
#define ofxOceanodeTypesRegistry_h

#include "ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"
#include "router.h"

class ofxOceanodeTypesRegistry{
public:
    using registryCreator   = std::function<ofxOceanodeAbstractConnection*(ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active)>;
    using routerCreator     = std::function<shared_ptr<ofAbstractParameter>(ofxOceanodeNode* routerNode)>;
    using absParamCreator   = std::function<shared_ptr<ofxOceanodeAbstractParameter> (ofAbstractParameter &p)>;
    
    ofxOceanodeTypesRegistry();
    ~ofxOceanodeTypesRegistry(){};
    
    static shared_ptr<ofxOceanodeTypesRegistry> getInstance(){
        static shared_ptr<ofxOceanodeTypesRegistry> instance(new ofxOceanodeTypesRegistry);
        return instance;
    }
    
    template<typename T>
    void registerType(){
        registryCreator creator = [](ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active) -> ofxOceanodeAbstractConnection*
            {
                if(source.valueType() == typeid(T).name()){
                    if(sink.valueType() == typeid(T).name()){
                        return container.connectConnection(source.cast<T>(), sink.cast<T>(), active);
                    }
                    else{
                        return container.connectCustomConnection(source.cast<T>(), sink, active);
                    }
                }
                return nullptr;
            };
        
        registryColector.push_back(std::move(creator));
        
        routerCreator rCreator = [this](ofxOceanodeNode* routerNode) -> shared_ptr<ofAbstractParameter>{
            auto castedRouter = dynamic_cast<router<T>*>(&routerNode->getNodeModel());
            if(castedRouter != NULL){
                auto castedRouter = dynamic_cast<router<T>*>(&routerNode->getNodeModel());
                auto paramRef = dynamic_pointer_cast<ofParameter<T>>(castedRouter->getValue().newReference());
                auto param = make_shared<ofParameter<T>>();
                param->set(castedRouter->getNameParam().get(), paramRef->get(), castedRouter->getMin(), castedRouter->getMax());
                listeners.push(castedRouter->getMin().newListener([this, paramRef, param](T &val){
                    paramRef->setMin(val);
                    param->setMin(val);
                }));
                listeners.push(castedRouter->getMax().newListener([this, paramRef, param](T &val){
                    paramRef->setMax(val);
                    param->setMax(val);
                }));
                listeners.push(paramRef->newListener([this, paramRef, param](T &val){
                    param->set(paramRef->get());
                }));
                listeners.push(param->newListener([this, paramRef, param](T &val){
                    paramRef->set(param->get());
                }));
                auto &nameParam = castedRouter->getNameParam();
                listeners.push(nameParam.newListener([this, param, nameParam](string &s){
                    if(!param->setName(s)){
                        s = param->getName();
                    }
                }));
                return param;
            }
            return nullptr;
        };
        
        routerColector.push_back(std::move(rCreator));
        
        absParamCreator aPCreator = [](ofAbstractParameter &p) -> shared_ptr<ofxOceanodeAbstractParameter>{
            if(p.valueType() == typeid(T).name()){
                auto oceaParam = make_shared<ofxOceanodeParameter<T>>();
                oceaParam->bindParameter(p.cast<T>());
                return oceaParam;
            }
            return nullptr;
        };
        
        absParamColector.push_back(std::move(aPCreator));
    }
	
    ofxOceanodeAbstractConnection* createCustomTypeConnection(ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active = true);
    shared_ptr<ofAbstractParameter> createRouterFromType(ofxOceanodeNode* routerNode);
    shared_ptr<ofxOceanodeAbstractParameter> createOceanodeAbstractFromAbstract(ofAbstractParameter &p);
    
private:
    vector<registryCreator> registryColector;
    vector<routerCreator> routerColector;
    vector<absParamCreator> absParamColector;
    
    ofEventListeners listeners;
};

template<>
inline void ofxOceanodeTypesRegistry::registerType<void>(){
    registryCreator creator = [](ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active) -> ofxOceanodeAbstractConnection*
    {
        if(source.valueType() == typeid(void).name()){
            return container.connectConnection(source.cast<void>(), sink.cast<void>(), active);
        }
        return nullptr;
    };
    
    registryColector.push_back(std::move(creator));
    
    routerCreator rCreator = [this](ofxOceanodeNode* routerNode) -> shared_ptr<ofAbstractParameter>{
        auto castedRouter = dynamic_cast<router<void>*>(&routerNode->getNodeModel());
        if(castedRouter != NULL){
           auto castedRouter = dynamic_cast<router<void>*>(&routerNode->getNodeModel());
           auto paramRef = dynamic_pointer_cast<ofParameter<void>>(castedRouter->getValue().newReference());
           auto param = make_shared<ofParameter<void>>();
           param->set(castedRouter->getNameParam().get());
           listeners.push(paramRef->newListener([this, paramRef, param](){
               param->trigger();
           }));
           listeners.push(param->newListener([this, paramRef, param](){
               paramRef->trigger();
           }));
           listeners.push(castedRouter->getNameParam().newListener([this, param](string &s){
               param->setName(s);
           }));
           return param;
        }
        return nullptr;
    };
    
    routerColector.push_back(std::move(rCreator));
    
    absParamCreator aPCreator = [](ofAbstractParameter &p) -> shared_ptr<ofxOceanodeAbstractParameter>{
        if(p.valueType() == typeid(void).name()){
            auto oceaParam = make_shared<ofxOceanodeParameter<void>>();
            oceaParam->bindParameter(p.cast<void>());
            return oceaParam;
        }
        return nullptr;
    };
    
    absParamColector.push_back(std::move(aPCreator));
}

#endif /* ofxOceanodeTypesRegistry_h */
