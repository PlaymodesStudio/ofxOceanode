#include "ofxOceanodeDockCollapse.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

ofxOceanodeDockCollapse::~ofxOceanodeDockCollapse() {
    if(!settingsContext) return;
    auto* previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(settingsContext);
    ImGui::RemoveSettingsHandler("OceanodeDockCollapse");
    ImGui::RemoveContextHook(settingsContext, shutdownHook);
    ImGui::SetCurrentContext(previous);
}

void ofxOceanodeDockCollapse::installSettings() {
    if(settingsContext == GImGui) return;
    IM_ASSERT(!settingsContext && GImGui);
    settingsContext = GImGui;
    ImGuiSettingsHandler handler;
    handler.TypeName = "OceanodeDockCollapse";
    handler.TypeHash = ImHashStr(handler.TypeName);
    handler.UserData = this;
    handler.ClearAllFn = handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* h) {
        static_cast<ofxOceanodeDockCollapse*>(h->UserData)->reset();
    };
    handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* h, const char* name) -> void* {
        auto* self = static_cast<ofxOceanodeDockCollapse*>(h->UserData);
        if(std::strcmp(name, "State") != 0) return nullptr;
        self->restoreAfterFrame = ctx->FrameCount + 2;
        return self;
    };
    handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        static_cast<ofxOceanodeDockCollapse*>(entry)->readSetting(line);
    };
    handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* out) {
        static_cast<ofxOceanodeDockCollapse*>(h->UserData)->writeSettings(*out);
    };
    ImGui::AddSettingsHandler(&handler);
    ImGuiContextHook hook;
    hook.Type = ImGuiContextHookType_Shutdown;
    hook.UserData = this;
    hook.Callback = [](ImGuiContext*, ImGuiContextHook* h) {
        auto* self = static_cast<ofxOceanodeDockCollapse*>(h->UserData);
        self->settingsContext = nullptr;
        self->reset();
    };
    shutdownHook = ImGui::AddContextHook(settingsContext, &hook);
}

void ofxOceanodeDockCollapse::reset() {
    axes[0] = AxisState(); axes[1] = AxisState(); pending = Request();
    restore = SavedState(); restoreAfterFrame = 0;
    rootID = dragging = 0; enabled = false; frame = -1;
}

void ofxOceanodeDockCollapse::prepare(ImGuiID id, ImVec2 pos, ImVec2 size, ImGuiDockNodeFlags flags, ImGuiDockNode* activeSplitter) {
    rootID = id; frame = GImGui->FrameCount; dockFlags = flags;
    enabled = !ImGui::GetCurrentWindow()->SkipItems && !(flags & ImGuiDockNodeFlags_KeepAliveOnly);
    auto* root = ImGui::DockBuilderGetNode(id);
    if(!root) { axes[0] = AxisState(); axes[1] = AxisState(); pending = Request(); return; }
    for(int axis = 0; axis < 2; ++axis) {
        auto& state = axes[axis];
        if(!state.layout.empty() && !matches(root, state.signature)) state = AxisState();
        if(activeSplitter && activeSplitter->SplitAxis == axis && !state.layout.empty()) {
            // A manual divider drag takes over this axis from the fold policy.
            syncReferences(root, axis); state = AxisState();
        }
    }
    if(!enabled) { pending = Request(); return; }
    if(pending.id || activeSplitter) restore = SavedState(); // User interaction takes priority.
    tryRestore(root);
    if(pending.id) {
        auto* n = ImGui::DockBuilderGetNode(pending.id);
        if(matches(root, pending.signature) && eligible(n)) {
            const int axis = n->SplitAxis;
            auto& state = axes[axis];
            if(!pending.toggle) {
                syncReferences(root, axis); state = AxisState();
                manualResize(n, axis, pending.firstSize);
                ImGui::MarkIniSettingsDirty();
            } else {
                if(state.layout.empty()) { capture(root, axis, state.layout); signature(root, state.signature); }
                std::vector<ImGuiID> targets;
                collectTargets(root, n, direction(n, root), targets);
                if(!targets.empty()) {
                    const bool allFolded = std::all_of(targets.begin(), targets.end(), [&](ImGuiID target) { return isFolded(state, target); });
                    if(allFolded) {
                        // Expanding an inner divider also opens its portion
                        // of a larger fold, without opening neighbouring panes.
                        for(auto& fold : state.folds)
                            fold.targets.erase(std::remove_if(fold.targets.begin(), fold.targets.end(), [&](ImGuiID target) {
                                return std::find(targets.begin(), targets.end(), target) != targets.end();
                            }), fold.targets.end());
                        state.folds.erase(std::remove_if(state.folds.begin(), state.folds.end(), [](const Fold& f) { return f.targets.empty(); }), state.folds.end());
                    } else state.folds.push_back({n->ID, std::move(targets)});
                    ImGui::MarkIniSettingsDirty();
                }
            }
        }
        pending = Request();
    }
    for(int axis = 0; axis < 2; ++axis) {
        auto& state = axes[axis];
        if(state.layout.empty()) continue;
        expanded(root, axis, size[axis], state);
        preferred(root, axis, state);
        apply(root, axis, pos[axis], size[axis], state);
    }
}

