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
    using registeredModelsMap = std::unordered_map<string, registryItemPtr>;
    
    ofxOceanodeNodeRegistry();
    ~ofxOceanodeNodeRegistry(){};
    
    
    template<typename ModelType>
    void registerModel(std::unique_ptr<ModelType> uniqueModel = std::make_unique<ModelType>()){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value,
                      "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        const string name = uniqueModel->nodeName();
        
        if (registeredModels.count(name) == 0)
        {
            registeredModels[name] = std::move(uniqueModel);
        }
    }
    
    std::unique_ptr<ofxOceanodeNodeModel> create(const string typeName);
    
    registeredModelsMap const & getRegisteredModels();
private:
    registeredModelsMap registeredModels;
    
};

#endif /* ofxOceanodeNodeRegistry_h */
