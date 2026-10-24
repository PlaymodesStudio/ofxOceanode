#!/usr/bin/env python3
"""Convert pre-refactor Oceanode GUI files to the formats of docs/layouts.md, once.

    python3 scripts/migrate_gui_files.py --data <dataDir> [--apply]

Without --apply this is a dry run: it prints what it would do and touches
nothing. With --apply it copies every file it changes or deletes into
<dataDir>/../gui_migration_backup_<YYYYmmdd-HHMMSS>/, writes the new files,
deletes the old ones and prints the report. A second run finds nothing to do.

Preset folders (any directory under <data>/Presets/ holding ImGuiLayout.ini,
view.json, preset_layouts.json or workspaces.json) become <preset>/layout.json.
In <data>/config/: config.json becomes version 2 (defaultLayout moves to
guiLayouts.json), themes/defaultTheme becomes themes.json and view.json is
removed. Each global layout guiLayouts/<name>.ini becomes guiLayouts/<name>.json
({version, ini, view}); the default one takes its controller visibility from the
old view.json when that file is still present. Python 3 standard library only. Exit code 0 on success, 1 on any error;
a unit (one preset folder, or the config folder) with an error is left untouched.
"""
import argparse
import datetime
import json
import shutil
import sys
from pathlib import Path

OLD_PRESET_FILES = ("ImGuiLayout.ini", "view.json", "preset_layouts.json", "workspaces.json")
CONFIG_KEYS = ("desiredFPS", "vsync", "configurationFlags", "showMode", "colorTexturePrecision",
               "nodeTextWidth", "nodeWidgetWidth", "gridDivisions", "snapToGrid", "autoInspectorShowHide",
               "selectionCentersCanvas", "connectionWidth", "spaceTogglesPlay")


class MigrationError(Exception):
    pass


def valid_name(name):
    """ofxOceanodePresetLayouts::validName."""
    if any(ord(c) < 32 or ord(c) == 127 for c in name):
        return False
    return (0 < len(name.encode("utf-8")) < 256 and name.strip(" \t\r\n") == name
            and "##" not in name)


def read_json(path):
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError) as error:
        raise MigrationError(f"cannot read {path}: {error}")


def read_text(path):
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise MigrationError(f"cannot read {path}: {error}")


def dump(value):
    return json.dumps(value, indent=4, ensure_ascii=False)


class Unit:
    """One preset folder or the config folder: files to write and to delete."""

    def __init__(self, name):
        self.name = name
        self.writes = {}    # Path -> text
        self.deletes = []   # Path
        self.notes = []
        self.status = "migrated"

    def describe(self):
        details = [f"write {p.name}" for p in self.writes] + [f"delete {p.name}" for p in self.deletes]
        return "; ".join(details + self.notes)


def view_from(view_path):
    view = read_json(view_path)
    if not isinstance(view, dict) or view.get("version") != 1 or not isinstance(view.get("controllers"), dict):
        raise MigrationError(f"{view_path.name}: expected version 1 with a controllers object")
    view = dict(view)
    del view["version"]
    return view


def collection_from(folder):
    current = folder / "preset_layouts.json"
    legacy = folder / "workspaces.json"
    if current.exists():
        data, version, key = read_json(current), 2, "layouts"
        source = current
    elif legacy.exists():
        data, version, key = read_json(legacy), 1, "workspaces"
        source = legacy
    else:
        return {}, "", ""
    if not isinstance(data, dict) or data.get("version") != version or not isinstance(data.get(key), dict):
        raise MigrationError(f"{source.name}: expected version {version} with a {key} object")
    layouts = {}
    for name, snapshot in data[key].items():
        if (not valid_name(name) or not isinstance(snapshot, dict)
                or not isinstance(snapshot.get("layout"), str) or not snapshot["layout"]
                or not isinstance(snapshot.get("controllers"), dict) or not isinstance(snapshot.get("windows"), dict)):
            raise MigrationError(f"{source.name}: invalid snapshot {name!r}")
        view = {"controllers": snapshot["controllers"], "windows": snapshot["windows"]}
        if "scopes" in snapshot:
            view["scopes"] = snapshot["scopes"]
        layouts[name] = {"ini": snapshot["layout"], "view": view}
    active = data.get("active", "")
    default = data.get("default", "")
    active = active if isinstance(active, str) and active in layouts else ""
    default = default if isinstance(default, str) and default in layouts else ""
    return layouts, active, default


