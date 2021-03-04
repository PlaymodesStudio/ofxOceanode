//
//  ofxOceanodeParameter.h
//  example-basic
//
//  Created by Eduard Frigola on 27/04/2020.
//	With ideas from: https://forum.openframeworks.cc/t/ofparametergroup-and-custom-inherited-ofparameter-class/30237/10

#ifndef ofxOceanodeParameter_h
#define ofxOceanodeParameter_h

#include "ofParameter.h"

class ofxOceanodeAbstractConnection;

class BaseConnectionFunction{
public:
    virtual ~BaseConnectionFunction() {}
    
    virtual void execute(ofAbstractParameter *p) = 0;
};

template <typename T>
class ConnectionFunction : public BaseConnectionFunction{
  typedef std::function<void(const T &val)> FuncType;
  
  public:
    ConnectionFunction() {}
    ConnectionFunction(FuncType f) : f_(f) {}
    void execute(ofAbstractParameter *p) {
        if(f_) f_(p->cast<T>().get());
    }
private:
    FuncType f_;
};

template <>
class ConnectionFunction<void> : public BaseConnectionFunction{
  typedef std::function<void()> FuncType;
  
  public:
    ConnectionFunction() {}
    ConnectionFunction(FuncType f) : f_(f) {}
    void execute(ofAbstractParameter *p) {
        if(f_) f_();
    }
private:
    FuncType f_;
};

typedef int ofxOceanodeParameterFlags;

enum ofxOceanodeParameterFlags_
{
	ofxOceanodeParameterFlags_None               	= 0,
	ofxOceanodeParameterFlags_DisableSavePreset     = 1 << 0,   // Parameter not stored in preset
	ofxOceanodeParameterFlags_DisableSaveProject  	= 1 << 1,   // Parameter not stored in project
	ofxOceanodeParameterFlags_DisableInConnection   = 1 << 2,   // Parameter cannot be connected from
	ofxOceanodeParameterFlags_DisableOutConnection  = 1 << 3   	// Parameter cannot connect to
//	ofxOceanodeParameterFlags_CollapsingHeader    = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog// Callback on pressing TAB (for completion handling)
};

template<typename ParameterType>
class ofxOceanodeParameter;

class ofxOceanodeAbstractParameter : public ofAbstractParameter{
public:
	ofxOceanodeAbstractParameter(){
		flags = 0;
        inConnection = nullptr;
        hasScope = false;
	};
    virtual ~ofxOceanodeAbstractParameter(){};
	
	virtual std::string getName() const = 0;
	virtual bool setName(const std::string & name) = 0;
	virtual std::string toString() const = 0;
	virtual void fromString(const std::string & str) = 0;
	
	virtual std::string valueType() const = 0;
	
	virtual void setParent(ofParameterGroup & _parent) = 0;
	
	template<typename ParameterType>
	ofxOceanodeParameter<ParameterType> & cast(){
		return static_cast<ofxOceanodeParameter<ParameterType> &>(*this);
	}
	
	template<typename ParameterType>
	const ofxOceanodeParameter<ParameterType> & cast() const{
		return static_cast<const ofxOceanodeParameter<ParameterType> &>(*this);
	}
	
	virtual bool isSerializable() const = 0;
	virtual bool isReadOnly() const = 0;
	virtual std::shared_ptr<ofAbstractParameter> newReference() const = 0;
    virtual ofEvent<std::string>& nameChangedEvent() = 0;
	
	ofxOceanodeParameterFlags getFlags(){return flags;};
	void setFlags(ofxOceanodeParameterFlags f){flags = f;};
    
    bool hasInConnection(){return inConnection != nullptr;};
    ofxOceanodeAbstractConnection* getInConnection(){return inConnection;};
    
    bool hasOutConnections(){return outConnections.size() != 0;};
    std::vector<ofxOceanodeAbstractConnection*> getOutConnections(){return outConnections;};
    
    void setInConnection(ofxOceanodeAbstractConnection* c){inConnection = c;};
    void addOutConnection(ofxOceanodeAbstractConnection* c){outConnections.push_back(c);};
    
    void removeInConnection(ofxOceanodeAbstractConnection* c){if(inConnection == c) inConnection = nullptr;};
    void removeOutConnection(ofxOceanodeAbstractConnection* c){outConnections.erase(std::remove(outConnections.begin(), outConnections.end(), c));};
    
    void removeAllConnections();
    
    bool isScoped(){return hasScope;};
    void setScoped(bool b){hasScope = b;};
    
    template <typename T>
    void addReceiveFunc(std::function<void(const T &val)> cmd){
       connnectionFunctions.insert(std::pair<std::string, std::shared_ptr<BaseConnectionFunction>>(typeid(T).name(), std::make_shared<ConnectionFunction<T>>(cmd)));
    }
    
    bool receiveParameter(ofAbstractParameter *p){
        auto it = connnectionFunctions.find(p->valueType());
        if(it != connnectionFunctions.end())
        {
            it->second->execute(p);
            return true;
        }
        return false;
    }
    
    void addConnectFunc(std::function<void()> func){
        if(!connectFunction) connectFunction = func;
    }
    
    void connectedParameter(){if(connectFunction) connectFunction();};
    
    void addDisconnectFunc(std::function<void()> func){
        if(!disconnectFunction) disconnectFunction = func;
    }
    
