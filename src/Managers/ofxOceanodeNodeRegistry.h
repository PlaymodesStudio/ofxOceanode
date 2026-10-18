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
#include "ofxOceanodeNodeHelpCatalog.h"

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
            modelDescriptions[name] = prototype->getDescription();
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
        modelDescriptions[name] = prototype->getDescription();
        return true;
    }

    // refreshModel for a caller that knows the node's name: no throwaway model
    // is constructed just to read it (refreshing hundreds of SynthDef models
    // built and destroyed one node each)
    template<typename ModelType, typename... Args>
    bool refreshModelNamed(string const& name, string const category, Args... args){
        static_assert(std::is_base_of<ofxOceanodeNodeModel, ModelType>::value, "Must pass a subclass of ofxOceanodeNodeModel");
        const auto existing = registeredModelsCategory.find(name);
        if(existing != registeredModelsCategory.end() && existing->second != category) return false;
        registeredModelCreators[name] = [args...](){ return std::make_unique<ModelType>(args...); };
        categories.insert(category);
        registeredModelsCategory[name] = category;
        modelDescriptions[name] = ofxOceanodeNodeHelpDescription(name);
        // Imported SynthDefs carry their description in a lightweight spec.
        // Preserve it without constructing potentially expensive live models.
        const std::vector<std::string> suppliedDescriptions { argumentDescription(args, 0)... };
        for(const auto& detail : suppliedDescriptions)
            if(!detail.empty()) modelDescriptions[name] = detail;
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
            modelDescriptions.erase(name);
        }
    }
    
    std::unique_ptr<ofxOceanodeNodeModel> create(const string typeName);
    
    registeredModelsMap const & getRegisteredModels();
    registeredModelsCategoryMap const & getRegisteredModelsCategoryAssociation();
    categoriesSet const & getCategories();
    const std::unordered_map<string, string>& getModelDescriptions() const { return modelDescriptions; }
    void setModelDescription(const string& name, const string& description) {
        if(!description.empty() && registeredModelCreators.count(name)) modelDescriptions[name] = description;
    }
private:
    template<typename T>
    static auto argumentDescription(const T& value, int) -> decltype(std::string(value.description)) {
        return std::string(value.description);
    }
    template<typename T>
    static std::string argumentDescription(const T&, long) { return ""; }
    std::unordered_map<string, string> modelDescriptions;
    registeredModelsCategoryMap registeredModelsCategory;
    categoriesSet categories;
    registeredModelsMap registeredModelCreators;
    
};

#endif /* ofxOceanodeNodeRegistry_h */
