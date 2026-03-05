//
//  router.h
//  example-basic
//
//  Created by Eduard Frigola on 21/04/2020.
//

#ifndef router_h
#define router_h

#include "ofxOceanodeNodeModel.h"

class abstractRouter : public ofxOceanodeNodeModel{
public:
    abstractRouter(string typelabel) : ofxOceanodeNodeModel("Router " + typelabel){
        description = "To Send " + typelabel + " values to and from a macro and it's parent";
        addParameter(nameParam.set("Name", typelabel));
        addInspectorParameter(snapshotExclude.set("Exclude From Snapshots", false));
    };
    ~abstractRouter(){};
    
    ofParameter<string>& getNameParam(){
        return nameParam;
    }
    
    void loadBeforeConnections(ofJson &json){
        ofDeserialize(json, nameParam);
    }
    
    string type(){
        return typeid(*this).name();
    }
    
    bool isExcludeFromSnapshot(){return snapshotExclude;}
protected:
    ofParameter<string> nameParam;
    ofParameter<bool> snapshotExclude;
};

template<typename T>
class router : public abstractRouter{
public:
    router(string typelabel, T val, T _min, T _max) : abstractRouter(typelabel){
        addParameter(value.set("Value", val, _min, _max));
        addParameter(min.set("Min", _min, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
        addParameter(max.set("Max", _max, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
    };
    
    router(string typelabel, T val) : abstractRouter(typelabel){
        addParameter(value.set("Value", val));
    };
    
    ofParameter<T> &getValue(){return value;};
    ofParameter<T> &getMin(){return min;};
    ofParameter<T> &getMax(){return max;};
    
protected:
    ofParameter<T> value;
    ofParameter<T> min;
    ofParameter<T> max;
};

template<typename T>
class router<std::vector<T>> : public abstractRouter{
public:
    router(string typelabel, T val, T _min, T _max) : abstractRouter(typelabel){
        addParameter(value.set("Value", std::vector<T>(1, val), std::vector<T>(1, _min), std::vector<T>(1, _max)));
        addParameter(min.set("Min", std::vector<T>(1, _min), std::vector<T>(1, std::numeric_limits<T>::lowest()), std::vector<T>(1, std::numeric_limits<T>::max())));
        addParameter(max.set("Max", std::vector<T>(1, _max), std::vector<T>(1, std::numeric_limits<T>::lowest()), std::vector<T>(1, std::numeric_limits<T>::max())));
    };
    
    router(string typelabel, T val) : abstractRouter(typelabel){
        addParameter(value.set("Value", std::vector<T>(1, val)));
    };
    
    router(string typelabel, vector<T> val) : abstractRouter(typelabel){
        addParameter(value.set("Value", val));
    };
    
    ofParameter<std::vector<T>> &getValue(){return value;};
    ofParameter<std::vector<T>> &getMin(){return min;};
    ofParameter<std::vector<T>> &getMax(){return max;};
    
protected:
    ofEventListeners listeners;
    ofParameter<std::vector<T>> value;
    ofParameter<std::vector<T>> min;
    ofParameter<std::vector<T>> max;
};

template<>
class router<void> : public abstractRouter{
public:
    router(string typelabel) : abstractRouter(typelabel){
        addParameter(value.set("Val"));
    };
    
    ofParameter<void> &getValue(){return value;};
    
protected:
    ofParameter<void> value;
};

class routerDropdown : public abstractRouter {
public:
    routerDropdown() : abstractRouter("Dropdown") {
        addParameterDropdown(value, "Value", 0, {"..."});
        addParameter(options.set("Options", {}));
        optionsListener = options.newListener([this](vector<string> &opts) {
            if(opts.empty()) return;
            getOceanodeParameter(value).setDropdownOptions(opts);
            value.setMin(0);
            value.setMax((int)opts.size() - 1);
            if(value.get() >= (int)opts.size()) value = 0;
        });
    };

    ofParameter<int> &getValue() { return value; }
    ofParameter<int> &getMin() { return dummyMin; }
    ofParameter<int> &getMax() { return dummyMax; }
    vector<string> getCurrentOptions() { return options.get(); }

private:
    ofParameter<int> value;
    ofParameter<vector<string>> options;
    // router<int> interface compatibility — unused, but needed if createRouterFromType casts
    ofParameter<int> dummyMin{ofParameter<int>("Min", 0, 0, 0)};
    ofParameter<int> dummyMax{ofParameter<int>("Max", 0, 0, 0)};
    ofEventListener optionsListener;
};

#endif /* router_h */