void ofxOceanodeDockCollapse::finish() {
    auto* root = ImGui::DockBuilderGetNode(rootID);
    for(int axis = 0; axis < 2; ++axis) {
        auto& state = axes[axis];
        if(state.layout.empty()) continue;
        if(!matches(root, state.signature)) { state = AxisState(); continue; }
        // Windows still receive the folded Size, but ini serialization sees
        // expanded SizeRef. Never change the global WindowMinSize.
        for(const auto& entry : state.layout)
            ImGui::DockBuilderGetNode(entry.id)->SizeRef[axis] = entry.expanded;
    }
}

void ofxOceanodeDockCollapse::drawHandles() {
    auto* root = ImGui::DockBuilderGetNode(rootID);
    for(auto& state : axes) {
        if(!state.layout.empty() && matches(root, state.signature))
            for(const auto& entry : state.layout) {
                auto* n = ImGui::DockBuilderGetNode(entry.id);
                n->SetLocalFlags((n->LocalFlags & ~tabFlags) | entry.tabFlags);
            }
        if(state.folds.empty()) state = AxisState();
    }
    if(!enabled || frame != GImGui->FrameCount || GImGui->OpenPopupStack.Size
        || GImGui->DragDropActive || GImGui->MovingWindow) return;
    if(root && root->LastFrameActive == frame) drawTree(root, root);
}

void ofxOceanodeDockCollapse::readSetting(const char* line) {
    unsigned int a, b; int axis;
    if(std::sscanf(line, "Root=%x", &a) == 1) restore.root = a;
    else if(std::sscanf(line, "Window=%x,%x", &a, &b) == 2) restore.windows.push_back({a, b});
    else if(std::sscanf(line, "Fold=%d,%x,%x", &axis, &a, &b) == 3 && axis >= 0 && axis < 2)
        restore.folds.push_back({axis, a, b});
}

void ofxOceanodeDockCollapse::saveWindows(ImGuiDockNode* n, std::vector<SavedWindow>& out) {
    if(!n->IsVisible) return;
    for(auto* w : n->Windows) if(w->Active || w->WasActive) out.push_back({w->ID, n->ID});
    if(n->IsSplitNode()) { saveWindows(n->ChildNodes[0], out); saveWindows(n->ChildNodes[1], out); }
}

void ofxOceanodeDockCollapse::writeSettings(ImGuiTextBuffer& out) const {
    SavedState saved = restore;
    if(saved.folds.empty()) {
        auto* root = ImGui::DockBuilderGetNode(rootID);
        if(!root) return;
        saved.root = rootID;
        for(int axis = 0; axis < 2; ++axis) if(matches(root, axes[axis].signature))
            for(const auto& fold : axes[axis].folds)
                for(auto target : fold.targets) saved.folds.push_back({axis, fold.id, target});
        saveWindows(root, saved.windows);
    }
    if(saved.folds.empty()) return;
    out.appendf("[OceanodeDockCollapse][State]\nRoot=%08X\n", saved.root);
    for(const auto& w : saved.windows) out.appendf("Window=%08X,%08X\n", w.window, w.dock);
    for(const auto& f : saved.folds) out.appendf("Fold=%d,%08X,%08X\n", f.axis, f.split, f.target);
    out.append("\n");
}

