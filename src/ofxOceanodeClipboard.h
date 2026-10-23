#pragma once

#include "imgui.h"
#include <string>
#include <iterator>
#if defined(TARGET_MINGW)
#include <utf8cpp/utf8.h>
#else
#include <utf8.h>
#endif

#if defined(__APPLE__) && !defined(TARGET_OF_IOS)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_OSX
#include <objc/message.h>
#include <objc/runtime.h>
#else
#include "ofAppRunner.h"
#endif

namespace ofxOceanodeClipboard {

inline std::string validText(const std::string& text){
    std::string result;
    result.reserve(text.size());
    utf8::replace_invalid(text.begin(), text.end(), std::back_inserter(result));
    // Clipboard APIs and ImGui use zero-terminated strings. Keep the rest of
    // a message visible/copyable even if a logger sends an embedded zero.
    for(size_t pos = 0; (pos = result.find('\0', pos)) != std::string::npos; pos += 3)
        result.replace(pos, 1, "\xef\xbf\xbd");
    return result;
}

#if defined(__APPLE__) && TARGET_OS_OSX
// Call AppKit from C++ so existing host projects need no new .mm source file.
template<typename Result, typename... Args>
inline Result send(id object, const char* selector, Args... args){
    return reinterpret_cast<Result (*)(id, SEL, Args...)>(objc_msgSend)(
        object, sel_registerName(selector), args...);
}

inline id string(const char* text){
    return send<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                    "stringWithUTF8String:", text);
}

inline id pasteboard(){
    return send<id>(reinterpret_cast<id>(objc_getClass("NSPasteboard")), "generalPasteboard");
}

inline bool setText(const std::string& text){
    const std::string valid = validText(text);
    id pool = send<id>(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")), "new");
    id board = pasteboard();
    id value = string(valid.c_str());
    if(!board || !value){ send<void>(pool, "drain"); return false; }
    send<long>(board, "clearContents");
    const bool copied = send<BOOL>(board, "setString:forType:", value, string("public.utf8-plain-text"));
    send<void>(pool, "drain");
    return copied;
}

inline std::string getText(){
    id pool = send<id>(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")), "new");
    id value = send<id>(pasteboard(), "stringForType:", string("public.utf8-plain-text"));
    const char* utf8 = value ? send<const char*>(value, "UTF8String") : nullptr;
    std::string text = utf8 ? utf8 : "";
    send<void>(pool, "drain");
    return text;
}
#else
inline bool setText(const std::string& text){
    const std::string valid = validText(text);
    ofSetClipboardString(valid);
    return ofGetClipboardString() == valid;
}
inline std::string getText(){ return ofGetClipboardString(); }
#endif

inline void install(){
#if IMGUI_VERSION_NUM >= 19110
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text){ setText(text); };
    ImGui::GetPlatformIO().Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
#else
    ImGui::GetIO().SetClipboardTextFn = [](void*, const char* text){ setText(text); };
    ImGui::GetIO().GetClipboardTextFn = [](void*) -> const char* {
#endif
        static std::string text;
        text = getText();
        return text.c_str();
    };
}

}
