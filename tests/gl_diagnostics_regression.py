#!/usr/bin/env python3
"""Check GL error attribution without requiring a graphics context."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <deque>
#include <sstream>
#include <string>
#include <vector>
using GLenum = unsigned int;
constexpr GLenum GL_NO_ERROR = 0, GL_INVALID_OPERATION = 1282;
inline std::deque<GLenum> errors;
inline int polls = 0;
inline std::vector<std::string> messages;
inline GLenum glGetError(){
    ++polls;
    if(errors.empty()) return GL_NO_ERROR;
    auto value = errors.front(); errors.pop_front(); return value;
}
struct Log {
    std::ostringstream stream;
    template<class T> Log& operator<<(const T& value){ stream << value; return *this; }
    ~Log(){ messages.push_back(stream.str()); }
};
inline Log ofLogError(const char*){ return {}; }
'''
CASES = r'''
#include "ofxOceanodeGlDiagnostics.h"
#include <cassert>
#include <iostream>
using oceanodeGlDiagnostics::Scope;
int main(int argc, char** argv){
    assert(oceanodeGlDiagnostics::enabled() == (std::string(argv[1]) == "on"));
    oceanodeGlDiagnostics::enabled() = false;
    int identities = 0;
    { Scope s([&]{ ++identities; return "off"; }, "draw"); }
    Scope::checkpoint("disabled addon checkpoint");
    assert(polls == 0 && identities == 0);
    oceanodeGlDiagnostics::enabled() = true;
    errors.push_back(1282);
    { Scope s([]{ return "first"; }, "draw"); }
    assert(messages.size() == 1 && messages[0].find("outside node callbacks") != std::string::npos);
    messages.clear();
    {
        Scope parent([]{ return "root/Macro_2"; }, "draw");
        errors.push_back(1282);
        {
            Scope child([]{ return "root/Macro_2/Noise_1"; }, "draw");
            errors.push_back(1282);
        }
        errors.push_back(1281);
    }
    assert(messages.size() == 3);
    assert(messages[0].find("root/Macro_2 | draw | before next node") != std::string::npos);
    assert(messages[1].find("root/Macro_2/Noise_1 | draw | after call") != std::string::npos);
    assert(messages[2].find("1281 | root/Macro_2 | draw | after call") != std::string::npos);
    messages.clear();
    { Scope s([]{ return "next"; }, "GUI draw"); }
    assert(messages.empty());
    { Scope s([]{ return "update node"; }, "update"); errors.push_back(1282); }
    assert(messages.size() == 1 && messages[0].find("update node | update") != std::string::npos);
    messages.clear();
    {
        Scope s([]{ return "root/Macro_2/Indexer Texture_1"; }, "draw");
        errors.push_back(1282);
        Scope::checkpoint("bind parameter texture buffer");
    }
    assert(messages.size() == 1 && messages[0].find("Indexer Texture_1 | draw | bind parameter texture buffer") != std::string::npos);
    std::cout << "PASS disabled tracing, environment opt-in, inherited errors, nested macros and callback phases\n";
}
'''
with tempfile.TemporaryDirectory(prefix="oceanode-gl-diagnostics-") as directory:
    tmp = Path(directory)
    (tmp / "ofMain.h").write_text(STUB)
    (tmp / "test.cpp").write_text(CASES)
    executable = tmp / "test"
    subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                    "-I" + str(tmp), "-I" + str(ROOT / "src"), str(tmp / "test.cpp"), "-o", str(executable)], check=True)
    for setting in ("0", "1"):
        subprocess.run([str(executable), "on" if setting == "1" else "off"], env={**os.environ, "OCEANODE_GL_TRACE": setting}, check=True)
