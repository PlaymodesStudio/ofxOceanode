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
        
        const auto prototype = cloner();
        const string name = prototype->nodeName();
        
        if (registeredModelCreators.count(name) == 0)
        {
            registeredModelCreators[name] = std::move(cloner);
            categories.insert(category);
            registeredModelsCategory[name] = category;
            macroMixerBlockers[name] = prototype->macroMixerBlocker();
        }
    }
    
    // Refresh factories for imported effects without disturbing running nodes
    // or a model registered by another category with the same name.
    template<typename ModelType, typename... Args>
    bool refreshModel(string const category, Args... args){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value, "Must pass a subclass of ofxOceanodeNodeModel");
        registryItemCloner cloner = [args...](){ return std::make_unique<ModelType>(args...); };
        const auto prototype = cloner();
        const string name = prototype->nodeName();
        const auto existing = registeredModelsCategory.find(name);
        if(existing != registeredModelsCategory.end() && existing->second != category) return false;
        registeredModelCreators[name] = std::move(cloner);
        categories.insert(category);
        registeredModelsCategory[name] = category;
        macroMixerBlockers[name] = prototype->macroMixerBlocker();
        return true;
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
            macroMixerBlockers.erase(name);
        }
    }
    
    std::unique_ptr<ofxOceanodeNodeModel> create(const string typeName);
    
    registeredModelsMap const & getRegisteredModels();
    registeredModelsCategoryMap const & getRegisteredModelsCategoryAssociation();
    categoriesSet const & getCategories();
    // Capture the model's declaration during normal registration so mixer
    // analysis never needs to construct a resource-owning node to exclude it.
    std::string macroMixerBlocker(const std::string& name) const {
        const auto found = macroMixerBlockers.find(name);
        return found == macroMixerBlockers.end() ? std::string() : found->second;
    }
    // A copied registry can constrain embedded Macro factories without changing
    // the host registry. Nested native Macros inherit the same registry.
    void setCreationGuard(std::function<void(const std::string&)> guard) { creationGuard = std::move(guard); }
    void checkCanCreate(const std::string& model) const { if(creationGuard) creationGuard(model); }
    void setFolderLoadGuard(std::function<void(const std::string&)> guard) { folderLoadGuard = std::move(guard); }
    void checkFolderLoad(const std::string& path) const { if(folderLoadGuard) folderLoadGuard(path); }
private:
    std::function<void(const std::string&)> creationGuard, folderLoadGuard;
    std::unordered_map<std::string, std::string> macroMixerBlockers;
    registeredModelsCategoryMap registeredModelsCategory;
    categoriesSet categories;
    registeredModelsMap registeredModelCreators;
    
};

#endif /* ofxOceanodeNodeRegistry_h */
