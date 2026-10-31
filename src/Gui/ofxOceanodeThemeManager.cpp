#include "ofxOceanodeThemeManager.h"
#include "ofxOceanodeTheme.h"
#include "ofxOceanodeFloatingWindowDecor.h"
#include "ofxOceanodeTabText.h"
#include "ofxOceanodeColors.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeMenu.h"
#include "imgui_internal.h"
#include "ofMain.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <functional>
#include <map>
#include <utility>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H

namespace {
const char* themesFolder = "config/themes/";
const char* themesFile = "config/themes.json";

// The same rule as global layout names: a file name that menus show as-is.
bool validThemeName(const std::string& name){
    for(unsigned char c : name) if(c < 32 || c == 127) return false;
    const auto first = name.find_first_not_of(" \t\r\n");
    return !name.empty() && name.size() < 256 && first == 0 &&
        name.find_last_not_of(" \t\r\n") == name.size() - 1 &&
        name.find("##") == std::string::npos && name.find_first_of("/\\:") == std::string::npos;
}

// Prefer the typographic family (name ID 16): legacy family names often
// split Medium/Light/etc. into separate families. Windows names are UTF-16BE.
std::string themeFontFamilyName(FT_Face face){
    for(FT_UInt i = 0; i < FT_Get_Sfnt_Name_Count(face); ++i){
        FT_SfntName name;
        if(FT_Get_Sfnt_Name(face, i, &name) != 0 || name.name_id != 16) continue;
        if(name.platform_id != 0 && !(name.platform_id == 3 && (name.encoding_id == 1 || name.encoding_id == 10))) continue;
        std::string result;
        for(FT_UInt j = 0; j + 1 < name.string_len; j += 2){
            uint32_t cp = (uint32_t(name.string[j]) << 8) | name.string[j + 1];
            if(cp >= 0xD800 && cp <= 0xDBFF && j + 3 < name.string_len){
                const uint32_t low = (uint32_t(name.string[j + 2]) << 8) | name.string[j + 3];
                if(low >= 0xDC00 && low <= 0xDFFF){
                    cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00;
                    j += 2;
                }
            }
            if(cp != 0 && !(cp >= 0xD800 && cp <= 0xDFFF)) ofUTF8Append(result, cp);
        }
        if(!result.empty()) return result;
    }
    return face->family_name ? face->family_name : "";
}
}

void ofxOceanodeThemeManager::setup(ofxOceanodeCanvas& canvasRef){
    canvas = &canvasRef;
    ofxOceanodeTabText::install();
    refreshThemeFonts();
    const auto path = ofToDataPath(themesFile, true);
    if(ofFile(path).exists()){
        const ofJson config = ofLoadJson(path);
        if(config.is_object() && config.value("version", 0) == 1){
            defaultThemeName = config.value("default", std::string());
            if(!defaultThemeName.empty()) load(defaultThemeName);
        }else ofLogError("ofxOceanodeThemeManager") << "Ignoring " << path << ": unsupported version.";
    }
    applyThemeFont();
}

void ofxOceanodeThemeManager::beforeFrame(){
    // Font atlas changes must happen before NewFrame, after the last frame rendered.
    applyThemeFont();
}

void ofxOceanodeThemeManager::afterRender(){
    // Needs this frame's rendered draw data, which is valid until the next NewFrame.
    pickThemeColorUnderMouse();
}

void ofxOceanodeThemeManager::drawWindows(){
    if(!showThemeEditor) return;
    drawThemeEditorWindow();
    drawThemeColorPickTooltip();
}

void ofxOceanodeThemeManager::setDefaultTheme(const std::string& name){
    ofDirectory::createDirectory(ofToDataPath("config", true), true, true);
    if(ofSavePrettyJson(ofToDataPath(themesFile, true), {{"version", 1}, {"default", name}}))
        defaultThemeName = name;
    else ofLogError("ofxOceanodeThemeManager") << "Could not save " << themesFile;
}

