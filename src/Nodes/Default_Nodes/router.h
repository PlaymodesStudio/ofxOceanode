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
        addParameter(nameParam.set("Name", typelabel));
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
protected:
    ofParameter<string> nameParam;
};

template<typename T>
class router : public abstractRouter{
public:
    router(string typelabel, T val, T _min, T _max) : abstractRouter(typelabel){
        addParameter(value.set("Value", val, _min, _max));
        addParameter(min.set("Min", _min, _min, _max));
        addParameter(max.set("Max", _max, _min, _max));
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

#endif /* router_h */
