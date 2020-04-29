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
    
    ofxOceanodeTypesRegistry();
    ~ofxOceanodeTypesRegistry(){};
    
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
    }
	
    ofxOceanodeAbstractConnection* createCustomTypeConnection(ofxOceanodeContainer &container, ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active = true);
    
private:
    vector<registryCreator> registryColector;
};

#endif /* ofxOceanodeTypesRegistry_h */