void ofxOceanodeThemeManager::refreshThemeFonts(){
    themeFontFamilies.clear();
    FT_Library library = nullptr;
    if(FT_Init_FreeType(&library) != 0){
        themeFontWarning = "Could not read the font folder.";
        return;
    }
    struct Candidate {
        ThemeFontFamily family;
        int regularScore = INT_MAX;
        int boldScore = INT_MAX;
    };
    std::map<std::string, Candidate> families;
    std::function<void(const std::string&)> scan = [&](const std::string& relativeDir){
        ofDirectory dir(ofToDataPath(relativeDir, true));
        if(!dir.exists()) return;
        dir.listDir();
        dir.sort();
        for(const auto& file : dir.getFiles()){
            // Avoid recursion loops through linked directories.
            if(file.isLink()) continue;
            const std::string path = relativeDir + "/" + file.getFileName();
            if(file.isDirectory()){
                scan(path);
                continue;
            }
            const std::string ext = ofToLower(file.getExtension());
            if(ext != "ttf" && ext != "otf") continue;
            FT_Face face = nullptr;
            if(FT_New_Face(library, ofToDataPath(path, true).c_str(), 0, &face) != 0) continue;
            const std::string familyName = themeFontFamilyName(face);
            if(FT_IS_SCALABLE(face) && !familyName.empty()){
                auto& candidate = families[familyName];
                candidate.family.name = familyName;
                const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
                const int weight = os2 && os2->usWeightClass ? os2->usWeightClass
                    : ((face->style_flags & FT_STYLE_FLAG_BOLD) ? 700 : 400);
                const bool italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
                // Prefer upright static faces. Variable fonts can still supply
                // their default instance when no static equivalent is present.
                const int penalty = (italic ? 2000 : 0) + (FT_HAS_MULTIPLE_MASTERS(face) ? 1000 : 0);
                const int regularScore = penalty + std::abs(weight - 400);
                if(regularScore < candidate.regularScore){
                    candidate.regularScore = regularScore;
                    candidate.family.regularPath = path;
                }
                const int boldScore = penalty + std::abs(weight - 700);
                if(!italic && weight >= 600 && boldScore < candidate.boldScore){
                    candidate.boldScore = boldScore;
                    candidate.family.boldPath = path;
                }
            }
            FT_Done_Face(face);
        }
    };
    scan("config/font");
    FT_Done_FreeType(library);
    for(const auto& entry : families) themeFontFamilies.push_back(entry.second.family);
    pendingThemeFontChange = true;
}

void ofxOceanodeThemeManager::applyThemeFont(){
    if(!pendingThemeFontChange) return;
    pendingThemeFontChange = false;
    themeFontWarning.clear();
    if(!themeFontFamily.empty()){
        for(const auto& family : themeFontFamilies){
            if(family.name == themeFontFamily && ofFile(ofToDataPath(family.regularPath, true)).exists()){
                canvas->setupFonts(family.regularPath, family.boldPath);
                return;
            }
        }
        themeFontWarning = "Font family '" + themeFontFamily + "' is unavailable. Using the built-in default.";
        ofLogWarning("ofxOceanodeThemeManager") << themeFontWarning;
    }
    canvas->setupFonts();
}

void ofxOceanodeThemeManager::save(const std::string& name){
    ofDirectory::createDirectory(ofToDataPath(themesFolder, true), true, true);
    ofJson j;
    j["fontFamily"] = themeFontFamily;
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg];
    for(int i = 0; i < ImGuiCol_COUNT; i++){
        string colorName = ImGui::GetStyleColorName(i);
        j["colors"][colorName] = { colors[i].x, colors[i].y, colors[i].z, colors[i].w };
    }
    // Save custom Oceanode semantic colours
    for(auto& f : OceanodeColors::getFields()){
        j["oceanodeColors"][f.name] = { f.ptr->x, f.ptr->y, f.ptr->z, f.ptr->w };
    }
    string path = ofToDataPath(themesFolder + name + ".json", true);
    ofSavePrettyJson(path, j);
    currentThemeName = name;
    ofLogNotice("ofxOceanodeThemeManager") << "Theme saved to " << path;
}

