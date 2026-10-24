#!/usr/bin/env python3
"""Run scripts/migrate_gui_files.py on a temporary data folder.

Covers a preset with all four old files, one with only ImGuiLayout.ini, a
workspaces.json v1 collection, an already migrated preset, a malformed view.json
and a config folder with defaultLayout, fps, view.json, defaultTheme and two
global .ini layouts (the default one takes view.json's controller visibility). Checks
that the dry run changes nothing, --apply writes the exact new JSON and a backup,
and a second --apply is a no-op.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/migrate_gui_files.py"

INI = "[Window][Nodes]\nPos=0,0\nDockId=0x00000002,0\n\n[Docking][Data]\nDockSpace ID=0x8B93E3BD\n"
VIEW = {"controllers": {"Nodes": True, "Timeline": False},
        "windows": {"nodes": {'["Synth",1]': {"parameters": {"Show": True}, "inspector": None}}},
        "scopes": False}
SNAPSHOT = {"layout": INI, "controllers": {"Nodes": True}, "windows": {"nodes": {}}, "scopes": True}


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value if isinstance(value, str) else json.dumps(value), encoding="utf-8")


def hashes(folder):
    return {str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(folder.rglob("*")) if p.is_file()}


def run(data, *flags):
    return subprocess.run([sys.executable, str(SCRIPT), "--data", str(data), *flags],
                          capture_output=True, text=True)


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-gui-migration-") as work:
        work = Path(work)
        data = work / "data"
        presets = data / "Presets/Bank"
        full, ini_only, legacy, done, broken = (presets / name for name in
                                                ("1--Full", "2--IniOnly", "3--Legacy", "4--Done", "5--Broken"))
        write(full / "ImGuiLayout.ini", INI)
        write(full / "view.json", dict(VIEW, version=1))
        write(full / "preset_layouts.json", {"version": 2, "active": "Sound", "default": "Gone",
                                            "layouts": {"Sound": SNAPSHOT,
                                                        "Laser": {k: v for k, v in SNAPSHOT.items() if k != "scopes"}}})
        write(full / "workspaces.json", {"version": 1, "workspaces": {}})
        write(full / "preset.json", "{}")
        write(ini_only / "ImGuiLayout.ini", INI)
        write(legacy / "workspaces.json", {"version": 1, "active": "Old", "workspaces": {"Old": SNAPSHOT}})
        write(done / "layout.json", {"version": 1, "layouts": {}, "active": "", "default": ""})
        write(done / "ImGuiLayout.ini", INI)
        write(broken / "ImGuiLayout.ini", INI)
        write(broken / "view.json", {"version": 3, "controllers": {}})
        config = data / "config"
        write(config / "config.json", {"fps": 59.9, "defaultLayout": "init_4.ini", "vsync": True, "legacy": 1})
        write(config / "guiLayouts/init_4.ini", INI)
        write(config / "guiLayouts/Studio.ini", INI + "\n")
        write(config / "view.json", {"Nodes": True, "MIDI": False})
        write(config / "themes/defaultTheme", {"theme": "dark"})
        write(config / "themes/dark.json", {"colors": {}})
        write(config / "font/readme.txt", "fonts")

        # The dry run reports every unit and touches nothing.
        before = hashes(work)
        dry = run(data)
        assert dry.returncode == 1, dry.stdout + dry.stderr  # the malformed view.json is an error
        assert hashes(work) == before
        assert "error     Presets/Bank/5--Broken" in dry.stdout, dry.stdout
        assert "skipped   Presets/Bank/4--Done" in dry.stdout and "WARNING old files remain: ImGuiLayout.ini" in dry.stdout
        assert "global view config dropped; per-preset visibility is in layout.json" in dry.stdout
        assert "init_4: startup controller visibility from view.json" in dry.stdout, dry.stdout
        assert "Summary: 5 migrated, 1 skipped, 1 error(s)" in dry.stdout, dry.stdout

        applied = run(data, "--apply")
        assert applied.returncode == 1, applied.stdout + applied.stderr
        assert load(full / "layout.json") == {
            "version": 1,
            "current": {"ini": INI, "view": VIEW},
            "layouts": {"Sound": {"ini": INI, "view": {"controllers": {"Nodes": True}, "windows": {"nodes": {}}, "scopes": True}},
                        "Laser": {"ini": INI, "view": {"controllers": {"Nodes": True}, "windows": {"nodes": {}}}}},
            "active": "Sound", "default": ""}
        assert sorted(p.name for p in full.iterdir()) == ["layout.json", "preset.json"]
        assert load(ini_only / "layout.json") == {"version": 1, "current": {"ini": INI, "view": None},
                                                  "layouts": {}, "active": "", "default": ""}
        assert load(legacy / "layout.json") == {
            "version": 1, "layouts": {"Old": {"ini": INI, "view": {"controllers": {"Nodes": True}, "windows": {"nodes": {}}, "scopes": True}}},
            "active": "Old", "default": ""}
        assert not (legacy / "workspaces.json").exists()
        assert (done / "ImGuiLayout.ini").exists() and load(done / "layout.json")["layouts"] == {}
        # A malformed view.json aborts that preset only.
        assert sorted(p.name for p in broken.iterdir()) == ["ImGuiLayout.ini", "view.json"]
        assert load(config / "config.json") == {"version": 2, "desiredFPS": 59, "vsync": True}
        assert load(config / "guiLayouts.json") == {"version": 1, "default": "init_4"}
        assert load(config / "themes.json") == {"version": 1, "default": "dark"}
        assert not (config / "view.json").exists() and not (config / "themes/defaultTheme").exists()
        assert load(config / "guiLayouts/init_4.json") == {
            "version": 1, "ini": INI, "view": {"controllers": {"Nodes": True, "MIDI": False}, "windows": {}}}
        assert load(config / "guiLayouts/Studio.json") == {"version": 1, "ini": INI + "\n", "view": None}
        assert not list((config / "guiLayouts").glob("*.ini")) and (config / "themes/dark.json").exists()
        assert (config / "font/readme.txt").exists()
        # Every changed or deleted file is backed up beside data/, keeping relative paths.
        backups = list(work.glob("gui_migration_backup_*"))
        assert len(backups) == 1
        backup = backups[0]
        for relative in ("Presets/Bank/1--Full/ImGuiLayout.ini", "Presets/Bank/1--Full/view.json",
                         "Presets/Bank/1--Full/preset_layouts.json", "Presets/Bank/1--Full/workspaces.json",
                         "Presets/Bank/3--Legacy/workspaces.json", "config/config.json",
                         "config/view.json", "config/themes/defaultTheme",
                         "config/guiLayouts/init_4.ini", "config/guiLayouts/Studio.ini"):
            assert (backup / relative).is_file(), relative
        assert before["data/config/config.json"] == hashlib.sha256((backup / "config/config.json").read_bytes()).hexdigest()
        assert not (backup / "Presets/Bank/4--Done").exists() and not (backup / "Presets/Bank/5--Broken").exists()

        # Fix the broken preset: the next run migrates only it, then nothing is left to do.
        write(broken / "view.json", dict(VIEW, version=1))
        again = run(data, "--apply")
        assert again.returncode == 0 and "Summary: 1 migrated, 1 skipped, 0 error(s)" in again.stdout, again.stdout
        (done / "ImGuiLayout.ini").unlink()
        after = hashes(work)
        final = run(data, "--apply")
        assert final.returncode == 0 and "Nothing to migrate." in final.stdout, final.stdout
        assert hashes(work) == after and len(list(work.glob("gui_migration_backup_*"))) == 2
    print("PASS: dry run untouched, exact layout.json/config/theme output, backups, malformed preset isolated, idempotent re-run.")


if __name__ == "__main__":
    main()
