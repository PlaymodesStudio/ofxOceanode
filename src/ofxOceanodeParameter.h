//
//  ofxOceanodeParameter.h
//  example-basic
//
//  Created by Eduard Frigola on 27/04/2020.
//	With ideas from: https://forum.openframeworks.cc/t/ofparametergroup-and-custom-inherited-ofparameter-class/30237/10

#ifndef ofxOceanodeParameter_h
#define ofxOceanodeParameter_h

#include "ofParameter.h"
#include "Profiling/ofxOceanodeParameterProfiling.h"

class ofxOceanodeAbstractConnection;
class ofxOceanodeNodeModel;

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

#define OFXOCEANODE_HAS_HEADER_TOGGLE 1

typedef int ofxOceanodeParameterFlags;

enum ofxOceanodeParameterFlags_
{
	ofxOceanodeParameterFlags_None               	= 0,
	ofxOceanodeParameterFlags_DisableSavePreset     = 1 << 0,   // Parameter not stored in preset
	ofxOceanodeParameterFlags_DisableSaveProject  	= 1 << 1,   // Parameter not stored in project
	ofxOceanodeParameterFlags_DisableInConnection   = 1 << 2,   // Parameter cannot be connected from
	ofxOceanodeParameterFlags_DisableOutConnection  = 1 << 3,   // Parameter cannot connect to
    ofxOceanodeParameterFlags_DisplayMinimized      = 1 << 4,   // Scope Parameter when node minimized
    ofxOceanodeParameterFlags_ScopeKeepAspectRatio  = 1 << 5,   // Scope Parameter keeps Aspect Ratio
    ofxOceanodeParameterFlags_ReadOnly              = 1 << 6,   // Parameter cannot be interacted
    ofxOceanodeParameterFlags_NoGuiWidget           = 1 << 7,   // Parameter hidden from node GUI (pin still exists)
    ofxOceanodeParameterFlags_HeaderToggle          = 1 << 8    // Boolean checkbox in the node header, with ordinary pins
//	ofxOceanodeParameterFlags_CollapsingHeader    = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog// Callback on pressing TAB (for completion handling)
};

template<typename ParameterType>
class ofxOceanodeParameter;

class ofxOceanodeAbstractParameter : public ofAbstractParameter{
public:
	ofxOceanodeAbstractParameter(){
		flags = 0;
        hasScope = false;
        hasTimeline = false;
	};
    virtual ~ofxOceanodeAbstractParameter();

    // Only routers and their macro pins relay dropdown metadata. Ordinary
    // parameters are boundaries, so discovery never crosses a processing node.
    virtual std::vector<std::string> getDropdownOptions(){return {};}
    void setDropdownRelay(bool relay){
        if(dropdownRelay != relay){ dropdownRelay = relay; touchDropdownGraph(); }
    }
    void setDropdownSourceAvailable(bool available){
        if(dropdownSourceAvailable != available){ dropdownSourceAvailable = available; touchDropdownGraph(); }
    }
    void linkDropdownPeer(ofxOceanodeAbstractParameter& peer);
    std::vector<ofxOceanodeAbstractParameter*> getConnectedDropdownSources();
    static uint64_t getDropdownGraphRevision(){return dropdownGraphRevision();}
    static void touchDropdownGraph(){++dropdownGraphRevision();}
	
	virtual std::string getName() const = 0;
	virtual bool setName(const std::string & name) = 0;
	virtual std::string toString() const = 0;
	virtual void fromString(const std::string & str) = 0;
	
    virtual std::string type() const = 0;
    virtual std::string getEscapedName() const = 0;
	virtual std::string valueType() const = 0;
    
    virtual bool isInit() const = 0;
	
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
    virtual bool isReferenceTo(const ofAbstractParameter & other) const = 0;
    virtual ofEvent<std::string>& nameChangedEvent() = 0;
	
	ofxOceanodeParameterFlags getFlags(){return flags;};
	void setFlags(ofxOceanodeParameterFlags f){
        flags = f;
        if(profilingHook) profilingHook->configurePublication(
            !(f & ofxOceanodeParameterFlags_DisableOutConnection), !(f & ofxOceanodeParameterFlags_DisableInConnection));
    };
    
    bool hasInConnection(){return !inConnections.empty();};
    ofxOceanodeAbstractConnection* getInConnection(){return inConnections.empty() ? nullptr : inConnections.back();};
    const std::vector<ofxOceanodeAbstractConnection*>& getInConnections() const {return inConnections;}