void ofxOceanodeThemeManager::load(const std::string& name){
    string path = ofToDataPath(themesFolder + name + ".json", true);
    ofFile f(path);
    if(!f.exists()){
        ofLogWarning("ofxOceanodeThemeManager") << "Theme file not found: " << path;
        return;
    }
    ofJson j = ofLoadJson(path);
    if(!j.contains("colors")) return;

    // Reset to OceanodeTheme baseline before applying JSON overrides
    OceanodeTheme base;
    base.setup();
    themeFontFamily = j.contains("fontFamily") && j["fontFamily"].is_string()
        ? j["fontFamily"].get<std::string>() : "";
    pendingThemeFontChange = true;

    ImVec4* colors = ImGui::GetStyle().Colors;
    for(int i = 0; i < ImGuiCol_COUNT; i++){
        string colorName = ImGui::GetStyleColorName(i);
        if(j["colors"].contains(colorName)){
            auto& arr = j["colors"][colorName];
            if(arr.is_array() && arr.size() == 4){
                colors[i] = ImVec4(arr[0].get<float>(), arr[1].get<float>(),
                                   arr[2].get<float>(), arr[3].get<float>());
            }
        }
    }
    // Migrate themes saved with the former ofxImGui extension. Explicit Oceanode
    // entries below take precedence; older themes inherit their own text palette.
    OceanodeColors::TabText = colors[ImGuiCol_TextDisabled];
    OceanodeColors::TabSelectedText = colors[ImGuiCol_Text];
    for(const auto& legacy : {std::make_pair("TabText", &OceanodeColors::TabText),
                             std::make_pair("TabSelectedText", &OceanodeColors::TabSelectedText)}){
        if(j["colors"].contains(legacy.first)){
            const auto& arr = j["colors"][legacy.first];
            if(arr.is_array() && arr.size() == 4)
                *legacy.second = ImVec4(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>(), arr[3].get<float>());
        }
    }
    // Old themes may contain a separate PopupBg; WindowBg now controls both.
    colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg];
    // Load custom Oceanode semantic colours (optional: gracefully absent in old theme files)
    if(j.contains("oceanodeColors")){
        for(auto& field : OceanodeColors::getFields()){
            if(j["oceanodeColors"].contains(field.name)){
                auto& arr = j["oceanodeColors"][field.name];
                if(arr.is_array() && arr.size() == 4){
                    *field.ptr = ImVec4(arr[0].get<float>(), arr[1].get<float>(),
                                       arr[2].get<float>(), arr[3].get<float>());
                }
            }
        }
    }
    currentThemeName = name;
    ofLogNotice("ofxOceanodeThemeManager") << "Theme loaded from " << path;
}