def plan_preset(data, folder):
    unit = Unit(str(folder.relative_to(data)))
    old = [folder / name for name in OLD_PRESET_FILES if (folder / name).exists()]
    if (folder / "layout.json").exists():
        unit.status = "skipped"
        unit.notes.append("skip (already migrated); WARNING old files remain: " + ", ".join(p.name for p in old))
        return unit
    ini_path, view_path = folder / "ImGuiLayout.ini", folder / "view.json"
    layout = {"version": 1}
    if ini_path.exists() or view_path.exists():
        layout["current"] = {
            "ini": read_text(ini_path) if ini_path.exists() else "",
            "view": view_from(view_path) if view_path.exists() else None,
        }
    layouts, active, default = collection_from(folder)
    layout.update({"layouts": layouts, "active": active, "default": default})
    unit.writes[folder / "layout.json"] = dump(layout)
    unit.deletes = old
    unit.notes.append(f"{len(layouts)} preset layout(s)")
    return unit


def plan_config(data):
    config_dir = data / "config"
    unit = Unit("config")
    config_path = config_dir / "config.json"
    if config_path.exists():
        config = read_json(config_path)
        if not isinstance(config, dict):
            raise MigrationError("config.json: expected an object")
        if config.get("version") != 2:
            default_layout = config.get("defaultLayout", "")
            if default_layout:
                name = default_layout if isinstance(default_layout, str) else ""
                name = name[:-4] if name.endswith(".ini") else name
                if not name or not any((config_dir / "guiLayouts" / f"{name}{ext}").exists() for ext in (".ini", ".json")):
                    unit.notes.append(f"default layout {default_layout!r} not found, default cleared")
                    name = ""
                guiLayouts = config_dir / "guiLayouts.json"
                if guiLayouts.exists():
                    raise MigrationError("guiLayouts.json already exists next to a config.json with defaultLayout")
                unit.writes[guiLayouts] = dump({"version": 1, "default": name})
            if "desiredFPS" not in config and "fps" in config:
                config["desiredFPS"] = int(config["fps"])
            dropped = [key for key in config if key not in CONFIG_KEYS and key not in ("defaultLayout", "fps", "version")]
            if dropped:
                unit.notes.append("dropped keys: " + ", ".join(dropped))
            migrated = {"version": 2}
            migrated.update({key: config[key] for key in CONFIG_KEYS if key in config})
            unit.writes[config_path] = dump(migrated)
    pointer = config_dir / "themes" / "defaultTheme"
    if pointer.exists():
        theme = read_json(pointer)
        name = theme.get("theme", "") if isinstance(theme, dict) else ""
        if not isinstance(name, str) or not name or not (config_dir / "themes" / f"{name}.json").exists():
            unit.notes.append(f"default theme {name!r} not found, default cleared")
            name = ""
        themes = config_dir / "themes.json"
        if themes.exists():
            raise MigrationError("themes.json already exists next to themes/defaultTheme")
        unit.writes[themes] = dump({"version": 1, "default": name})
        unit.deletes.append(pointer)
    view = config_dir / "view.json"
    if view.exists():
        unit.deletes.append(view)
        unit.notes.append("global view config dropped; per-preset visibility is in layout.json")
    if not unit.writes and not unit.deletes:
        return None
    return unit


def valid_global_name(name):
    return valid_name(name) and not any(c in name for c in "/\\:")