    // Opt-in by custom value type. Ordinary numeric/texture ports keep their
    // existing single-input behavior; the SC addon registers only ScBus.
    using InputCombiner = std::function<void(ofxOceanodeAbstractParameter&,
        const std::vector<ofxOceanodeAbstractParameter*>&, std::shared_ptr<void>&)>;
    static void registerInputCombiner(const std::string& type, InputCombiner combine){
        inputCombiners()[type] = std::move(combine);
    }
    bool allowsMultipleInputs() const {return inputCombiners().count(valueType()) != 0;}
    bool acceptsMultipleInputFrom(const ofxOceanodeAbstractParameter& source) const {
        return allowsMultipleInputs() && source.valueType() == valueType();
    }
    bool refreshCombinedInput();
    
    bool hasOutConnections(){return outConnections.size() != 0;};
    std::vector<ofxOceanodeAbstractConnection*> getOutConnections(){return outConnections;};
    
    void setInConnection(ofxOceanodeAbstractConnection* c){inConnections.push_back(c); touchDropdownGraph();};
    void addOutConnection(ofxOceanodeAbstractConnection* c){outConnections.push_back(c); touchDropdownGraph();};
    
    void removeInConnection(ofxOceanodeAbstractConnection* c){
        inConnections.erase(std::remove(inConnections.begin(), inConnections.end(), c), inConnections.end());
        touchDropdownGraph();
    };
    void removeOutConnection(ofxOceanodeAbstractConnection* c){outConnections.erase(std::remove(outConnections.begin(), outConnections.end(), c)); touchDropdownGraph();};
    
    void removeAllConnections();
    
    bool isScoped(){return hasScope;};
    void setScoped(bool b){hasScope = b;};
    
    bool isTimelined(){return hasTimeline;};
    void setTimelined(bool b){hasTimeline = b;};
    
    template <typename T>
    void addReceiveFunc(std::function<void(const T &val)> cmd){
       connnectionFunctions.insert(std::pair<std::string, std::shared_ptr<BaseConnectionFunction>>(typeid(T).name(), std::make_shared<ConnectionFunction<T>>(cmd)));
    }
    
    bool receiveParameter(ofAbstractParameter *p){
        oceanodeProfiling::Scope timing(profilingRecord, oceanodeProfiling::Phase::Parameters);
        auto it = connnectionFunctions.find(p->valueType());
        if(it != connnectionFunctions.end())
        {
            it->second->execute(p);
            return true;
        }
        // A type this parameter cannot take goes on to what it feeds; the
        // connection is only made if one of those takes it.
        if(forwardTargets){
            // Portals can feed another portal in the same matching group, and
            // routers can form loops. Stop only the current cyclic branch so
            // other branches (including shared downstream sinks) still run.
            static thread_local std::vector<const ofxOceanodeAbstractParameter*> forwardingPath;
            if(std::find(forwardingPath.begin(), forwardingPath.end(), this) != forwardingPath.end()) return false;
            forwardingPath.push_back(this);
            struct ForwardingPathScope {
                std::vector<const ofxOceanodeAbstractParameter*>& path;
                ~ForwardingPathScope(){path.pop_back();}
            } forwardingScope{forwardingPath};
            bool accepted = false;
            for(auto* target : forwardTargets()) accepted |= target->receiveParameter(p);
            if(accepted) forwardedSource = p;
            return accepted;
        }
        return false;
    }
    
    // Routers and portals: pass on values of types they do not take themselves
    // (an SC audio port into a number router) to the given parameters.
    void setForwardTargets(std::function<std::vector<ofxOceanodeAbstractParameter*>()> targets){forwardTargets = targets;};
    void forwardToOutConnections();
    ofAbstractParameter* getForwardedSource(){return forwardedSource;};
    
    void addConnectFunc(std::function<void()> func){
        if(!connectFunction) connectFunction = func;
    }
    
    void connectedParameter(){if(connectFunction) connectFunction();};
    
    void addDisconnectFunc(std::function<void()> func){
        if(!disconnectFunction) disconnectFunction = func;
    }
    
    void disconnectedParameter(){
        if(disconnectFunction) disconnectFunction();
        if(forwardedSource == nullptr) return;
        forwardedSource = nullptr;
        if(forwardTargets) for(auto* target : forwardTargets()) target->disconnectedParameter();
    };
    
    void setNodeModel(ofxOceanodeNodeModel* _nodeModel);
    ofxOceanodeNodeModel* getNodeModel(){return nodeModel;};
	
protected:
	std::shared_ptr<oceanodeProfiling::ParameterHook> profilingHook;
    std::shared_ptr<oceanodeProfiling::Record> profilingRecord;
	virtual const ofParameterGroup getFirstParent() const = 0;
	virtual void setSerializable(bool serializable)=0;
    virtual std::string escape(const std::string & str) const = 0;
	virtual const void* getInternalObject() const = 0;
	
private:
    static uint64_t& dropdownGraphRevision(){static uint64_t revision = 1; return revision;}
    bool dropdownRelay = false;
    bool dropdownSourceAvailable = true;
    std::vector<ofxOceanodeAbstractParameter*> dropdownPeers;
    static std::map<std::string, InputCombiner>& inputCombiners(){
        static std::map<std::string, InputCombiner> combiners;
        return combiners;
    }
    std::shared_ptr<void> combinedInputState;
    std::vector<ofxOceanodeAbstractConnection*> inConnections;
    std::vector<ofxOceanodeAbstractConnection*> outConnections;
	ofxOceanodeParameterFlags flags;
    bool hasScope, hasTimeline;
    
