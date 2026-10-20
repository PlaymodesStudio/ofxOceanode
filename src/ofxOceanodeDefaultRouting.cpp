//
//  ofxOceanodeDefaultRouting.cpp
//
//  The default routers, portals and buffers, registered by the ofxOceanode
//  constructor.
//  Split from ofxOceanode.cpp so editing it does not compile every default
//  node again; the constructor runs the three parts in their old order.
//

#include "ofxOceanode.h"

void ofxOceanode::registerDefaultRouting(){
    //Register default Routers
    nodeRegistry->registerModel<router<vector<float>>>("Router", "v_f", 0, 0, 1);
    nodeRegistry->registerModel<router<float>>("Router", "f", 0, 0, 1);
    nodeRegistry->registerModel<router<vector<int>>>("Router", "v_i", 0, 0, 1);
    nodeRegistry->registerModel<router<int>>("Router", "i", 0, 0, 1);
    nodeRegistry->registerModel<router<string>>("Router", "s", "string");
    nodeRegistry->registerModel<router<vector<string>>>("Router", "v_s", "string");
    nodeRegistry->registerModel<routerDropdown>("Router");
    nodeRegistry->registerModel<router<bool>>("Router", "b", false);
    nodeRegistry->registerModel<router<void>>("Router", "v");
    nodeRegistry->registerModel<router<char>>("Router", "c", ' ');
    nodeRegistry->registerModel<router<ofColor>>("Router", "color", ofColor::black);
    nodeRegistry->registerModel<router<ofFloatColor>>("Router", "color_f", ofFloatColor::black);
    nodeRegistry->registerModel<router<Timestamp>>("Router", "timestamp", Timestamp());
    
    //Register default Portals
    nodeRegistry->registerModel<portal<vector<float>>>("Portal", "v_f", 0, true);
    nodeRegistry->registerModel<portal<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<portal<vector<int>>>("Portal", "v_i", 0, true);
    nodeRegistry->registerModel<portal<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<portal<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<portal<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<portal<void>>("Portal", "v");
    nodeRegistry->registerModel<portal<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<portal<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<portal<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);
    nodeRegistry->registerModel<portal<Timestamp>>("Router", "timestamp", Timestamp());
    
    //Register default BufferNodes
    nodeRegistry->registerModel<bufferNode<vector<float>>>("Portal", "v_f", 0, true);
    nodeRegistry->registerModel<bufferNode<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<bufferNode<vector<int>>>("Portal", "v_i", 0, true);
    nodeRegistry->registerModel<bufferNode<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<bufferNode<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<bufferNode<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<bufferNode<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<bufferNode<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<bufferNode<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);
    
    //Register defalut BufferHeaders
    nodeRegistry->registerModel<bufferHeader<vector<float>>>("Portal", "v_f", vector<float>(1, 0), false);
    nodeRegistry->registerModel<bufferHeader<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<bufferHeader<vector<int>>>("Portal", "v_i", vector<int>(1, 0), false);
    nodeRegistry->registerModel<bufferHeader<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<bufferHeader<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<bufferHeader<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<bufferHeader<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<bufferHeader<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<bufferHeader<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);

    // HeaderPro uses an explicit offset mode; keep Header registered for saved patches.
    nodeRegistry->registerModel<bufferHeaderPro<vector<float>>>("HeaderPro", "v_f", vector<float>(1, 0), false);
    nodeRegistry->registerModel<bufferHeaderPro<float>>("HeaderPro", "f", 0, true);
    nodeRegistry->registerModel<bufferHeaderPro<vector<int>>>("HeaderPro", "v_i", vector<int>(1, 0), false);
    nodeRegistry->registerModel<bufferHeaderPro<int>>("HeaderPro", "i", 0, true);
    nodeRegistry->registerModel<bufferHeaderPro<string>>("HeaderPro", "s", "string");
    nodeRegistry->registerModel<bufferHeaderPro<bool>>("HeaderPro", "b", false);
    nodeRegistry->registerModel<bufferHeaderPro<char>>("HeaderPro", "c", ' ');
    nodeRegistry->registerModel<bufferHeaderPro<ofColor>>("HeaderPro", "color", ofColor::black);
    nodeRegistry->registerModel<bufferHeaderPro<ofFloatColor>>("HeaderPro", "color_f", ofFloatColor::black);
    
    //Register default BufferTpes
    registerType<buffer<float>*>("buffer_f");
    registerType<buffer<int>*>("buffer_i");
    registerType<buffer<bool>*>("buffer_b");
    registerType<buffer<string>*>("buffer_s");
    registerType<buffer<char>*>("buffer_c");
    registerType<buffer<vector<float>>*>("buffer_v_f");
    registerType<buffer<vector<int>>*>("buffer_v_i");
    registerType<buffer<ofColor>*>("buffer_color");
    registerType<buffer<ofFloatColor>*>("buffer_color_f");
}