    void disconnectedParameter(){if(disconnectFunction) disconnectFunction();};
	
protected:
	virtual const ofParameterGroup getFirstParent() const = 0;
	virtual void setSerializable(bool serializable)=0;
	virtual const void* getInternalObject() const = 0;
	
private:
    ofxOceanodeAbstractConnection* inConnection;
    std::vector<ofxOceanodeAbstractConnection*> outConnections;
	ofxOceanodeParameterFlags flags;
    bool hasScope;
    
    std::map<std::string, std::shared_ptr<BaseConnectionFunction>> connnectionFunctions;
    std::function<void()> connectFunction;
    std::function<void()> disconnectFunction;
};

template<typename ParameterType>
class ofxOceanodeParameter: public ofxOceanodeAbstractParameter{
public:
	ofxOceanodeParameter(){
		parameter = nullptr;
        registerDragFunctions();
	};
    
	~ofxOceanodeParameter() {
        removeAllConnections();
    };
	
	//Overrided
	std::string getName() const { return parameter->getName();}
	bool setName(const std::string& name ) { return parameter->setName(name); }
	std::string toString() const { return parameter->toString();}
	void fromString(const std::string& str) { return parameter->fromString(str);}
	std::string valueType() const { return parameter->valueType(); };
	void setParent(ofParameterGroup &_parent) { parameter->setParent(_parent);}
	bool isSerializable() const { return parameter->isSerializable();}
	bool isReadOnly() const { return parameter->isReadOnly();}
	std::shared_ptr<ofAbstractParameter> newReference() const { return std::make_shared<ofxOceanodeParameter<ParameterType>>(*this);}
    ofEvent<std::string>& nameChangedEvent() {return parameter->nameChangedEvent();}
	
	//Custom
	void bindParameter(ofParameter<ParameterType> &p){
		//TODO: Throw exception
		if(parameter == nullptr){
			parameter = std::make_shared<ofParameter<ParameterType>>(p);
			defaultValue = p;
		}
	}
	ofParameter<ParameterType> & getParameter(){return *parameter;}
    
    void setDefaultValue(ParameterType v){defaultValue = v;};
	ParameterType getDefaultValue(){return defaultValue;};
	
	void setDropdownOptions(std::vector<std::string> op){dropdownOptions = op;};
	std::vector<std::string> getDropdownOptions(){return dropdownOptions;};
	
    void registerDragFunctions(){
        normalDrag = [](ofParameter<ParameterType> &p, int drag){};
        precisionDrag = [](ofParameter<ParameterType> &p, int drag){};
        speedDrag = [](ofParameter<ParameterType> &p, int drag){};
    };
    
	void registerNormalDrag(std::function<void(ofParameter<ParameterType> &p, int drag)> func){normalDrag = func;};
	void registerPrecisionDrag(std::function<void(ofParameter<ParameterType> &p, int drag)> func){precisionDrag = func;};
	void registerSpeedDrag(std::function<void(ofParameter<ParameterType> &p, int drag)> func){speedDrag = func;};
	
	void applyNormalDrag(int drag){normalDrag(*parameter.get(), drag);};
	void applyPrecisionDrag(int drag){precisionDrag(*parameter.get(), drag);};
	void applySpeedDrag(int drag){speedDrag(*parameter.get(), drag);};
	
protected:
	const ofParameterGroup getFirstParent() const { return parameter->getFirstParent();}
	void setSerializable(bool serializable) { parameter->setSerializable(serializable);}
	const void* getInternalObject() const { return parameter->getInternalObject();}
	
private:
	std::shared_ptr<ofParameter<ParameterType>> parameter;
	std::vector<std::string> dropdownOptions;
	ParameterType defaultValue;
	
	std::function<void(ofParameter<ParameterType> &p, int drag)> normalDrag;
	std::function<void(ofParameter<ParameterType> &p, int drag)> precisionDrag;
	std::function<void(ofParameter<ParameterType> &p, int drag)> speedDrag;
};

template<>
class ofxOceanodeParameter<void>: public ofxOceanodeAbstractParameter{
public:
	ofxOceanodeParameter(){};
	~ofxOceanodeParameter() {
        removeAllConnections();
    };
	
	//Overrided
	std::string getName() const { return parameter->getName();}
	bool setName(const std::string& name ) { return parameter->setName(name); }
	std::string toString() const { return parameter->toString();}
	void fromString(const std::string& str) { return parameter->fromString(str);}
	std::string valueType() const { return parameter->valueType(); };
	void setParent(ofParameterGroup &_parent) { parameter->setParent(_parent);}
	bool isSerializable() const { return parameter->isSerializable();}
	bool isReadOnly() const { return parameter->isReadOnly();}
	std::shared_ptr<ofAbstractParameter> newReference() const { return std::make_shared<ofxOceanodeParameter<void>>(*this);}
    ofEvent<std::string>& nameChangedEvent() {return parameter->nameChangedEvent();}
	
	//Custom
	void bindParameter(ofParameter<void> &p){
		parameter = std::make_shared<ofParameter<void>>(p);
	}
	ofParameter<void> & getParameter(){return *parameter;}
	
	ofxOceanodeParameterFlags getFlags();
	void setFlags(ofxOceanodeParameterFlags f){flags = f;};
	
protected:
	const ofParameterGroup getFirstParent() const { return parameter->getFirstParent();}
	void setSerializable(bool serializable) { parameter->setSerializable(serializable);}
	const void* getInternalObject() const { return parameter->getInternalObject();}
	
private:
	std::shared_ptr<ofParameter<void>> parameter;
	ofxOceanodeParameterFlags flags = 0;
};


#endif /* ofxOceanodeParameter_h */
