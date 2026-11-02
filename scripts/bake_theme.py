#!/usr/bin/env python3
"""Bake a saved Oceanode theme JSON into the built-in C++ defaults.

Rewrites the colour assignments in src/Gui/ofxOceanodeTheme.cpp and the
OceanodeColors initialisers (declarations and reset()) in src/ofxOceanodeColors.h
from a theme saved by the Theme Editor (config/themes/<name>.json). Style
variables and the font family are left alone: fonts come from the app's own
config/font folder, which a fresh app does not have.

    python3 scripts/bake_theme.py path/to/theme.json [imgui.h]

imgui.h defaults to the sibling ofxImGui addon and provides the ImGuiCol_ order.
"""
import json, re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
if len(sys.argv) < 2:
    sys.exit(__doc__)
theme_path = sys.argv[1]
imgui_h = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else root.parent / "ofxImGui/libs/imgui/src/imgui.h"
j = json.load(open(theme_path))
def vec(v): return "ImVec4(" + ", ".join(f"{x:.3f}f" for x in v) + ")"
def brace(v): return "{" + ", ".join(f"{x:.3f}f" for x in v) + "}"

# ---- ImGui colours, in enum order, modern names only ----
enum = imgui_h.read_text()
m = re.search(r"enum ImGuiCol_\s*\{(.*?)ImGuiCol_COUNT", enum, re.S)
names = re.findall(r"^\s*ImGuiCol_(\w+),", m.group(1), re.M)
colors = j["colors"]
missing = [n for n in names if n not in colors]
extra = [n for n in colors if n not in names]
print("enum entries:", len(names), "| not in theme:", missing, "| in theme but not enum:", extra)
width = max(len(n) for n in names) + len("colors[ImGuiCol_]")
lines = []
for n in names:
    lhs = f"colors[ImGuiCol_{n}]".ljust(width + 1)
    if n == "PopupBg":
        lines.append(f"    {lhs}= colors[ImGuiCol_WindowBg]; // WindowBg controls both")
    elif n in colors:
        lines.append(f"    {lhs}= {vec(colors[n])};")
theme_cpp = root / "src/Gui/ofxOceanodeTheme.cpp"
src = theme_cpp.read_text()
start = src.index("    colors[ImGuiCol_Text]")
end = src.index("    // Reset all custom Oceanode semantic colours")
src = src[:start] + "\n".join(lines) + "\n\n" + src[end:]
# Drop a previous bake note so re-running stays idempotent.
src = re.sub(r"//  Colour values are baked from the \".*?\" theme so a fresh app with\n"
             r"//  no config/themes folder starts with it\. Regenerate with scripts/bake_theme\.py\.\n//\n", "", src, count=1)
src = src.replace("//  Created by Eduard Frigola Bagué on 13/04/2020.\n//\n",
    "//  Created by Eduard Frigola Bagué on 13/04/2020.\n//\n"
    "//  Colour values are baked from the \"" + pathlib.Path(theme_path).stem + "\" theme so a fresh app with\n"
    "//  no config/themes folder starts with it. Regenerate with scripts/bake_theme.py.\n//\n", 1)
theme_cpp.write_text(src)

# ---- Oceanode semantic colours: declarations and reset() ----
colors_h = root / "src/ofxOceanodeColors.h"
hs = colors_h.read_text()
oc = j["oceanodeColors"]
fields = re.findall(r'\{ "OC_(\w+)",\s*&(\w+)\s*\}', hs)
assert all(a == b for a, b in fields), fields
field_names = [a for a, _ in fields]
print("fields:", len(field_names), "| theme OC keys missing from header:", [k for k in oc if k[3:] not in field_names],
      "| header fields missing from theme:", [f for f in field_names if "OC_" + f not in oc])
for f in field_names:
    key = "OC_" + f
    if key not in oc: continue
    pattern = re.compile(r"(\b" + f + r"\s*=\s*)\{[^}]*\}")
    hs, count = pattern.subn(lambda mm: mm.group(1) + brace(oc[key]), hs)
    assert count == 2, (f, count)
colors_h.write_text(hs)
print("done")
