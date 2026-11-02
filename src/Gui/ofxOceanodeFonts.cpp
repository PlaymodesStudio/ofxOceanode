#include "ofxOceanodeFonts.h"
#include "ofConstants.h"
#include "ofFileUtils.h"

namespace {
#ifdef TARGET_OSX
// Terminal.app bundles SF Mono on every macOS install; Apple does not install it
// system-wide, and its licence does not allow shipping a copy with the addon.
const char* systemFontDir = "/System/Applications/Utilities/Terminal.app/Contents/Resources/Fonts";
#endif

std::string systemFace(const char* file){
    const std::string dir = ofxOceanodeFonts::systemFontDirectory();
    if(dir.empty()) return "";
    const std::string path = dir + "/" + file;
    return ofFile(path, ofFile::Reference).exists() ? path : "";
}
}

std::string ofxOceanodeFonts::systemFontDirectory(){
#ifdef TARGET_OSX
    if(ofDirectory(systemFontDir).exists()) return systemFontDir;
#endif
    return "";
}

std::string ofxOceanodeFonts::systemRegularPath(){ return systemFace("SF-Mono-Regular.otf"); }
std::string ofxOceanodeFonts::systemBoldPath(){ return systemFace("SF-Mono-Bold.otf"); }

std::string ofxOceanodeFonts::builtinDescription(){
    return "JetBrains Mono, embedded";
}

ofxOceanodeFonts::Embedded ofxOceanodeFonts::embeddedRegular(){
    return {ofxOceanodeEmbeddedFonts::JetBrainsMonoMedium_compressed_data,
            ofxOceanodeEmbeddedFonts::JetBrainsMonoMedium_compressed_size};
}

ofxOceanodeFonts::Embedded ofxOceanodeFonts::embeddedBold(){
    return {ofxOceanodeEmbeddedFonts::JetBrainsMonoExtraBold_compressed_data,
            ofxOceanodeEmbeddedFonts::JetBrainsMonoExtraBold_compressed_size};
}
