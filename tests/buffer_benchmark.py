#!/usr/bin/env python3
"""Micro benchmark for the Buffer ring and HeaderPro reads (not a regression test).

Builds the production buffer.h / frame.h / bufferHeaderPro.h with the parameter
and event mocks from header_pro_regression.py, optimized and without sanitizers,
and prints microseconds per operation. Use it to compare storage or lookup
changes: run it before and after, on the same machine.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

TIMESTAMP = r'''
struct Timestamp {
    std::chrono::time_point<std::chrono::system_clock> currentTime;
    Timestamp() : currentTime(std::chrono::system_clock::now()) {}
    explicit Timestamp(int64_t us) : currentTime(std::chrono::system_clock::time_point(std::chrono::microseconds(us))) {}
    int64_t epochMicrosecondsSigned() const { return std::chrono::duration_cast<std::chrono::microseconds>(currentTime.time_since_epoch()).count(); }
    void update() { currentTime = std::chrono::system_clock::now(); }
};
'''

BODY = r'''
template<typename T, typename Node> ofParameter<T> &parameter(Node &node, string name) {
    return *static_cast<ofParameter<T>*>(node.parameters.at(name));
}
using Clock = std::chrono::steady_clock;
template<typename F> double timeMs(F f){ auto a=Clock::now(); f(); return std::chrono::duration<double,std::milli>(Clock::now()-a).count(); }
static float sinkF = 0;
template<typename T> void fill(buffer<T> &b, int M){ for(int i=0;i<M;++i) b.addFrame(T(i), Timestamp(int64_t(i)*16667)); }
void report(const string &label, double usPerOp){ printf("%-52s %12.3f\n", label.c_str(), usPerOp); }
int main(){
    printf("%-52s %12s\n", "case", "us/op");
    for(int M : {100, 1000, 10000}){
        buffer<float> b([](float &v, float &s){ s = v; }, [](float &s){ return s; });
        b.setMaxSize(M); fill(b, M);
        const int K = 200000 / (M/100);
        double ms = timeMs([&]{ for(int i=0;i<K;++i) b.addFrame(float(i), Timestamp(int64_t(M+i)*16667)); });
        report("addFrame<float> full M=" + to_string(M), ms*1000/K);
        ms = timeMs([&]{ for(int i=0;i<K;++i) sinkF += b.getAvailableHistoryMs(); });
        report("getAvailableHistoryMs M=" + to_string(M), ms*1000/K);
        for(int N : {1, 16, 128}){
            ms = timeMs([&]{ for(int i=0;i<K/N+1;++i) for(int c=0;c<N;++c){ bool o; sinkF += b.getClosestFrameDelayMs(c*10.f, true, &o).getObjectRef(); } });
            report("ms-read N=" + to_string(N) + " M=" + to_string(M) + " (per full read)", ms*1000/(K/N+1));
        }
    }
    for(int M : {100, 1000}){
        buffer<vector<float>> b([](vector<float> &v, vector<float> &s){ s = v; }, [](vector<float> &s){ return s; });
        b.setMaxSize(M);
        vector<float> v(1024, 1.f);
        for(int i=0;i<M;++i) b.addFrame(v, Timestamp(int64_t(i)*16667));
        const int K = 20000 / (M/100);
        double ms = timeMs([&]{ for(int i=0;i<K;++i) b.addFrame(v, Timestamp(int64_t(M+i)*16667)); });
        report("addFrame<vector<float>(1024)> full M=" + to_string(M), ms*1000/K);
    }
    // HeaderPro at "60 Hz": one sample recorded then one update per tick.
    for(int mode : {1, 0}) for(int M : {100, 1000, 10000}) for(int N : {1, 16, 128}){
        if(mode == 0 && (N != 16 || M == 1000)) continue;
        buffer<float> history([](float &v, float &s){ s = v; }, [](float &s){ return s; });
        history.setMaxSize(M); fill(history, M);
        bufferHeaderPro<float> reader("f", 0.f, true);
        reader.setup();
        parameter<buffer<float>*>(reader, "Buffer Input") = &history;
        parameter<int>(reader, "Copies") = N;
        parameter<int>(reader, "Offset Mode") = mode;
        parameter<vector<float>>(reader, "Offset") = vector<float>{mode == 1 ? 10.f : 1.f};
        ofEventArgs args; const int K = 20000 / (M/100) / (N>16?4:1);
        double ms = timeMs([&]{ for(int i=0;i<K;++i){ history.addFrame(float(i), Timestamp(int64_t(M+i)*16667)); reader.update(args); } });
        report(string("HeaderPro tick ") + (mode == 1 ? "Ms" : "Frames") + " N=" + to_string(N) + " M=" + to_string(M), ms*1000/K);
    }
    printf("sizeof(bufferFrame<float>)=%zu sizeof(bufferFrame<vector<float>>)=%zu\n", sizeof(bufferFrame<float>), sizeof(bufferFrame<vector<float>>));
}
'''


def main():
    test_source = (ROOT / "tests/header_pro_regression.py").read_text()
    prelude = test_source.split("SOURCE = r'''")[1]
    prelude = prelude.split("template<typename T, typename Node> ofParameter<T> &parameter")[0]
    prelude = re.sub(r"struct Timestamp \{.*?\n\};\n", TIMESTAMP, prelude, count=1, flags=re.S)
    prelude = re.sub(r"template<typename T> struct TestBuffer.*?\n\};\n", "", prelude, count=1, flags=re.S)
    prelude = "#include <chrono>\n#include <cstdio>\n" + prelude
    with tempfile.TemporaryDirectory(prefix="buffer-benchmark-") as directory:
        folder = Path(directory)
        source = folder / "buffer_benchmark.cpp"
        binary = folder / "buffer_benchmark"
        source.write_text(prelude + BODY)
        subprocess.run([
            "clang++", "-std=c++17", "-O2", "-DNDEBUG",
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Buffer"),
            "-I" + str(ROOT / "src/Nodes"),
            "-I" + str(ROOT / "src"),
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Generators"),
            str(source), "-o", str(binary),
        ], check=True)
        sys.stdout.flush()
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