void ofxOceanodeDockCollapse::tryRestore(ImGuiDockNode* root) {
    if(restore.folds.empty() || restore.root != rootID || frame <= restoreAfterFrame) return;
    // Ini loads precede window visibility restoration and node creation.
    // Wait for those windows to have actually submitted in their saved docks.
    for(const auto& saved : restore.windows) {
        auto* w = ImGui::FindWindowByID(saved.window);
        if(!w || !w->WasActive || !w->DockNode || w->DockNode->ID != saved.dock) return;
    }
    for(const auto& f : restore.folds) {
        auto* split = ImGui::DockBuilderGetNode(f.split);
        auto* target = ImGui::DockBuilderGetNode(f.target);
        if(!split || !target) return;
        if(ImGui::DockNodeGetRootNode(split) != root || ImGui::DockNodeGetRootNode(target) != root
            || split->SplitAxis != f.axis || target->IsSplitNode() || target->IsCentralNode()) {
            restore = SavedState(); return;
        }
        if(!eligible(split) || !target->IsVisible) return;
    }
    for(const auto& f : restore.folds) {
        auto& state = axes[f.axis];
        if(state.layout.empty()) { capture(root, f.axis, state.layout); signature(root, state.signature); }
        state.folds.push_back({f.split, {f.target}});
    }
    restore = SavedState();
}

bool ofxOceanodeDockCollapse::central(ImGuiDockNode* n) {
    return n->IsCentralNode() || (n->IsSplitNode() && (central(n->ChildNodes[0]) || central(n->ChildNodes[1])));
}

ImGuiDockNode* ofxOceanodeDockCollapse::findCentral(ImGuiDockNode* n) {
    if(n->IsCentralNode()) return n;
    if(!n->IsSplitNode()) return nullptr;
    if(auto* c = findCentral(n->ChildNodes[0])) return c;
    return findCentral(n->ChildNodes[1]);
}

int ofxOceanodeDockCollapse::direction(ImGuiDockNode* n, ImGuiDockNode* root) {
    if(central(n->ChildNodes[0])) return 1;
    if(central(n->ChildNodes[1])) return 0;
    auto* center = findCentral(root);
    if(!center) center = root;
    const int axis = n->SplitAxis;
    const float line = n->ChildNodes[0]->Pos[axis] + n->ChildNodes[0]->Size[axis];
    return line < center->Pos[axis] + center->Size[axis] * 0.5f ? 0 : 1;
}

void ofxOceanodeDockCollapse::signature(ImGuiDockNode* n, std::vector<ImGuiID>& out) {
    out.push_back(n->ID); out.push_back(ImGuiID(n->SplitAxis + 1));
    out.push_back(n->IsVisible); out.push_back(n->Windows.Size);
    std::vector<ImGuiID> windows;
    for(auto* w : n->Windows) windows.push_back(w->ID);
    std::sort(windows.begin(), windows.end());
    out.insert(out.end(), windows.begin(), windows.end());
    if(n->IsSplitNode()) { signature(n->ChildNodes[0], out); signature(n->ChildNodes[1], out); }
}

bool ofxOceanodeDockCollapse::matches(ImGuiDockNode* n, const std::vector<ImGuiID>& expected) {
    if(!n) return false;
    std::vector<ImGuiID> current; signature(n, current); return current == expected;
}

bool ofxOceanodeDockCollapse::eligible(ImGuiDockNode* n) const {
    if(!n || !n->HostWindow || !n->IsVisible || !n->IsSplitNode()) return false;
    auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
    const auto flags = dockFlags | a->MergedFlags | b->MergedFlags;
    return a->IsVisible && b->IsVisible && ImGui::GetStyle().DockingSeparatorSize >= 1.0f
        && !(flags & (ImGuiDockNodeFlags_NoResize | (n->SplitAxis == ImGuiAxis_X ? ImGuiDockNodeFlags_NoResizeX : ImGuiDockNodeFlags_NoResizeY)));
}

