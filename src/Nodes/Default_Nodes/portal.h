//
//  portal.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 14/06/2021.
//

#ifndef portal_h
#define portal_h

#include "ofxOceanodeNodeModel.h"

class abstractPortal : public ofxOceanodeNodeModel{
public:
	abstractPortal(string typelabel);
	~abstractPortal();
	
	string type(){
		return typeid(*this).name();
	}
	
	void portalUpdated();
	
	string getName(){
		return name;
	}
    
    bool isLocal(){return local;};
    
    bool checkLocal(abstractPortal* p){
        return (!isLocal() && !p->isLocal()) || (isLocal() && p->isLocal() && getParents() == p->getParents());
    }
	
	virtual bool match(abstractPortal* p) = 0;
    
    bool isMatching(abstractPortal *p){
        return type() == p->type() && getName() == p->getName() && checkLocal(p);
    }
	
protected:
	ofParameter<string> name;
    ofParameter<bool> local;
	ofEventListener listener;
    ofEventListener nameListener;
    bool settingViaMatch;
};

template<typename T>
class portal : public abstractPortal{
public:
	portal(string typelabel, T val, bool minmax = false) : abstractPortal(typelabel){
		if(minmax){
			addParameter(value.set("Value", val, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()), ofxOceanodeParameterFlags_DisplayMinimized);
		}else{
			addParameter(value.set("Value", val), ofxOceanodeParameterFlags_DisplayMinimized);
		}
	}
	
	void setup(){
		listener = value.newListener([this](T &t){
			portalUpdated();
		});
	}
	
	T getValue(){
		return value;
	}
	
	void setValue(T t){
		value = t;
	}
	
	bool match(abstractPortal* p){
		if(type() == p->type() && getName() == p->getName()){
            if(checkLocal(p))
            {
                settingViaMatch = true;
                setValue(dynamic_cast<portal<T>*>(p)->getValue());
                settingViaMatch = false;
                return true;
            }
		}
        return false;
	}
	
private:
	ofParameter<T> value;
};

template<typename T>
class portal<std::vector<T>> : public abstractPortal{
public:
	portal(string typelabel, T val, bool minmax = false) : abstractPortal(typelabel){
		if(minmax){
			addParameter(value.set("Value", std::vector<T>(1, val), std::vector<T>(1, std::numeric_limits<T>::lowest()), std::vector<T>(1, std::numeric_limits<T>::max())), ofxOceanodeParameterFlags_DisplayMinimized);
		}else{
			addParameter(value.set("Value", std::vector<T>(1, val)), ofxOceanodeParameterFlags_DisplayMinimized);
		}
	}
	
	portal(string typelabel, vector<T> val) : abstractPortal(typelabel){
		addParameter(value.set("Value", val), ofxOceanodeParameterFlags_DisplayMinimized);
	}
	
	void setup(){
		listener = value.newListener([this](std::vector<T> &t){
			portalUpdated();
		});
	}
	
	std::vector<T> getValue(){
		return value;
	}
	
	void setValue(std::vector<T> t){
		value = t;
	}
	
	bool match(abstractPortal* p){
		if(type() == p->type() && getName() == p->getName() && checkLocal(p)){
            settingViaMatch = true;
			setValue(dynamic_cast<portal<std::vector<T>>*>(p)->getValue());
            settingViaMatch = false;
            return true;
		}
        return false;
	}
	
private:
	ofParameter<std::vector<T>> value;
};

template<>
class portal<void> : public abstractPortal{
public:
	portal(string typelabel) : abstractPortal(typelabel){
		addParameter(value.set("Value"), ofxOceanodeParameterFlags_DisplayMinimized);
	}
	
	void setup(){
		listener = value.newListener([this](){
			portalUpdated();
		});
	}
	
	bool match(abstractPortal* p){
		if(type() == p->type() && getName() == p->getName() && checkLocal(p)){
            settingViaMatch = true;
			value.trigger();
            settingViaMatch = false;
            return true;
		}
        return false;
	}
private:
	ofParameter<void> value;
};

#endif /* portal_h */