void ofxOceanodeThemeManager::drawThemeEditorWindow(){
    std::string editorTitle = "Theme Editor";
    if(!currentThemeName.empty()) editorTitle += " — " + currentThemeName;
    editorTitle += "###ThemeEditorWindow";

    ImGui::SetNextWindowSize(ImVec2(480, 620), ImGuiCond_FirstUseEver);
    const bool editorVisible = ImGui::Begin(editorTitle.c_str(), &showThemeEditor);
    themeEditorHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
                                                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if(editorVisible){
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        bool colorFilterChanged = false;

        if(currentThemeName.empty()){
            ImGui::TextDisabled("OceanodeTheme (built-in)");
        } else {
            ImGui::TextDisabled("Current theme: %s", currentThemeName.c_str());
        }
        ImGui::Spacing();

        if(ImGui::Button("Reset to Default")){
            OceanodeTheme defaultTheme;
            defaultTheme.setup();
            themeFontFamily.clear();
            pendingThemeFontChange = true;
            currentThemeName = "OceanodeTheme (built-in)";
        }

        ImGui::SameLine();
        bool focusColorSearch = false;
        if(ImGui::Button("[?]")){
            showThemeColorSearch = !showThemeColorSearch;
            focusColorSearch = showThemeColorSearch;
            if(!showThemeColorSearch) themeColorSearch[0] = '\0';
            colorFilterChanged = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Filter color names in both color tabs");
        if(showThemeColorSearch){
            const float clearWidth = ImGui::CalcTextSize("Clear").x + style.FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - clearWidth - style.ItemSpacing.x));
            if(focusColorSearch) ImGui::SetKeyboardFocusHere();
            colorFilterChanged |= ImGui::InputTextWithHint("##theme_color_search", "Filter color names...",
                                                          themeColorSearch, sizeof(themeColorSearch));
            ImGui::SameLine();
            if(ImGui::Button("Clear")){
                themeColorSearch[0] = '\0';
                colorFilterChanged = true;
            }
        }

        ImGui::TextDisabled("Hold Alt and hover any element to find its color.");
        if(!themeColorPickLayers.empty()) drawThemeColorPickLayers(true);

        ImGui::Separator();
        ImGui::Spacing();

        // Rows of the colour picked with Alt+hover (or from the list above).
        auto beginPickedRow = [&](){
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + ImGui::GetContentRegionAvail().x, min.y + ImGui::GetFrameHeight());
            ImVec4 fill = OceanodeColors::SelectedBorder;
            fill.w *= 0.25f;
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, OceanodeColors::U32(fill), style.FrameRounding);
            ImGui::GetWindowDrawList()->AddRect(min, max, OceanodeColors::U32(OceanodeColors::SelectedBorder), style.FrameRounding);
        };
        auto endPickedRow = [&](){
            if(!themeColorRevealPending) return;
            ImGui::SetScrollHereY(0.5f);
            themeColorRevealPending = false;
        };
        auto tabFlags = [&](bool oceanode){
            return themeColorRevealPending && themeSelectedColor.oceanode == oceanode ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        };

        if(ImGui::BeginTabBar("##themeditor_tabs")){

            if(ImGui::BeginTabItem("ImGui Colors", nullptr, tabFlags(false))){
                ImGui::TextDisabled("WindowBg also controls popup and modal backgrounds.");
                std::vector<int> colorIndices;
                for(int i = 0; i < ImGuiCol_COUNT; i++){
                    if(i != ImGuiCol_PopupBg) colorIndices.push_back(i);
                }
                std::sort(colorIndices.begin(), colorIndices.end(), [](int a, int b){
                    return strcmp(ImGui::GetStyleColorName(a), ImGui::GetStyleColorName(b)) < 0;
                });

                ImGui::BeginChild("##colorscroll_imgui", ImVec2(0, 0), false);
                if(colorFilterChanged || ImGui::IsWindowAppearing()) ImGui::SetScrollY(0.0f);
                int visibleColors = 0;
                for(int idx : colorIndices){
                    const char* colorName = ImGui::GetStyleColorName(idx);
                    if(themeColorSearch[0] && ImStristr(colorName, nullptr, themeColorSearch, nullptr) == nullptr) continue;
                    ++visibleColors;
                    const bool picked = !themeSelectedColor.oceanode && themeSelectedColor.name == colorName;
                    if(picked) beginPickedRow();
                    ImGui::PushID(idx);
                    if(ImGui::ColorEdit4(colorName, (float*)&colors[idx],
                                       ImGuiColorEditFlags_AlphaBar |
                                       ImGuiColorEditFlags_AlphaPreviewHalf) && idx == ImGuiCol_WindowBg){
                        colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg];
                    }
                    ImGui::PopID();
                    if(picked) endPickedRow();
                }
                if(visibleColors == 0) ImGui::TextDisabled("No matching colors.");
                if(!themeSelectedColor.oceanode) themeColorRevealPending = false; // not listed
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Oceanode Colors", nullptr, tabFlags(true))){
                ImGui::TextDisabled("OC_TabSelectedText: visible tab. OC_TabText: other tabs.");
                ImGui::BeginChild("##colorscroll_oc", ImVec2(0, 0), false);
                if(colorFilterChanged || ImGui::IsWindowAppearing()) ImGui::SetScrollY(0.0f);
                // Shared window/popup decoration sizes (colours: OC_FloatingWindow* below)
                if(themeColorSearch[0] == '\0'){
                    ImGui::Checkbox("Floating window / popup border and shadow", &ofxOceanodeFloatingWindowDecor::Enabled);
                    ImGui::SliderFloat("Window / popup border thickness", &ofxOceanodeFloatingWindowDecor::BorderThickness, 0.0f, 4.0f, "%.1f px");
                    ImGui::SliderFloat("Window / popup shadow size", &ofxOceanodeFloatingWindowDecor::ShadowSize, 0.0f, 40.0f, "%.0f px");
                    ImGui::Separator();
                }
                auto fields = OceanodeColors::getFields();
                std::sort(fields.begin(), fields.end(), [](const OceanodeColors::ColorField& a, const OceanodeColors::ColorField& b){
                    return strcmp(a.name, b.name) < 0;
                });
                int visibleColors = 0;
                for(int i = 0; i < (int)fields.size(); i++){
                    if(themeColorSearch[0] && ImStristr(fields[i].name, nullptr, themeColorSearch, nullptr) == nullptr) continue;
                    ++visibleColors;
                    const bool picked = themeSelectedColor.oceanode && themeSelectedColor.name == fields[i].name;
                    if(picked) beginPickedRow();
                    ImGui::PushID(i + ImGuiCol_COUNT);
                    ImGui::ColorEdit4(fields[i].name, (float*)fields[i].ptr,
                                      ImGuiColorEditFlags_AlphaBar |
                                      ImGuiColorEditFlags_AlphaPreviewHalf);
                    ImGui::PopID();
                    if(picked) endPickedRow();
                }
                if(visibleColors == 0) ImGui::TextDisabled("No matching colors.");
                if(themeSelectedColor.oceanode) themeColorRevealPending = false; // not listed
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Font")){
                ImGui::TextUnformatted("Default font family");
                ImGui::TextDisabled("Fonts from bin/data/config/font (including subfolders)");
                if(ImGui::Button("Refresh font list")) refreshThemeFonts();
                ImGui::TextWrapped("Choose a family to preview it throughout the app. Use Theme > Save Theme to keep your choice.");
                if(!themeFontWarning.empty()) ImGui::TextWrapped("%s", themeFontWarning.c_str());
                ImGui::Separator();
                ImGui::BeginChild("##fontfamilies", ImVec2(0, 0), false);
                if(ImGui::Selectable("Built-in default (JetBrains Mono)", themeFontFamily.empty())){
                    themeFontFamily.clear();
                    pendingThemeFontChange = true;
                }
                for(const auto& family : themeFontFamilies){
                    ImGui::PushID(family.name.c_str());
                    if(ImGui::Selectable(family.name.c_str(), themeFontFamily == family.name)){
                        themeFontFamily = family.name;
                        pendingThemeFontChange = true;
                    }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", family.regularPath.c_str());
                    ImGui::PopID();
                }
                if(themeFontFamilies.empty()){
                    ImGui::TextWrapped("No usable fonts found. Add .ttf or .otf files to bin/data/config/font, then refresh the list.");
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void ofxOceanodeThemeManager::selectThemeColor(const ofxOceanodeThemeColorPicker::Match& match){
    themeSelectedColor = match;
    themeColorRevealPending = true;
    if(themeColorSearch[0] && ImStristr(match.name.c_str(), nullptr, themeColorSearch, nullptr) == nullptr){
        themeColorSearch[0] = '\0';
    }
}

void ofxOceanodeThemeManager::pickThemeColorUnderMouse(){
    const ImGuiIO& io = ImGui::GetIO();
    if(!showThemeEditor || !io.KeyAlt || themeEditorHovered || !ImGui::IsMousePosValid()) return;
    ImGuiContext& g = *GImGui;
    ImGuiViewport* viewport = g.MouseViewport ? g.MouseViewport : ImGui::GetMainViewport();
    auto layers = ofxOceanodeThemeColorPicker::pick(viewport ? viewport->DrawData : nullptr, io.MousePos,
                                                    themeColorPickTooltipDrawList);
    if(layers.empty()) return;
    themeColorPickLayers = std::move(layers);
    themeColorPickFrame = ImGui::GetFrameCount();
    // Select the topmost colour that belongs to the theme.
    for(const auto& layer : themeColorPickLayers){
        if(layer.matches.empty()) continue;
        if(!(layer.matches.front() == themeSelectedColor)) selectThemeColor(layer.matches.front());
        break;
    }
}

void ofxOceanodeThemeManager::drawThemeColorPickTooltip(){
    themeColorPickTooltipDrawList = nullptr;
    if(!ImGui::GetIO().KeyAlt || themeColorPickLayers.empty() ||
       themeColorPickFrame != ImGui::GetFrameCount() - 1) return;
    if(!ImGui::BeginTooltip()) return;
    // The lookup must not find its own tooltip.
    themeColorPickTooltipDrawList = ImGui::GetWindowDrawList();
    ImGui::TextDisabled("Theme colors under the cursor, top first");
    drawThemeColorPickLayers(false);
    ImGui::EndTooltip();
}

void ofxOceanodeThemeManager::drawThemeColorPickLayers(bool selectable){
    const ImGuiStyle& style = ImGui::GetStyle();
    const float swatchSize = selectable ? ImGui::GetFrameHeight() : ImGui::GetTextLineHeight();
    for(int i = 0; i < (int)themeColorPickLayers.size(); i++){
        const auto& layer = themeColorPickLayers[i];
        ImGui::PushID(i);
        const float rowStart = ImGui::GetCursorPosX();
        const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        ImGui::ColorButton("##swatch", ImGui::ColorConvertU32ToFloat4(layer.color),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreviewHalf,
                           ImVec2(swatchSize, swatchSize));
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ofxOceanodeThemeColorPicker::hex(layer.color).c_str());
        if(layer.matches.empty()){
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s  not a theme color", ofxOceanodeThemeColorPicker::hex(layer.color).c_str());
        }
        for(int m = 0; m < (int)layer.matches.size(); m++){
            const auto& match = layer.matches[m];
            const bool selected = match == themeSelectedColor;
            const ImVec2 size = ImGui::CalcTextSize(match.name.c_str());
            ImGui::SameLine();
            if(selectable && ImGui::GetCursorScreenPos().x + size.x > rowRight){
                ImGui::NewLine();
                ImGui::SetCursorPosX(rowStart + swatchSize + style.ItemSpacing.x);
            }
            if(selectable){
                ImGui::PushID(m);
                ImGui::AlignTextToFramePadding();
                if(ImGui::Selectable(match.name.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(size.x, 0))){
                    selectThemeColor(match);
                }
                if(ImGui::IsItemHovered()) ImGui::SetTooltip(match.oceanode ? "Oceanode Colors" : "ImGui Colors");
                ImGui::PopID();
            }else if(selected){
                ImGui::TextColored(OceanodeColors::SelectedNodeText, "%s", match.name.c_str());
            }else{
                ImGui::TextUnformatted(match.name.c_str());
            }
        }
        if(!layer.exactAlpha){
            ImGui::SameLine();
            ImGui::TextDisabled("(other alpha)");
        }
        ImGui::PopID();
    }
}

void ofxOceanodeThemeManager::drawMenuSection(){
    ofxOceanodeMenu::section("Theme");
    if(!currentThemeName.empty()){
        ImGui::TextDisabled("Current: %s", currentThemeName.c_str());
    }
    if(ImGui::MenuItem("Edit Theme", nullptr, &showThemeEditor)){}
    if(ImGui::MenuItem("Save Theme...")){
        memset(themeNameBuf, 0, sizeof(themeNameBuf));
        if(!currentThemeName.empty()){
            strncpy(themeNameBuf, currentThemeName.c_str(), sizeof(themeNameBuf) - 1);
        }
        themeSaveError.clear();
        openSaveThemePopup = true;
    }
    if(ImGui::BeginMenu("Load Theme...")){
        ofDirectory themeDir(ofToDataPath("config/themes", true));
        themeDir.allowExt("json");
        themeDir.listDir();
        if(themeDir.size() == 0){
            ImGui::BeginDisabled();
            ImGui::MenuItem("(no themes saved)");
            ImGui::EndDisabled();
        } else {
            for(int i = 0; i < (int)themeDir.size(); i++){
                std::string name = themeDir.getFile(i).getBaseName();
                bool isCurrent = (name == currentThemeName);
                if(isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, OceanodeColors::ActiveItemHighlight);
                if(ImGui::MenuItem(name.c_str())){
                    load(name);
                }
                if(isCurrent) ImGui::PopStyleColor();
            }
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Set default theme")){
        ofDirectory dir(ofToDataPath("config/themes", true));
        dir.allowExt("json");
        dir.listDir();

        if(dir.size() == 0){
            ImGui::TextDisabled("No themes saved yet");
        }
        for(int i = 0; i < (int)dir.size(); i++){
            std::string name = dir.getFile(i).getBaseName();
            bool isDefault = (name == defaultThemeName);
            if(isDefault) ImGui::PushStyleColor(ImGuiCol_Text, OceanodeColors::ActiveItemHighlight);
            if(ImGui::MenuItem(name.c_str())){
                setDefaultTheme(name);
            }
            if(isDefault) ImGui::PopStyleColor();
        }
        ImGui::EndMenu();
    }
    if(ImGui::MenuItem("Reset to Built-in Theme")){
        OceanodeTheme base;
        base.setup();
        themeFontFamily.clear();
        pendingThemeFontChange = true;
        currentThemeName = "OceanodeTheme (built-in)";
    }
}

void ofxOceanodeThemeManager::drawPopups(){
    if(openSaveThemePopup){
        ImGui::OpenPopup("Save Theme##modal");
        openSaveThemePopup = false;
    }
    if(ImGui::BeginPopupModal("Save Theme##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Enter a name for the theme:");
        ImGui::SetNextItemWidth(260);
        ImGui::InputText("##themename", themeNameBuf, sizeof(themeNameBuf));
        if(!themeSaveError.empty()) ImGui::TextWrapped("%s", themeSaveError.c_str());
        ImGui::Spacing();
        bool canSave = (strlen(themeNameBuf) > 0);
        if(!canSave) ImGui::BeginDisabled();
        if(ImGui::Button("Save", ImVec2(120, 0))){
            const std::string name(themeNameBuf);
            if(validThemeName(name)){
                save(name);
                ImGui::CloseCurrentPopup();
            }else themeSaveError = "Enter a name (1-255 bytes, without ##, line breaks, surrounding spaces, /, \\ or :).";
        }
        if(!canSave) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
