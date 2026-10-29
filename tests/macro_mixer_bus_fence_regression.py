#!/usr/bin/env python3
"""Backend readiness fences a transition only before its fade starts.

A resource policy with a private processor (as the SuperCollider ScBus bridge)
reports ready() only after the backend acknowledges a rewiring. Before the fade
starts, that keeps loading outside the timed interpolation. Once it runs, a
rebuild holds the fade instead of restarting it: completing a morph that rewires
a resource cable used to run a second, empty pass of the whole duration.
"""
import macro_mixer_regression as mixer

mixer.PRELUDE += r'''
// A backend resource port, as a SuperCollider bus: mixing runs in a private
// bridge the backend must acknowledge after every rewiring before it is ready.
struct BusPort {
    int node=0; const void* bridge=nullptr;
    bool operator==(const BusPort& o) const { return node==o.node && bridge==o.bridge; }
    bool operator!=(const BusPort& o) const { return !(*this==o); }
};
inline std::ostream& operator<<(std::ostream& s,const BusPort& p) { return s<<p.node; }
inline std::istream& operator>>(std::istream& s,BusPort& p) { return s>>p.node; }
struct BusBridge { BusPort a,b; float t=0; bool ready=false; int rebuilds=0; };
static std::vector<std::weak_ptr<BusBridge>> busBridges;
// Retirement leases the backend holds until it acknowledges the final routing.
static std::vector<std::shared_ptr<void>> busLeases;
static std::vector<std::shared_ptr<BusBridge>> liveBridges() {
    std::vector<std::shared_ptr<BusBridge>> live;
    for(auto& weak:busBridges) if(auto bridge=weak.lock()) live.push_back(bridge);
    return live;
}
// The backend acknowledges every pending rebuild and releases what retired.
static void busServerSync() { for(auto& bridge:liveBridges()) bridge->ready=true; busLeases.clear(); }
// The backend rebuilds its whole graph (as any SC Output rewired in the app).
static void busServerRebuild() { for(auto& bridge:liveBridges()) bridge->ready=false; }
static bool busServerReady() { for(auto& bridge:liveBridges()) if(!bridge->ready) return false; return true; }
class BusBlob : public ofxOceanodeNodeModel {
public:
    BusBlob():ofxOceanodeNodeModel("Bus Blob") {}
    ofParameter<int> voices; ofParameter<float> gain; ofParameter<BusPort> output;
    void setup() override {
        addParameter(voices.set("Voices",1,1,8)); addParameter(gain.set("Gain",0,0,10));
        addOutputParameter(output.set("Output",BusPort{}));
    }
    void update(ofEventArgs&) override { if(output.get().node!=getNumIdentifier()) output=BusPort{int(getNumIdentifier()),nullptr}; }
    MorphHint morphHint(const ofxOceanodeAbstractParameter& p) const override {
        return p.getName()=="Voices" ? MorphHint::Structural : MorphHint::Interpolate;
    }
};
static void registerBus() {
    using Value=MacroMixerMixing::Value;
    auto& mixing=MacroMixerMixing::registry();
    assert(mixing.registerValueType<BusPort>("Bus","Router Bus",[](const BusPort&) {}));
    MacroMixerMixing::Policy p{"linear","Linear","Bus",[](const Value&,const Value&,float,const ofJson&)->Value {
        throw std::runtime_error("Bus mixing requires a private processor");
    },[](const ofJson& o) { if(!o.is_object() || !o.empty()) throw std::runtime_error("Bus takes no options"); return ofJson::object(); }};
    p.createProcessor=[] {
        auto bridge=std::make_shared<BusBridge>(); busBridges.push_back(bridge);
        return [bridge](const Value& av,const Value& bv,float t,const ofJson&) {
            const auto& a=MacroMixerMixing::object<BusPort>(av); const auto& b=MacroMixerMixing::object<BusPort>(bv);
            if(a!=bridge->a || b!=bridge->b) { bridge->a=a; bridge->b=b; bridge->ready=false; ++bridge->rebuilds; }
            bridge->t=t;
            return MacroMixerMixing::object("Bus",BusPort{0,bridge.get()});
        };
    };
    p.ready=[](const Value& v) {
        auto* bridge=static_cast<const BusBridge*>(MacroMixerMixing::object<BusPort>(v).bridge); return !bridge || bridge->ready;
    };
    p.retire=[](const Value& v,std::shared_ptr<void> lease) { if(MacroMixerMixing::object<BusPort>(v).bridge) busLeases.push_back(std::move(lease)); };
    mixing.registerPolicy(std::move(p),true);
}
'''

