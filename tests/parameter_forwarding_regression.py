#!/usr/bin/env python3
"""Exercise production receiveParameter with cyclic portal/router target graphs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def receive_method():
    source = (ROOT / "src/ofxOceanodeParameter.h").read_text()
    start = source.index("bool receiveParameter(")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


CODE = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace oceanodeProfiling {
enum class Phase { Parameters };
struct Scope { Scope(int, Phase) {} };
}
struct ofAbstractParameter {
    std::string valueType() const {return "custom-audio";}
};
struct Receiver {
    int calls = 0;
    bool throws = false;
    void execute(ofAbstractParameter*) {
        if(throws) throw std::runtime_error("receiver failure");
        ++calls;
    }
};
struct ofxOceanodeAbstractParameter {
    int profilingRecord = 0;
    std::map<std::string, std::shared_ptr<Receiver>> connnectionFunctions;
    std::function<std::vector<ofxOceanodeAbstractParameter*>()> forwardTargets;
    ofAbstractParameter* forwardedSource = nullptr;
    RECEIVE_METHOD
};
using Parameter = ofxOceanodeAbstractParameter;
static void targets(Parameter& parameter, std::vector<Parameter*> sinks) {
    parameter.forwardTargets = [sinks] {return sinks;};
}
int main() {
    ofAbstractParameter source;
    Parameter a, b, sink, unsupported;
    auto receiver = std::make_shared<Receiver>();
    sink.connnectionFunctions[source.valueType()] = receiver;

    // A portal's matching group can route straight back into its own Value.
    targets(a, {&a});
    assert(!a.receiveParameter(&source));
    assert(a.forwardedSource == nullptr);

    // A router/portal loop with no compatible exit must reject the value.
    targets(a, {&b}); targets(b, {&a, &unsupported});
    assert(!a.receiveParameter(&source));
    assert(a.forwardedSource == nullptr && b.forwardedSource == nullptr);

    // Stop the cyclic branch, but continue to the compatible exit.
    targets(b, {&a, &sink});
    assert(a.receiveParameter(&source));
    assert(receiver->calls == 1);
    assert(a.forwardedSource == &source && b.forwardedSource == &source);
    assert(a.receiveParameter(&source));
    assert(receiver->calls == 2); // guard resets between updates

    // Shared downstream routers can be revisited through independent branches.
    Parameter left, right, shared;
    targets(a, {&left, &right}); targets(left, {&shared});
    targets(right, {&shared}); targets(shared, {&sink});
    assert(a.receiveParameter(&source));
    assert(receiver->calls == 4);

    // Exceptions from either target discovery or delivery unwind the guard.
    shared.forwardTargets = []() -> std::vector<Parameter*> {
        throw std::runtime_error("target discovery failure");
    };
    try {a.receiveParameter(&source); assert(false);} catch(const std::runtime_error&) {}
    targets(shared, {&sink}); receiver->throws = true;
    try {a.receiveParameter(&source); assert(false);} catch(const std::runtime_error&) {}
    receiver->throws = false;
    assert(a.receiveParameter(&source));
    assert(receiver->calls == 6);

    // Supported input goes directly to its receiver without resolving targets.
    sink.forwardTargets = []() -> std::vector<Parameter*> {std::abort();};
    assert(sink.receiveParameter(&source));
    assert(receiver->calls == 7);
    assert(!unsupported.receiveParameter(&source));
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="parameter-forwarding-") as directory:
        source = Path(directory) / "forwarding.cpp"
        binary = Path(directory) / "forwarding"
        source.write_text(CODE.replace("RECEIVE_METHOD", receive_method()))
        subprocess.run([
            "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print("Parameter forwarding regression passed: cycles, exits, shared branches, repeated updates, exceptions, direct input.")


if __name__ == "__main__":
    main()
