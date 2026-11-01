#pragma once

#include "ofMain.h"
#include <cstdlib>
#include <functional>
#include <string>

// Opt in: polling the driver after every node can affect playback timing.
namespace oceanodeGlDiagnostics {
inline bool& enabled(){
    static bool value = []{
        const char* setting = std::getenv("OCEANODE_GL_TRACE");
        return setting && std::string(setting) == "1";
    }();
    return value;
}

class Scope {
public:
    using Identity = std::function<std::string()>;

    // The identity callable is only kept (and its std::function built) while
    // tracing is on: every node constructs four scopes per frame.
    template<typename Fn>
    Scope(Fn&& identity, const char* phase) : phase(phase), tracing(enabled()) {
        if(!tracing) return;
        this->identity = std::forward<Fn>(identity);
        previous = current();
        // A Macro can enter a child before returning. Attribute pending parent
        // errors to the parent, rather than blaming the child about to run.
        drain(previous, "before next node");
        current() = this;
    }

    ~Scope(){
        if(!tracing) return;
        drain(this, "after call");
        current() = previous;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    // Addon checkpoints distinguish GL commands from synchronous downstream
    // work which otherwise shares the enclosing node's callback.
    static void checkpoint(const char* operation){
        if(enabled()) drain(current(), operation);
    }

private:
    static Scope*& current(){
        static thread_local Scope* scope = nullptr;
        return scope;
    }

    static void drain(const Scope* owner, const char* checkpoint){
        GLenum error;
        while((error = glGetError()) != GL_NO_ERROR){
            ofLogError("Oceanode GL") << "OpenGL error " << error
                << (error == GL_INVALID_OPERATION ? " (GL_INVALID_OPERATION)" : "")
                << " | " << (owner ? owner->identity() : "outside node callbacks")
                << " | " << (owner ? owner->phase : "unattributed")
                << " | " << checkpoint;
        }
    }

    Identity identity;
    const char* phase;
    bool tracing;
    Scope* previous = nullptr;
};
}
