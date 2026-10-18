#pragma once
#include "ofMain.h"
#include "ofxOscMessage.h"
#include <map>
#include <set>
#include <stdexcept>
#define RATE_AUDIO 2
#define RATE_CONTROL 1
class ofxSCBus;
class ofxSCServer;
class SCFakeAllocator {
    ofxSCServer* server;
public:
    explicit SCFakeAllocator(ofxSCServer* s):server(s) {}
    int alloc(int);
    void free(int);
};
class ofxSCServer {
public:
    SCFakeAllocator allocatorStorage{this};
    SCFakeAllocator* allocatorBusAudio=&allocatorStorage;
    struct Record { std::string name; std::map<std::string,double> controls; bool running=true; };
    std::map<int,Record> synths;
    std::map<int,ofxSCBus*> audioBusses;
    std::set<int> buses;
    int busLimit=32,allocations=0,frees=0,synthFrees=0;
    bool latency=false;
    ofEvent<ofxOscMessage> newFeedbackMessage;
    std::vector<int> syncs;
    int syncSequence=100000;
    int requestSync() {int id=++syncSequence;syncs.push_back(id);return id;}
    void acknowledge() {auto pending=std::move(syncs);syncs.clear();for(int id:pending) {ofxOscMessage m;m.setAddress("/synced");m.addIntArg(id);newFeedbackMessage.notify(m);}}
    bool isNRTCapturing() const {return false;}
    bool isNRTCaptureOnly() const {return false;}
    std::vector<std::string> messages;
    void setBLatency(bool b) { latency=b; }
    void sendMsg(ofxOscMessage& m) { messages.push_back(m.getAddress()); }
};
inline int SCFakeAllocator::alloc(int) {
    if(server->buses.size()>=size_t(server->busLimit)) return -1;
    for(int i=256;i<256+server->busLimit*128;i+=128) if(!server->buses.count(i)) {server->buses.insert(i);++server->allocations;return i;}
    return -1;
}
inline void SCFakeAllocator::free(int index) {if(server->buses.erase(index)) ++server->frees;}
class ofxSCBus {
public:
    ofxSCServer* server; int index,channels;
    ofxSCBus(int,int n,ofxSCServer* s):server(s),index(-1),channels(n) {
        index=server->allocatorBusAudio->alloc(n);
        assert(index>=0); // Real ofxSCBus indexes a vector here, so failure must be caught before construction.
    }
    void free() { if(index>=0) {server->allocatorBusAudio->free(index);index=-1;} }
};
class ofxSCSynth {
    ofxSCServer* server;
public:
    static inline int sequence=100;
    static inline int live=0;
    int nodeID=++sequence;
    std::string name;
    std::map<std::string,double> controls;
    ofxSCSynth(std::string n,ofxSCServer* s):server(s),name(std::move(n)) { ++live; }
    ~ofxSCSynth() {--live;}
    template<typename T> void set(const std::string& key,const T& v) { controls[key]=double(v); if(server->synths.count(nodeID)) server->synths[nodeID].controls[key]=double(v); }
    void createAndRun(int=0,int=1,bool run=true) { server->synths[nodeID]={name,controls,run}; }
    void moveBefore(int) {}
    void free() { if(server->synths.erase(nodeID)) ++server->synthFrees; }
    void run(bool b) { if(server->synths.count(nodeID)) server->synths[nodeID].running=b; }
};
