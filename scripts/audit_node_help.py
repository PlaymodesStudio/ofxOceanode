#!/usr/bin/env python3
"""Check source-named node coverage in the local core and sibling addons.

Ignores hidden checkouts and third-party libraries. Runtime-generated names
(e.g. imported SynthDefs) obtain descriptions from their registration specs or
from a catalog entry ending in a space, which describes that type family.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASES = r'(?:ofxOceanodeNodeModel(?:ExternalWindow)?|timeGenerator|primitiveRenderer|RadioStationVLCBase|simpleEffect|abstractRouter|abstractPortal|portalSelectorBase|scNode)'
# Keep quoted strings intact while removing comments from C++ source.
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')

def main():
    catalog = json.loads((ROOT / 'docs/node-help-catalog.json').read_text())
    assert all(v['description'].strip() for v in catalog.values())
    roots = [ROOT / 'src/Nodes']
    roots += [path / 'src' for path in ROOT.parent.iterdir()
              if path.is_dir() and path.name != ROOT.name and
              (path.name.startswith('ofxOceanode') or path.name in ('ofxSantiNodes', 'ofxEloiNodes', 'ofxPlaymodes'))]
    missing, discovered = [], set()
    for root in roots:
        for path in root.rglob('*'):
            if path.suffix not in ('.h', '.cpp') or any(part.startswith('.') or part in ('libs', 'external')
                for part in path.relative_to(root).parts):
                continue
            source = TOKENS.sub(lambda match: '' if match[0].startswith(('//', '/*')) else match[0],
                                path.read_text(errors='ignore'))
            for match in re.finditer(r'(' + BASES + r')\s*\(\s*"([^"\n]+)"', source):
                name = ('SC ' if match[1] == 'scNode' else '') + match[2]
                discovered.add(name)
                if name not in catalog:
                    missing.append((name, str(path.relative_to(ROOT.parent))))
    for name, path in missing:
        print(f'MISSING {name}: {path}')
    print(f'{len(discovered)} source names/families checked; {len(catalog)} catalog entries; {len(missing)} missing')
    raise SystemExit(bool(missing))

if __name__ == '__main__':
    main()