def default_global_layout(config_dir):
    """The default global layout name, from either config format."""
    config_path, gui_layouts = config_dir / "config.json", config_dir / "guiLayouts.json"
    if config_path.exists():
        config = read_json(config_path)
        if isinstance(config, dict) and config.get("version") != 2 and isinstance(config.get("defaultLayout"), str):
            name = config["defaultLayout"]
            return name[:-4] if name.endswith(".ini") else name
    if gui_layouts.exists():
        layouts = read_json(gui_layouts)
        if isinstance(layouts, dict) and isinstance(layouts.get("default"), str):
            return layouts["default"]
    return ""


def plan_global_layouts(data):
    folder = data / "config" / "guiLayouts"
    if not folder.is_dir():
        return None
    old = sorted(p for p in folder.glob("*.ini") if p.is_file())
    if not old:
        return None
    unit = Unit("config/guiLayouts")
    default = default_global_layout(data / "config")
    view_path = data / "config" / "view.json"
    startup_view = None
    if view_path.exists():
        visibility = read_json(view_path)
        if not isinstance(visibility, dict) or not all(isinstance(v, bool) for v in visibility.values()):
            raise MigrationError("view.json: expected an object of controller names to booleans")
        startup_view = {"controllers": visibility, "windows": {}}
    for ini in old:
        name, target = ini.stem, ini.with_suffix(".json")
        if not valid_global_name(name):
            raise MigrationError(f"{ini.name}: invalid layout name")
        if target.exists():
            raise MigrationError(f"{target.name} already exists next to {ini.name}")
        text = read_text(ini)
        if not text:
            raise MigrationError(f"{ini.name}: empty layout")
        view = startup_view if name == default else None
        unit.writes[target] = dump({"version": 1, "ini": text, "view": view})
        unit.deletes.append(ini)
        if view is not None:
            unit.notes.append(f"{name}: startup controller visibility from view.json")
    return unit


def preset_folders(data):
    presets = data / "Presets"
    if not presets.is_dir():
        return []
    folders = {p.parent for name in OLD_PRESET_FILES for p in presets.rglob(name) if p.is_file()}
    return sorted(folders)


def apply(data, units, backup):
    for unit in units:
        for path in list(unit.writes) + unit.deletes:
            if path.exists():
                target = backup / path.relative_to(data)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    for unit in units:
        for path, text in unit.writes.items():
            path.write_text(text, encoding="utf-8")
        for path in unit.deletes:
            if path not in unit.writes:
                path.unlink()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--data", required=True, help="the openFrameworks data folder (bin/data)")
    parser.add_argument("--apply", action="store_true", help="change files (default: dry run)")
    args = parser.parse_args(argv)
    data = Path(args.data).resolve()
    if not data.is_dir():
        print(f"error: {data} is not a folder")
        return 1

    lines, changes, errors = [], [], 0
    counts = {"migrated": 0, "skipped": 0, "error": 0}
    planners = [(str(folder.relative_to(data)), lambda f=folder: plan_preset(data, f)) for folder in preset_folders(data)]
    planners.append(("config/guiLayouts", lambda: plan_global_layouts(data)))
    planners.append(("config", lambda: plan_config(data)))
    for name, planner in planners:
        try:
            unit = planner()
        except MigrationError as error:
            counts["error"] += 1
            errors += 1
            lines.append(f"error     {name}: {error}")
            continue
        if unit is None:
            continue
        counts[unit.status] += 1
        lines.append(f"{unit.status:<9} {unit.name}: {unit.describe()}")
        if unit.status == "migrated":
            changes.append(unit)

    mode = "APPLY" if args.apply else "DRY RUN (nothing changed; add --apply to migrate)"
    print(f"Oceanode GUI file migration: {data}\nMode: {mode}")
    if args.apply and changes:
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        backup = data.parent / f"gui_migration_backup_{stamp}"
        suffix = 2
        while backup.exists():
            backup = data.parent / f"gui_migration_backup_{stamp}_{suffix}"
            suffix += 1
        apply(data, changes, backup)
        print(f"Backup: {backup}")
    for line in lines:
        print(line)
    if not lines:
        print("Nothing to migrate.")
    print(f"Summary: {counts['migrated']} migrated, {counts['skipped']} skipped, {counts['error']} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
