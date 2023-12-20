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
    using registryItemPtr             = std::unique_ptr<ofxOceanodeNodeModel>;
    using registryItemCloner          = std::function<registryItemPtr()>;
    using registeredModelsMap         = std::unordered_map<string, registryItemCloner>;
    using registeredModelsCategoryMap = std::unordered_map<string, string>;
    using categoriesSet               = std::set<string>;
    
    ofxOceanodeNodeRegistry();
    ~ofxOceanodeNodeRegistry(){};
    
    
    template<typename ModelType, typename... Args>
    void registerModel(string const category, Args... args){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value, "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        registryItemCloner cloner = [args...](){
            return std::make_unique<ModelType>(args...);
        };
        
        const string name = cloner()->nodeName();
        
        if (registeredModelCreators.count(name) == 0)
        {
            registeredModelCreators[name] = std::move(cloner);
            categories.insert(category);
            registeredModelsCategory[name] = category;
        }
    }
    
    template<typename ModelType, typename... Args>
    void unregisterModel(string const category, Args... args){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value, "Must pass a subclass of ofxOceanodeNodeModel to registerType");
        
        registryItemCloner cloner = [args...](){
            return std::make_unique<ModelType>(args...);
        };
        
        const string name = cloner()->nodeName();
        
        if (registeredModelCreators.count(name) == 1)
        {
            registeredModelCreators.erase(name);
            registeredModelsCategory.erase(name);
        }
    }
    
    std::unique_ptr<ofxOceanodeNodeModel> create(const string typeName);
    
    registeredModelsMap const & getRegisteredModels();
    registeredModelsCategoryMap const & getRegisteredModelsCategoryAssociation();
    categoriesSet const & getCategories();
private:
    registeredModelsCategoryMap registeredModelsCategory;
    categoriesSet categories;
    registeredModelsMap registeredModelCreators;
    
};

#endif /* ofxOceanodeNodeRegistry_h */
