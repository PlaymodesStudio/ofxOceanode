#!/usr/bin/env python3
"""Numeric and Boolean type policy boundary checks, using production adapters."""
import macro_mixer_regression as mixer
mixer.CASES = r'''
using Vec=std::vector<int>;
static void reject(const std::function<void()>& f) { bool threw=false; try{f();}catch(const std::exception&){threw=true;} assert(threw); }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    using namespace MacroMixerMixing;
    Registry policies;
    auto scalar=[&](int a,int b,float t) { return std::get<int>(policies.mix("Integer","linear",a,b,t)); };
    assert(scalar(-3,8,.5)==3 && scalar(-2,-1,.5)==-2 && scalar(0,1,.5)==1);
    assert(scalar(INT_MIN,INT_MAX,0)==INT_MIN && scalar(INT_MIN,INT_MAX,1)==INT_MAX);
    assert(scalar(INT_MIN,INT_MAX,.5)==-1 && scalar(INT_MAX,INT_MIN,.5)==-1);
    assert(scalar(INT_MAX,INT_MAX,.5)==INT_MAX && scalar(INT_MIN,INT_MIN,.5)==INT_MIN);
    auto vector=[&](std::string id,Vec a,Vec b,float t,ofJson options=ofJson::object()) {
        return std::get<Vec>(policies.mix("Integer Vector",id,a,b,t,options));
    };
    assert(vector("index_zero",{-4,2},{8,20,32,44},.5)==Vec({2,11,16,22}));
    assert(vector("resample",{-4,2},{8,20,32,44},.5)==Vec({2,9,16,23}));
    assert(vector("resample_length",{-4,2},{8,20,32,44},.5)==Vec({2,13,23}));
    // Resampled fractions are rounded only after blending.
    assert(vector("resample",{0,1},{0,0},.25,{{"size",3}})==Vec({0,0,1}));
    assert(vector("resample_length",Vec(32,2),Vec(16,10),.25)==Vec(28,4));
    assert(vector("resample_length",Vec(16,2),Vec(32,10),.75)==Vec(28,8));
    for(const auto& id:{"index_zero","resample","resample_length","switch"}) {
        assert(vector(id,{INT_MIN,INT_MAX},{INT_MAX},0)==Vec({INT_MIN,INT_MAX}));
        assert(vector(id,{INT_MIN,INT_MAX},{INT_MAX},1)==Vec({INT_MAX}));
        assert(vector(id,{},{},.5).empty());
    }
    assert(vector("resample_length",{},{10,20,30,40},.5)==Vec({5,20}));
    assert(vector("resample_length",{10,20,30,40},{},.5)==Vec({5,20}));
    assert(vector("index_zero",{INT_MIN},{INT_MAX},.5)==Vec({-1}));
    assert(vector("resample",{INT_MIN},{INT_MAX},.5,{{"size",3}})==Vec(3,-1));
    assert(vector("index_zero",Vec(4096,INT_MAX),Vec(4096,INT_MAX),.5)==Vec(4096,INT_MAX));
    assert(std::get<int>(policies.mix("Integer","switch",2,10,.25,{{"threshold",.25}}))==10);
    auto gate=[&](bool a,bool b,float t,double threshold=.5) {
        return std::get<bool>(policies.mix("Boolean","switch",a,b,t,{{"threshold",threshold}}));
    };
    assert(!gate(false,true,.49) && gate(false,true,.5));
    assert(gate(true,false,.49) && !gate(true,false,.5));
    assert(!gate(false,true,0,0) && gate(false,true,1,1));
    assert(gate(true,true,.5) && !gate(false,false,.5));
    reject([&]{policies.mix("Integer","linear",1.f,2.f,.5);});
    reject([&]{policies.mix("Boolean","switch",0,1,.5);});
    reject([&]{policies.mix("Boolean","linear",false,true,.5);});
    reject([&]{vector("index_zero",Vec(4097,0),{},.5);});
    reject([&]{vector("resample",{1},{2},.5,{{"size",4097}});});
    reject([&]{gate(false,true,.5,2);});
    assert(policies.policy("Integer").id=="linear" && policies.policy("Integer Vector").id=="index_zero");
    assert(policies.policy("Boolean").id=="switch");

    std::cout<<"Numeric mixing policies passed.\n";
}
'''
if __name__ == '__main__': mixer.main()
