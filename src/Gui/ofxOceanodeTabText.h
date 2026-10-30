#pragma once

#include "imgui_internal.h"
#include "ofxOceanodeColors.h"
#include <map>
#include <vector>

// Oceanode-only tab text styling. ImGui has no separate tab-label style colors.
// At the end of the frame, recolor the font glyph quads already emitted inside
// current tab headers. Keep ImGui's clipping, ellipsis and interaction logic;
// solid geometry, close icons, images and window contents are left untouched.
// No changes to ImGui's enums, ABI or renderer are needed.
namespace ofxOceanodeTabText {
namespace detail {
struct Region {
    ImRect rect;
    ImU32 color;
    int sourceAlpha;
};

inline void appendRegions(ImGuiContext& g, ImGuiTabBar* bar,
                          std::map<ImDrawList*, std::vector<Region>>& lists){
    if(!bar || bar->CurrFrameVisible != g.FrameCount || !bar->Window) return;
    for(const auto& tab : bar->Tabs){
        if(tab.LastFrameVisible != g.FrameCount ||
           (tab.Flags & (ImGuiTabItemFlags_Button | ImGuiTabItemFlags_Invisible))) continue;
        const bool central = (tab.Flags & ImGuiTabItemFlags_SectionMask_) == 0;
        const float x = bar->BarRect.Min.x + (central ? IM_TRUNC(tab.Offset - bar->ScrollingAnim) : tab.Offset);
        ImRect rect(ImVec2(x, bar->BarRect.Min.y), ImVec2(x + tab.Width, bar->BarRect.Max.y));
        // The docked window records the actual item rectangle, including any
        // layout changes made by ImGui while submitting the tab bar.
        if(tab.Window) rect = tab.Window->DC.DockTabItemRect;
        rect.ClipWith(bar->BarRect);
        if(central){
            rect.Min.x = ImMax(rect.Min.x, bar->ScrollingRectMinX);
            rect.Max.x = ImMin(rect.Max.x, bar->ScrollingRectMaxX);
        }
        if(rect.IsInverted() || rect.GetWidth() <= 0) continue;
        const bool visible = bar->VisibleTabId == tab.ID ||
            (bar->SelectedTabId == 0 && bar->Tabs.Size == 1);
        const ImU32 source = tab.Window ? tab.Window->DockStyle.Colors[ImGuiWindowDockStyleCol_Text]
            : ImGui::ColorConvertFloat4ToU32(g.Style.Colors[ImGuiCol_Text]);
        lists[bar->Window->DrawList].push_back({rect,
            OceanodeColors::U32(visible ? OceanodeColors::TabSelectedText : OceanodeColors::TabText),
            int((source >> IM_COL32_A_SHIFT) & 0xFF)});
    }
}

inline void recolor(ImDrawList& list, const std::vector<Region>& regions){
    for(const auto& cmd : list.CmdBuffer){
        // Only the font atlas: user images and draw callbacks are unrelated.
        if(cmd.UserCallback || !cmd.TexRef._TexData) continue;
        const unsigned end = cmd.IdxOffset + cmd.ElemCount;
        for(unsigned i = cmd.IdxOffset; i + 5 < end; ){
            const auto* idx = list.IdxBuffer.Data + i;
            if(idx[0] != idx[3] || idx[2] != idx[4]){ i += 3; continue; }
            ImDrawVert* v[4] = {&list.VtxBuffer[cmd.VtxOffset + idx[0]],
                               &list.VtxBuffer[cmd.VtxOffset + idx[1]],
                               &list.VtxBuffer[cmd.VtxOffset + idx[2]],
                               &list.VtxBuffer[cmd.VtxOffset + idx[5]]};
            i += 6;
            // Font glyphs are axis-aligned textured quads. Filled shapes use
            // the same UV at every corner; textured AA lines aren't glyph quads.
            if(v[0]->pos.y != v[1]->pos.y || v[1]->pos.x != v[2]->pos.x ||
               v[2]->pos.y != v[3]->pos.y || v[3]->pos.x != v[0]->pos.x ||
               v[0]->uv.y != v[1]->uv.y || v[1]->uv.x != v[2]->uv.x ||
               v[2]->uv.y != v[3]->uv.y || v[3]->uv.x != v[0]->uv.x ||
               v[0]->uv.x == v[2]->uv.x || v[0]->uv.y == v[2]->uv.y) continue;
            const ImVec2 center((v[0]->pos.x + v[2]->pos.x) * 0.5f,
                                (v[0]->pos.y + v[2]->pos.y) * 0.5f);
            for(const auto& region : regions){
                if(!region.rect.Contains(center) || region.sourceAlpha == 0) continue;
                for(auto* vertex : v){
                    const int originalAlpha = (vertex->col >> IM_COL32_A_SHIFT) & 0xFF;
                    const int targetAlpha = (region.color >> IM_COL32_A_SHIFT) & 0xFF;
                    // Preserve global/disabled opacity and any vertex fading,
                    // while replacing the source text's own color and alpha.
                    const int alpha = ImMin(255, (originalAlpha * targetAlpha + region.sourceAlpha / 2) / region.sourceAlpha);
                    vertex->col = (region.color & ~IM_COL32_A_MASK) | (ImU32(alpha) << IM_COL32_A_SHIFT);
                }
                break;
            }
        }
    }
}

inline void onEndFrame(ImGuiContext* ctx, ImGuiContextHook*){
    std::map<ImDrawList*, std::vector<Region>> lists;
    for(const auto& entry : ctx->DockContext.Nodes.Data){
        auto* node = static_cast<ImGuiDockNode*>(entry.val_p);
        if(node && node->LastFrameActive == ctx->FrameCount && !node->IsHiddenTabBar() && !node->IsNoTabBar())
            appendRegions(*ctx, node->TabBar, lists);
    }
    for(int i = 0; i < ctx->TabBars.GetMapSize(); ++i)
        appendRegions(*ctx, ctx->TabBars.TryGetMapData(i), lists);
    for(auto& entry : lists) recolor(*entry.first, entry.second);
}
} // namespace detail

inline void install(ImGuiContext* ctx = ImGui::GetCurrentContext()){
    if(!ctx) return;
    const ImGuiID owner = ImHashStr("OceanodeTabText");
    for(const auto& hook : ctx->Hooks)
        if(hook.Owner == owner && hook.Type == ImGuiContextHookType_EndFramePost) return;
    ImGuiContextHook hook;
    hook.Owner = owner;
    hook.Type = ImGuiContextHookType_EndFramePost;
    hook.Callback = &detail::onEndFrame;
    ImGui::AddContextHook(ctx, &hook);
}
} // namespace ofxOceanodeTabText
