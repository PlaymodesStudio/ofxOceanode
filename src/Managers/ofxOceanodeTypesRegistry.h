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

class ofxOceanodeTypesRegistry{
public:
    using registryCreator   = std::function<ofxOceanodeAbstractConnection*(ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active)>;
    using absParamCreator   = std::function<shared_ptr<ofxOceanodeAbstractParameter> (ofAbstractParameter &p)>;
    
    ofxOceanodeTypesRegistry();
    ~ofxOceanodeTypesRegistry(){};
    
    static ofxOceanodeTypesRegistry &getInstance(){
        static ofxOceanodeTypesRegistry instance;
        return instance;
    }
    
    template<typename T>
    void registerType(){
        registryCreator creator = [](ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active) -> ofxOceanodeAbstractConnection*
            {
                if(source.valueType() == typeid(T).name()){
                    return container.connectConnection(source.cast<T>(), sink.cast<T>(), active);
                }
                return nullptr;
            };
        
        registryColector.push_back(std::move(creator));
        
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
    shared_ptr<ofxOceanodeAbstractParameter> createOceanodeAbstractFromAbstract(ofAbstractParameter &p);
    
private:
    vector<registryCreator> registryColector;
    vector<absParamCreator> absParamColector;
};

#endif /* ofxOceanodeTypesRegistry_h */
