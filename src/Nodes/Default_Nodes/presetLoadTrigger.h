#ifndef presetLoadTrigger_h
#define presetLoadTrigger_h

#include "ofxOceanodeNodeModel.h"
#include <queue>

class presetLoadTrigger : public ofxOceanodeNodeModel {
public:
    presetLoadTrigger() : ofxOceanodeNodeModel("Preset Load Trigger") {}

    ~presetLoadTrigger() {
        //ofRemoveListener(ofEvents().update, this, &PresetLoadTrigger::update);
    }

    void setup() override {
        addOutputParameter(triggerOut.set("Trigger Out", {0}, {0.0f}, {1.0f}));
        addOutputParameter(voidOut.set("Void Out"));
    }

    void presetHasLoaded() override {
        enqueueOutputValue(vector<float>{0.5f});
        voidOut.trigger();  // Trigger the void output on preset load
    }

    void update(ofEventArgs &args) {
        // If we have values to dispatch
        if (!outputQueue.empty()) {
            triggerOut = outputQueue.front();
            outputQueue.pop();
        }
    }

    void enqueueOutputValue(vector<float> value) {
        // Enqueue every new output value twice
        outputQueue.push(value);
        outputQueue.push(vector<float>(value.size(), 0.0f));
    }

private:
    ofParameter<vector<float>> triggerOut;
    ofParameter<void> voidOut;  // New void output parameter

    std::queue<vector<float>> outputQueue;
};

#endif /* presetLoadTrigger_h */