void ofxOceanodeDockCollapse::collectTargets(ImGuiDockNode* leaf, ImGuiDockNode* split, int side, std::vector<ImGuiID>& targets) {
    if(!leaf->IsVisible) return;
    if(leaf->IsSplitNode()) {
        collectTargets(leaf->ChildNodes[0], split, side, targets);
        collectTargets(leaf->ChildNodes[1], split, side, targets); return;
    }
    if(leaf->IsCentralNode()) return;
    const int axis = split->SplitAxis, cross = axis ^ 1;
    const float cut = split->ChildNodes[side]->Pos[axis] + (side == 0 ? split->ChildNodes[0]->Size[axis] : 0.0f);
    const bool outward = side == 0 ? leaf->Pos[axis] + leaf->Size[axis] <= cut + 1.0f : leaf->Pos[axis] >= cut - 1.0f;
    if(outward && leaf->Pos[cross] >= split->Pos[cross] - 1.0f
        && leaf->Pos[cross] + leaf->Size[cross] <= split->Pos[cross] + split->Size[cross] + 1.0f)
        targets.push_back(leaf->ID);
}

bool ofxOceanodeDockCollapse::isFolded(const AxisState& state, ImGuiID id) {
    for(const auto& fold : state.folds)
        if(std::find(fold.targets.begin(), fold.targets.end(), id) != fold.targets.end()) return true;
    return false;
}

ofxOceanodeDockCollapse::Layout& ofxOceanodeDockCollapse::entry(AxisState& state, ImGuiDockNode* n) {
    auto it = std::find_if(state.layout.begin(), state.layout.end(), [&](const Layout& e) { return e.id == n->ID; });
    IM_ASSERT(it != state.layout.end()); return *it;
}

void ofxOceanodeDockCollapse::capture(ImGuiDockNode* n, int axis, std::vector<Layout>& out) {
    out.push_back({n->ID, n->Size[axis], n->Size[axis], n->Size[axis], n->LocalFlags & tabFlags});
    if(n->IsSplitNode()) { capture(n->ChildNodes[0], axis, out); capture(n->ChildNodes[1], axis, out); }
}

float ofxOceanodeDockCollapse::minimum(ImGuiDockNode* n, int axis, bool compact) {
    if(!n->IsSplitNode()) {
        if(axis == ImGuiAxis_X) return ImGui::GetStyle().WindowMinSize.x;
        float height = ImGui::GetFrameHeight();
        for(auto* w : n->Windows) height = std::max(height, w->TitleBarHeight + w->MenuBarHeight);
        height = std::ceil(height + std::max(0.0f, ImGui::GetStyle().WindowRounding - 1.0f));
        return compact ? height : std::max(height, ImGui::GetStyle().WindowMinSize.y);
    }
    auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
    if(!a->IsVisible) return minimum(b, axis, compact);
    if(!b->IsVisible) return minimum(a, axis, compact);
    const float x = minimum(a, axis, compact), y = minimum(b, axis, compact);
    return n->SplitAxis == axis ? x + y + ImGui::GetStyle().DockingSeparatorSize : std::max(x, y);
}

float ofxOceanodeDockCollapse::clampSplit(float first, float available, float minA, float minB) {
    return available >= minA + minB ? std::max(minA, std::min(first, available - minB))
        : available * minA / std::max(1.0f, minA + minB);
}

void ofxOceanodeDockCollapse::expanded(ImGuiDockNode* n, int axis, float size, AxisState& state) {
    entry(state, n).expanded = size;
    if(!n->IsSplitNode()) return;
    auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
    if(!a->IsVisible || !b->IsVisible) { expanded(a->IsVisible ? a : b, axis, size, state); return; }
    float first = size, second = size;
    if(n->SplitAxis == axis) {
        const float available = std::max(2.0f, size - ImGui::GetStyle().DockingSeparatorSize);
        const float x = entry(state, a).original, y = entry(state, b).original;
        first = central(a) ? available - y : central(b) ? x : std::floor(available * x / std::max(1.0f, x + y) + 0.5f);
        first = clampSplit(first, available, minimum(a, axis), minimum(b, axis)); second = available - first;
    }
    expanded(a, axis, first, state); expanded(b, axis, second, state);
}

float ofxOceanodeDockCollapse::preferred(ImGuiDockNode* n, int axis, AxisState& state) {
    float value;
    if(!n->IsSplitNode()) value = isFolded(state, n->ID) ? minimum(n, axis, true) : entry(state, n).expanded;
    else {
        auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
        if(!a->IsVisible || !b->IsVisible) value = preferred(a->IsVisible ? a : b, axis, state);
        else {
            const float x = preferred(a, axis, state), y = preferred(b, axis, state);
            value = n->SplitAxis == axis ? x + y + ImGui::GetStyle().DockingSeparatorSize : std::max(x, y);
        }
    }
    entry(state, n).preferred = value; return value;
}