    std::map<std::string, std::shared_ptr<BaseConnectionFunction>> connnectionFunctions;
    std::function<void()> connectFunction;
    std::function<void()> disconnectFunction;
    std::function<std::vector<ofxOceanodeAbstractParameter*>()> forwardTargets;
    ofAbstractParameter* forwardedSource = nullptr;
    
    ofxOceanodeNodeModel* nodeModel = nullptr;
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
    std::string type() const {return parameter->type(); }
    std::string getEscapedName() const {return parameter->getEscapedName(); }
	std::string valueType() const { return parameter->valueType(); };
    bool isInit() const { return parameter->isInit(); }
	void setParent(ofParameterGroup &_parent) { parameter->setParent(_parent);}
	bool isSerializable() const { return parameter->isSerializable();}
	bool isReadOnly() const { return parameter->isReadOnly();}
	std::shared_ptr<ofAbstractParameter> newReference() const { return std::make_shared<ofxOceanodeParameter<ParameterType>>(*this);}
    bool isReferenceTo(const ofAbstractParameter & other) const { return parameter->isReferenceTo(other);}
    ofEvent<std::string>& nameChangedEvent() {return parameter->nameChangedEvent();}
	
	//Custom
	void bindParameter(ofParameter<ParameterType> &p){
		//TODO: Throw exception
		if(parameter == nullptr){
			parameter = std::make_shared<ofParameter<ParameterType>>(p);
			defaultValue = p;
			profilingHook = oceanodeProfiling::ParameterHook::bind(*parameter, true);
            setFlags(getFlags());
		}
	}
    void bindParameter(std::shared_ptr<ofParameter<ParameterType>> &p){
        parameter = p;
        defaultValue = p.get()->get();
        profilingHook = oceanodeProfiling::ParameterHook::bind(*parameter, true);
        setFlags(getFlags());
    }
	ofParameter<ParameterType> & getParameter(){return *parameter;}
    
    void setDefaultValue(ParameterType v){defaultValue = v;};
	ParameterType getDefaultValue(){return defaultValue;};
	
	void setDropdownOptions(std::vector<std::string> op){
        if(dropdownOptions == op) return;
        dropdownOptions = std::move(op);
        touchDropdownGraph();
        dropdownOptionsChanged.notify();
    };
	std::vector<std::string> getDropdownOptions() override {return dropdownOptions;};
    ofEvent<void> dropdownOptionsChanged;
	
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
    std::string escape(const std::string & str) const { return "";}
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
    std::string type() const {return parameter->type(); }
    std::string getEscapedName() const {return parameter->getEscapedName(); }
	std::string valueType() const { return parameter->valueType(); };
    bool isInit() const { return parameter->isInit(); }
	void setParent(ofParameterGroup &_parent) { parameter->setParent(_parent);}
	bool isSerializable() const { return parameter->isSerializable();}
	bool isReadOnly() const { return parameter->isReadOnly();}
	std::shared_ptr<ofAbstractParameter> newReference() const { return std::make_shared<ofxOceanodeParameter<void>>(*this);}
    bool isReferenceTo(const ofAbstractParameter & other) const { return parameter->isReferenceTo(other);}
    ofEvent<std::string>& nameChangedEvent() {return parameter->nameChangedEvent();}
	
	//Custom
	void bindParameter(ofParameter<void> &p){
		parameter = std::make_shared<ofParameter<void>>(p);
		profilingHook = oceanodeProfiling::ParameterHook::bind(*parameter, true);
        setFlags(getFlags());
	}
	ofParameter<void> & getParameter(){return *parameter;}
	
	ofxOceanodeParameterFlags getFlags(){ return ofxOceanodeAbstractParameter::getFlags(); };
	void setFlags(ofxOceanodeParameterFlags f){ ofxOceanodeAbstractParameter::setFlags(f); };
	
protected:
	const ofParameterGroup getFirstParent() const { return parameter->getFirstParent();}
	void setSerializable(bool serializable) { parameter->setSerializable(serializable);}
    std::string escape(const std::string & str) const { return "";}
	const void* getInternalObject() const { return parameter->getInternalObject();}
	
private:
	std::shared_ptr<ofParameter<void>> parameter;
};

#include "ofxOceanodeParameter.inl"

#endif /* ofxOceanodeParameter_h */
