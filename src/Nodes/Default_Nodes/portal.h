//
//  portal.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 14/06/2021.
//

#ifndef portal_h
#define portal_h

#include "ofxOceanodeNodeModel.h"
#include <typeindex>

class abstractPortal : public ofxOceanodeNodeModel{
public:
	abstractPortal(string typelabel);
	~abstractPortal();
	
	std::type_index type() const {
		return std::type_index(typeid(*this));
	}
	
	void portalUpdated();
	
	string getName(){
		return name;
	}
    
    bool isLocal(){return local;};
    
    bool checkLocal(abstractPortal* p){
        // getParents() returns the cached canvasID string — already O(1)
        return (!isLocal() && !p->isLocal()) || (isLocal() && p->isLocal() && getParents() == p->getParents());
    }
	
	virtual bool match(abstractPortal* p) = 0;
	virtual void resendValue() = 0;
    
    bool isMatching(abstractPortal *p){
        // Name equality is guaranteed by the map-based lookup in ofxOceanodeShared
        return type() == p->type() && getName() == p->getName() && checkLocal(p);
    }
    
    void activate() override;
    
    void presetHasLoaded() override;
	
protected:
	ofParameter<string> name;
    ofParameter<bool> local;
	ofParameter<bool> resendOnNameChange;
	ofEventListener listener;
    ofEventListener nameListener;
    ofEventListener localListener;
    bool settingViaMatch;
    std::string currentName; // Tracks current portal name for map-bucket migration
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
	
	void setup() override {
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
	
	void resendValue() override {
		if(!getOceanodeParameter(value).hasOutConnections()) {
			portalUpdated();
		}
	}
	
	bool match(abstractPortal* p) override {
		// getName() == p->getName() is guaranteed by the map-based registry in ofxOceanodeShared
		if(type() == p->type() && p != this){
	           if(checkLocal(p))
	           {
	               settingViaMatch = true;
	               // static_cast is safe: type() equality already verified
	               setValue(static_cast<portal<T>*>(p)->getValue());
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
	
	void resendValue() override {
		if(!getOceanodeParameter(value).hasOutConnections()) {
			portalUpdated();
		}
	}
	
	bool match(abstractPortal* p){
		// getName() == p->getName() is guaranteed by the map-based registry in ofxOceanodeShared
		if(type() == p->type() && checkLocal(p) && p != this){
	           settingViaMatch = true;
			// static_cast is safe: type() equality already verified
			setValue(static_cast<portal<std::vector<T>>*>(p)->getValue());
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
	
	void trigger() {
		value.trigger();
	}

	void resendValue() override {
		if(!getOceanodeParameter(value).hasOutConnections()) {
			value.trigger();
		}
	}

	bool match(abstractPortal* p){
		// getName() == p->getName() is guaranteed by the map-based registry in ofxOceanodeShared
		if(type() == p->type() && checkLocal(p) && p != this){
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
