#pragma once
#include "ofParameter.h"
#include "ofxOceanodeProfiler.h"
#include <limits>
#include <type_traits>

namespace oceanodeProfiling {
// Oceanode copies its parameter wrappers into groups. Copies must share ONE
// listener pair, and callbacks must not capture the original wrapper's address.
struct ParameterHook : ListenerBinding, std::enable_shared_from_this<ParameterHook> {
    std::shared_ptr<Record> record;
    std::shared_ptr<Publication> publication;
    std::vector<Scope> scopes;
    ofEventListeners listeners;
    std::function<void()> install;
    bool installed = false;

    void setRecord(const std::shared_ptr<Record>& value) {
        if(record == value) return;
        setEnabled(false);
        record = value;
        owner = value;
        if(publication){
            publication->owner = value;
            publication->pending = {};
            publication->history.clear();
        }
        if(record){
            record->bindings.push_back(shared_from_this());
            if(publication){
                record->publications.erase(std::remove_if(record->publications.begin(), record->publications.end(), [&](const std::weak_ptr<Publication>& weak){
                    return weak.expired() || weak.lock() == publication;
                }), record->publications.end());
                record->publications.push_back(publication);
                if(auto session = record->session.lock()) session->preparePublication(*publication);
            }
            setEnabled(record->enabled.load(std::memory_order_relaxed));
        }
    }
    void configurePublication(bool outputCapable, bool bidirectional) {
        if(!publication) return;
        publication->outputCapable = outputCapable;
        publication->bidirectional = bidirectional;
        if(record) if(auto session = record->session.lock()) session->preparePublication(*publication);
    }
    void setEnabled(bool value) override {
        if(value == installed) return;
        installed = value;
        if(value) { scopes.reserve(8); install(); }
        else {
            listeners.unsubscribeAll();
            while(!scopes.empty()) scopes.pop_back();
        }
    }

    void begin() {
        if(record && record->enabled.load(std::memory_order_relaxed) &&
           record->ownerThread == std::this_thread::get_id()){
            while(!scopes.empty() && !scopes.back().isCurrent()) scopes.pop_back();
            scopes.emplace_back(record, Phase::Parameters);
            if(publication) scopes.back().publish(*publication);
        }
    }
    void end() {
        if(record && record->ownerThread == std::this_thread::get_id() && !scopes.empty()) scopes.pop_back();
    }

    template<typename T>
    static std::shared_ptr<ParameterHook> bind(ofParameter<T>& parameter, bool trackPublications = false) {
        auto hook = std::make_shared<ParameterHook>();
        if(trackPublications){
            hook->publication = std::make_shared<Publication>();
            hook->publication->name = [parameter](){ return parameter.getName(); };
        }
        std::weak_ptr<ParameterHook> weak = hook;
        // Keep a reference to the OF parameter, but install NO listeners until
        // Profile! is pressed. Copies share the same OF event object.
        hook->install = [weak, parameter]() mutable {
            auto hook = weak.lock();
            if(!hook) return;
            constexpr int first = std::numeric_limits<int>::min();
            constexpr int last = std::numeric_limits<int>::max();
            if constexpr(std::is_void<T>::value){
                hook->listeners.push(parameter.newListener([weak](){ if(auto h = weak.lock()) h->begin(); }, first));
                hook->listeners.push(parameter.newListener([weak](){ if(auto h = weak.lock()) h->end(); }, last));
            }else{
                hook->listeners.push(parameter.newListener([weak](T&){ if(auto h = weak.lock()) h->begin(); }, first));
                hook->listeners.push(parameter.newListener([weak](T&){ if(auto h = weak.lock()) h->end(); }, last));
            }
        };
        return hook;
    }

    static std::shared_ptr<ParameterHook> bindGroup(ofParameterGroup& group) {
        auto hook = std::make_shared<ParameterHook>();
        std::weak_ptr<ParameterHook> weak = hook;
        hook->install = [weak, group]() mutable {
            auto hook = weak.lock();
            if(!hook) return;
            hook->listeners.push(group.parameterChangedE().newListener([weak](ofAbstractParameter&){
                if(auto h = weak.lock()) h->begin();
            }, std::numeric_limits<int>::min()));
            hook->listeners.push(group.parameterChangedE().newListener([weak](ofAbstractParameter&){
                if(auto h = weak.lock()) h->end();
            }, std::numeric_limits<int>::max()));
        };
        return hook;
    }
};
}
