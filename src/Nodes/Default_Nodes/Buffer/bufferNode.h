//
//  bufferNode.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef bufferNode_h
#define bufferNode_h

#include "buffer.h"
#include "ofxOceanodeNodeModel.h"
#include "phasor.h"

template <typename T1, typename T2 = T1>
class bufferNode : public timeGenerator{
public:
    bufferNode(string typelabel, T1 val, bool minmax = false,
               std::function<void(T1&, T2&)> _assignFunction = [](T1 &data, T2 &container){container = data;},
               std::function<T1(T2&)> _returnFunction = [](T2 &container)->T1{return container;},
               std::function<bool(T1&)> _checkFunction = [](T1 &data)->bool{return true;}
               ) :  timeGenerator("Buffer " + typelabel), assignFunction(_assignFunction), returnFunction(_returnFunction), checkFunction(_checkFunction){
        myBuffer = std::make_unique<buffer<T1, T2>>(assignFunction, returnFunction);
    }
    ~bufferNode(){}
    
    void setup() override{
        addParameter(input.set("Input", T1()));
        addParameter(rec.set("Rec", true));
		addParameter(clearBuffer.set("Clear"));

        addParameter(timestamp.set("Timestamp", Timestamp()));
        addParameter(maxSize.set("Max Size", 100, 1, INT_MAX));
        addParameter(output.set("Output", myBuffer.get()));
        
        addInspectorParameter(lastFrameMs.set("Last Frame Ms",0,0,FLT_MAX));
        
        listeners.push(input.newListener([this](T1 &object){
            if(checkFunction(object) && rec){
                if(getOceanodeParameter(timestamp).hasInConnection()){
                    myBuffer->addFrame(input.get(), timestamp);
                }else{ //TODO: Use ofxOceanodeTime time reference
                    myBuffer->addFrame(input.get(), Timestamp(getTime() * 1000000));
                }
            }
            lastFrameMs = myBuffer->getFirstFrameTimestamp().epochMilliseconds() - myBuffer->getLastFrameTimestamp().epochMilliseconds();
            
        }));

		listeners.push(clearBuffer.newListener([this](){
			myBuffer->clear();
		}));

        sizeListener = maxSize.newListener([this](int &_maxSize){
            myBuffer->setMaxSize(_maxSize);
        });
        
        myBuffer->setMaxSize(maxSize);
    }
    
private:
    std::function<void(T1&, T2&)> assignFunction;
    std::function<T1(T2&)> returnFunction;
    std::function<bool(T1&)> checkFunction;
    
    ofEventListeners listeners;
    ofEventListener sizeListener;
    
    ofParameter<T1> input;
    ofParameter<bool> rec;
    ofParameter<Timestamp> timestamp;
	ofParameter<void> clearBuffer;

    
    ofParameter<int> maxSize;
    
    ofParameter<buffer<T1, T2>*> output;
    
    ofParameter<float> lastFrameMs;
    
    std::unique_ptr<buffer<T1, T2>> myBuffer;
};

template<typename T>
class bufferNode<std::vector<T>> : public ofxOceanodeNodeModel{
public:
    bufferNode(string typelabel, T val, bool minmax = false,
               std::function<void(std::vector<T>&, std::vector<T>&)> _assignFunction = [](std::vector<T> &data, std::vector<T> &container){container = data;},
               std::function<std::vector<T>(std::vector<T>&)> _returnFunction = [](std::vector<T> &container)->std::vector<T>{return container;},
               std::function<bool(std::vector<T>&)> _checkFunction = [](std::vector<T> &data)->bool{return true;}
               ) :  ofxOceanodeNodeModel("Buffer " + typelabel), assignFunction(_assignFunction), returnFunction(_returnFunction), checkFunction(_checkFunction){
                   myBuffer = std::make_unique<buffer<std::vector<T>, std::vector<T>>>(assignFunction, returnFunction);
                   if(minmax){
                       addParameter(input.set("Input", std::vector<T>(1, val),
                                              std::vector<T>(1, std::numeric_limits<T>::lowest()),
                                              std::vector<T>(1, std::numeric_limits<T>::max())));
                   }else{
                       addParameter(input.set("Input", std::vector<T>(1, val)));
                   }
               }
    ~bufferNode(){}
    
    void setup() override{
        addParameter(rec.set("Rec", true));
        addParameter(timestamp.set("Timestamp", Timestamp()));
        addParameter(maxSize.set("Max Size", 100, 1, INT_MAX));
        addParameter(output.set("Output", myBuffer.get()));
        
        listeners.push(input.newListener([this](std::vector<T> &object){
            if(checkFunction(object) && rec){
                if(getOceanodeParameter(timestamp).hasInConnection()){
                    myBuffer->addFrame(input.get(), timestamp);
                }else{
                    myBuffer->addFrame(input.get());
                }
            }
        }));
        
		
        sizeListener = maxSize.newListener([this](int &_maxSize){
            myBuffer->setMaxSize(_maxSize);
        });
        
        myBuffer->setMaxSize(maxSize);
    }
    
	void clear()
	{
		myBuffer->clear();
	}
private:
    std::function<void(std::vector<T>&, std::vector<T>&)> assignFunction;
    std::function<std::vector<T>(std::vector<T>&)> returnFunction;
    std::function<bool(std::vector<T>&)> checkFunction;
    
    ofEventListeners listeners;
    ofEventListener sizeListener;
    
    ofParameter<std::vector<T>> input;
    ofParameter<bool> rec;
    ofParameter<Timestamp> timestamp;
    
    ofParameter<int> maxSize;
    
    ofParameter<buffer<std::vector<T>, std::vector<T>>*> output;
    
    std::unique_ptr<buffer<std::vector<T>, std::vector<T>>> myBuffer;
};

#endif /* bufferNode_h */