void ofxOceanodeDockCollapse::apply(ImGuiDockNode* n, int axis, float pos, float size, AxisState& state) {
    n->Pos[axis] = pos; n->Size[axis] = n->SizeRef[axis] = size;
    if(!n->IsSplitNode()) {
        if(isFolded(state, n->ID)) {
            n->SetLocalFlags(n->LocalFlags & ~tabFlags);
            n->WantHiddenTabBarToggle = n->WantHiddenTabBarUpdate = false;
        }
        return;
    }
    auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
    if(!a->IsVisible || !b->IsVisible) { apply(a->IsVisible ? a : b, axis, pos, size, state); return; }
    float first = size, second = size, secondPos = pos;
    if(n->SplitAxis == axis) {
        const float spacing = ImGui::GetStyle().DockingSeparatorSize;
        const float available = std::max(2.0f, size - spacing);
        const auto& x = entry(state, a); const auto& y = entry(state, b);
        if(central(a)) first = available - y.preferred;
        else if(central(b)) first = x.preferred;
        else if(x.preferred < x.expanded - 0.5f) first = x.preferred;
        else if(y.preferred < y.expanded - 0.5f) first = available - y.preferred;
        else first = std::floor(available * x.expanded / std::max(1.0f, x.expanded + y.expanded) + 0.5f);
        first = clampSplit(first, available, minimum(a, axis, true), minimum(b, axis, true));
        second = available - first; secondPos += first + spacing;
        a->WantLockSizeOnce = false; b->WantLockSizeOnce = true;
    }
    apply(a, axis, pos, first, state); apply(b, axis, secondPos, second, state);
}

void ofxOceanodeDockCollapse::syncReferences(ImGuiDockNode* n, int axis) {
    n->SizeRef[axis] = n->Size[axis];
    if(n->IsSplitNode()) { syncReferences(n->ChildNodes[0], axis); syncReferences(n->ChildNodes[1], axis); }
}

void ofxOceanodeDockCollapse::manualResize(ImGuiDockNode* n, int axis, float first) {
    AxisState state; capture(n, axis, state.layout);
    auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
    const float available = std::max(2.0f, n->Size[axis] - ImGui::GetStyle().DockingSeparatorSize);
    first = clampSplit(first, available, minimum(a, axis), minimum(b, axis));
    expanded(a, axis, first, state); preferred(a, axis, state); apply(a, axis, n->Pos[axis], first, state);
    expanded(b, axis, available - first, state); preferred(b, axis, state);
    apply(b, axis, n->Pos[axis] + first + ImGui::GetStyle().DockingSeparatorSize, available - first, state);
    a->WantLockSizeOnce = false; b->WantLockSizeOnce = true;
}

void ofxOceanodeDockCollapse::drawTree(ImGuiDockNode* n, ImGuiDockNode* root) {
    if(!n->IsVisible || !n->IsSplitNode()) return;
    if(eligible(n)) drawHandle(n, root);
    drawTree(n->ChildNodes[0], root); drawTree(n->ChildNodes[1], root);
}

void ofxOceanodeDockCollapse::placeAboveDockTree(ImGuiWindow* handle, ImGuiWindow* host) {
    // The handle is a separate input window so it can straddle the gutter.
    // Keep it immediately above its dock tree, below the next independent
    // window, instead of promoting it over every floating window each frame.
    auto* dockRoot = host->RootWindowDockTree;
    bool aboveDockRoot = false;
    for(auto* window : GImGui->Windows) {
        if(window == dockRoot) { aboveDockRoot = true; continue; }
        if(aboveDockRoot && window != handle && !(window->Flags & ImGuiWindowFlags_ChildWindow)
            && window->RootWindowDockTree != dockRoot) {
            ImGui::BringWindowToDisplayBehind(handle, window);
            return;
        }
    }
    // There are no independent windows above the dock tree in this case.
    ImGui::BringWindowToDisplayFront(handle);
}

