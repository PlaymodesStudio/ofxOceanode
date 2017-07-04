//
//  ofxOceanodeNodeRegistry.h
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#ifndef ofxOceanodeNodeRegistry_h
#define ofxOceanodeNodeRegistry_h

#include "ofxOceanodeNodeModel.h"

class ofxOceanodeNodeRegistry{
public:
    using registryItemPtr     = std::unique_ptr<ofxOceanodeNodeModel>;
    using registeredTypesMap = std::unordered_map<string, registryItemPtr>;
    
    ofxOceanodeNodeRegistry(){};
    ~ofxOceanodeNodeRegistry(){};
    
    
    template<typename NodeType>
    void registerType(std::unique_ptr<NodeType> uniqueType = std::make_unique<NodeType>()){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, NodeType>::value,
                      "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        const string name = uniqueType->name();
        
        if (registeredTypes.count(name) == 0)
        {
            registeredTypes[name] = std::move(uniqueType);
        }
    }
    
    std::unique_ptr<ofxOceanodeNodeModel> create(const string typeName);
    
    registeredTypesMap const & getRegisteredTypes();
private:
    registeredTypesMap registeredTypes;
    
};

#endif /* ofxOceanodeNodeRegistry_h */