mixer.CASES = r'''
static void near(float a,float b,float eps=.0001f) { if(!std::isfinite(a) || std::abs(a-b)>eps) { std::cerr<<a<<" != "<<b<<"\n"; std::abort(); } }
int main(int argc,char** argv) {
    assert(argc==3); namespace fs=std::filesystem;
    const std::string scratch=argv[2]; fs::current_path(scratch); ofDisableDataPath(); ofSeedRandom();
    auto window=std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry=std::make_shared<ofxOceanodeNodeRegistry>(); auto types=std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton=ofxOceanodeTypesRegistry::getInstance();
    registerBus();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    types->registerType<BusPort>("Bus"); singleton->registerType<BusPort>("Bus");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<router<BusPort>>("Router","Bus",BusPort{});
    registry->registerModel<BusBlob>("Test"); registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport=std::make_shared<ofxOceanodeTransport>();
    ofxOceanodeContainer parent(registry,types,transport); parent.setCanvasID("BusTest");
    auto save=[&](const std::string& folder,int voices,float gain) {
        ofxOceanodeContainer c(registry,types,transport); c.setCanvasID("BusSource");
        auto& out=c.createNode(std::make_unique<router<BusPort>>("Bus",BusPort{}),1).getNodeModel();
        out.getParameter<std::string>("Name")="Audio";
        auto& blob=static_cast<BusBlob&>(c.createNode(std::make_unique<BusBlob>(),1).getNodeModel());
        blob.voices=voices; blob.gain=gain;
        assert(c.createConnectionFromInfo("Bus_Blob_1","Output","Router_Bus_1","Value"));
        c.savePreset(folder);
    };
    const auto a=scratch+"/a",b=scratch+"/b"; save(a,1,2); save(b,4,6);
    MacroMixerContract contract; contract.routers={{"audio","Audio","Bus"}};
    auto blobs=[](const MacroMixerSource::Graph* graph) { return graph->container->getDynamicNodes().at("Bus Blob"); };
    MacroMixerRuntime r(parent); assert(r.setContract(contract)); assert(r.importMacro(a) && r.importMacro(b));
    const auto idB=r.importedMacro(b)->id();
    r.request(a,0,0,true); r.update(0); const auto* graph=r.origin(); assert(graph && !r.transitioning());
    assert(liveBridges().size()==1 && !busServerReady()); busServerSync(); r.update(1); near(r.progress(),1);
    auto& output=*liveBridges().front(); near(output.t,1); assert(output.rebuilds==1);
    // A Structural change replaces the node: the only Bus cable is rewired.
    auto details=r.sourceDetails(idB);
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="replaced" && d.location=="Bus_Blob_1" && d.message.find("(Voices)")!=std::string::npos;}));
    assert(std::any_of(details.begin(),details.end(),[](const auto& d){return d.kind=="mix" && d.location.find("Router_Bus_1")!=std::string::npos;}));
    {
        r.request(b,1000,0,true); r.update(10); assert(r.morphingInPlace() && r.origin()==graph && blobs(graph).size()==2);
        // Preparation: the cable blend's bridge and the rewired output wait for
        // the backend. The fade has not started; its clock keeps restarting.
        assert(liveBridges().size()==2 && !busServerReady()); auto& blend=*liveBridges().back();
        near(r.progress(),0); r.update(200); near(r.progress(),0); near(blend.t,0); assert(output.rebuilds==2);
        busServerSync(); r.update(300); near(r.progress(),0); // Acknowledged: the fade starts now.
        r.update(800); near(r.progress(),.5); near(blend.t,.5); near(output.t,1); assert(r.transitioning());
        // Completion restores the destination cable, so the output bridge is
        // rewired and waits for the backend again. The fade holds at 1 and
        // only promotion waits; it does not restart for another second.
        r.update(1300); near(r.progress(),1); near(blend.t,1); assert(output.rebuilds==3 && !output.ready && busLeases.size()==1);
        assert(blobs(graph).size()==1 && !blobs(graph).count(1));
        assert(static_cast<BusBlob&>(blobs(graph).begin()->second->getNodeModel()).voices==4);
        r.update(1400); near(r.progress(),1); assert(r.transitioning() && output.rebuilds==3);
        busServerSync(); r.update(1500); near(r.progress(),1); assert(!r.transitioning() && r.origin()==graph);
        r.update(2300); near(r.progress(),1); assert(!r.transitioning() && output.rebuilds==3 && liveBridges().size()==1);
    }
    // A backend rebuild during the fade (any SC Output rewired in the app)
    // holds the fade where it is instead of restarting it.
    {
        // The added node publishes its port on its first update, one frame
        // after preparation, so the backend acknowledges after that frame.
        r.request(a,1000,0,true); r.update(3000); r.update(3010); assert(r.morphingInPlace() && !busServerReady());
        busServerSync(); r.update(3020); near(r.progress(),0); // The fade starts now.
        for(double now=3030;now<=3420;now+=10) r.update(now); near(r.progress(),.4);
        busServerRebuild(); for(double now=3430;now<=3800;now+=10) r.update(now);
        near(r.progress(),.4,.02); assert(r.transitioning());
        busServerSync(); for(double now=3810;now<=4100;now+=10) r.update(now);
        near(r.progress(),.7,.02); assert(r.transitioning());
        for(double now=4110;now<=4500;now+=10) r.update(now);
        near(r.progress(),1); busServerSync(); r.update(4510); assert(!r.transitioning() && r.origin()==graph);
        assert(static_cast<BusBlob&>(blobs(graph).begin()->second->getNodeModel()).voices==1);
    }
    // Stop! during a hold completes as soon as the backend acknowledges.
    {
        r.request(b,1000,0,true); r.update(5000); r.update(5010); busServerSync(); r.update(5020);
        for(double now=5030;now<=5300;now+=10) r.update(now); near(r.progress(),.28,.02);
        busServerRebuild(); r.update(5310); r.finish(); r.update(5320); near(r.progress(),1); assert(r.transitioning());
        busServerSync(); r.update(5330); assert(!r.transitioning() && r.origin()==graph);
        assert(static_cast<BusBlob&>(blobs(graph).begin()->second->getNodeModel()).voices==4);
    }
    r.clear(); assert(liveBridges().empty());
    std::cout<<"MacroMixer backend readiness: loading fenced before the fade, holds during it, no restart at completion.\n";
}
'''

if __name__ == "__main__":
    mixer.main()
