//
//  ofxOceanodeDefaultTypes.cpp
//
//  The built-in types, registered by the ofxOceanode constructor.
//  Split from ofxOceanode.cpp so editing it does not compile every default
//  node again; the constructor runs the three parts in their old order.
//

#include "ofxOceanode.h"

void ofxOceanode::registerDefaultTypes(){
    //Register default types
    registerBuiltInType<float>("f", 0.0f);
    registerBuiltInType<int>("i", 0);
    registerBuiltInType<bool>("b", false);
    typesRegistry->registerType<void>("v");
    nodeRegistry->registerModel<portalSelector<void>>("Portal", "v");
    registerBuiltInType<string>("s", "");
    registerBuiltInType<char>("c", ' ');
    registerBuiltInType<vector<float>>("v_f", vector<float>(1, 0.0f));
    registerBuiltInType<vector<int>>("v_i", vector<int>(1, 0));
    registerBuiltInType<vector<string>>("v_s", vector<string>(1, ""));
    registerBuiltInType<ofColor>("color", ofColor::black);
    registerBuiltInType<ofFloatColor>("color_f", ofFloatColor::black);
    registerBuiltInType<Timestamp>("timestamp", Timestamp());
}