void ofxOceanodeDockCollapse::drawHandle(ImGuiDockNode* n, ImGuiDockNode* root) {
    const int axis = n->SplitAxis, cross = axis ^ 1, side = direction(n, root);
    std::vector<ImGuiID> targets; collectTargets(root, n, side, targets);
    if(targets.empty()) return;
    const bool collapsed = std::all_of(targets.begin(), targets.end(), [&](ImGuiID target) { return isFolded(axes[axis], target); });
    ImVec2 pos = n->Pos, size;
    pos[axis] = n->ChildNodes[0]->Pos[axis] + n->ChildNodes[0]->Size[axis];
    const float gutter = std::max(1.0f, n->ChildNodes[1]->Pos[axis] - pos[axis]);
    size[axis] = gutter * 2.0f;
    size[cross] = std::min(n->Size[cross], std::max(12.0f, gutter * 4.0f) * 2.0f);
    pos[axis] -= (size[axis] - gutter) * 0.5f;
    pos[cross] += (n->Size[cross] - size[cross]) * 0.5f;
    char name[80]; ImFormatString(name, IM_ARRAYSIZE(name), "##OceanodeDockCollapse_%08X_%08X", rootID, n->ID);
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(n->HostWindow->Viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(4);
    placeAboveDockTree(ImGui::GetCurrentWindow(), n->HostWindow);
    const bool clicked = ImGui::InvisibleButton("##Fold", size, ImGuiButtonFlags_NoNavFocus | ImGuiButtonFlags_NoFocus);
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if(ImGui::IsItemActivated()) {
        dragging = n->ID; dragged = false; dragThresholdOffset = 0;
        dragFirstSize = n->ChildNodes[0]->Size[axis];
    }
    const float dragThreshold = std::max(12.0f, ImGui::GetIO().MouseDragThreshold);
    const bool released = ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    if(dragging == n->ID && (active || released)) {
        // Only movement along the resize axis starts a drag. Read the raw
        // delta even on release, so a quick intentional drag cannot toggle.
        const float delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f)[axis];
        if(!dragged && std::abs(delta) > dragThreshold) {
            dragged = true;
            dragThresholdOffset = std::copysign(dragThreshold, delta);
        }
        if(dragged) {
            pending.id = n->ID; pending.toggle = false;
            // Consume the dead zone instead of jumping when dragging starts.
            pending.firstSize = dragFirstSize + delta - dragThresholdOffset;
            pending.signature.clear(); signature(root, pending.signature);
        } else if(released && (clicked || ImGui::IsMouseHoveringRect(
            ImVec2(pos.x - dragThreshold, pos.y - dragThreshold),
            ImVec2(pos.x + size.x + dragThreshold, pos.y + size.y + dragThreshold), false))) {
            // Small tremors may leave the thin hit rectangle. Keep the click
            // while allowing a deliberate move away from the arrow to cancel.
            pending.id = n->ID; pending.toggle = true;
            pending.signature.clear(); signature(root, pending.signature);
        }
    }
    if(ImGui::IsItemDeactivated()) dragging = 0;
    if(active && dragged) ImGui::SetMouseCursor(axis == ImGuiAxis_X ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    else if(hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    auto* draw = ImGui::GetWindowDrawList();
    // A small black backing and a filled triangle, clipped to the enlarged
    // hit area. The floating-window shadow and border remain disabled.
    draw->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
    const ImU32 background = active ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
        : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : IM_COL32(0, 0, 0, 255);
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), background, 2.0f);
    ImVec2 tip(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), a = tip, b = tip;
    const float sign = (side == 0 ? -1.0f : 1.0f) * (collapsed ? -1.0f : 1.0f);
    const float radius = size[axis] * 0.32f;
    tip[axis] += sign * radius; a[axis] -= sign * radius; b[axis] -= sign * radius;
    a[cross] -= std::min(size[cross] * 0.3f, size[axis] * 0.7f);
    b[cross] += std::min(size[cross] * 0.3f, size[axis] * 0.7f);
    draw->AddTriangleFilled(tip, a, b, hovered || active ? ImGui::GetColorU32(ImGuiCol_Text)
        : ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.0f)));
    draw->PopClipRect();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !active) {
        const char* edge = axis == ImGuiAxis_X ? (side == 0 ? "left" : "right") : (side == 0 ? "top" : "bottom");
        ImGui::SetTooltip("%s docks toward %s\nDrag to resize", collapsed ? "Restore" : "Collapse", edge);
    }
    ImGui::End();
}

