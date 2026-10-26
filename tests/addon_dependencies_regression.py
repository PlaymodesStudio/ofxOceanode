#!/usr/bin/env python3
"""Check transport consumers with the optional timeline linked and omitted.

Uses real local OF/addon headers. Builds objects for the SC boundary files and
rejects timeline-manager references in the core-only objects; other affected
units are syntax checked. Also runs the real backend scheduling adapter with
no timeline headers or library, and verifies explicit capability detection.
"""
from pathlib import Path
import argparse
import concurrent.futures
import os
import subprocess
import tempfile
from of_environment import ROOT, of_root, addon_source, addon_include_dirs

SC_UNITS = [
    'scNode.cpp', 'scSynthdef.cpp', 'scTimelineMenu.cpp',
    'ofxOceanodeSuperColliderController.cpp',
    'ofxOceanodeSuperColliderNodesSequencers.cpp',
    'ofxOceanodeSuperColliderNodesUtils.cpp',
    'nodes/scBeatRepeatPro.cpp', 'nodes/scGrainBoxPoly.cpp',
    'nodes/scMetroBox.cpp', 'nodes/scMixerPro.cpp',
    'nodes/scRhythmBox.cpp',
    'nodes/scSampleBox.cpp',
]
OBJECT_UNITS = {'scTimelineMenu.cpp', 'nodes/scMetroBox.cpp',
                'nodes/scSampleBox.cpp', 'ofxOceanodeSuperColliderController.cpp'}
SANTI_UNITS = ['ofxSantiNodes.cpp', 'ofxSantiNodesMath.cpp', 'ofxSantiNodesPitch.cpp',
               'ofxSantiNodesSequencing.cpp', 'ofxSantiNodesUtility.cpp',
               'chordSequence.cpp', 'polyphonicArpeggiatorGUI.cpp']

SCHEDULING = r'''
#include "scSchedulingCompat.h"
#include <cassert>
class ofxOceanodeAbstractParameter {};
int main() {
    ofxOceanodeAbstractParameter parameter;
    int owner = 0, received = 0;
    scScheduling::registerParameterTarget(&parameter, &owner,
        [&](uint64_t when, const std::string& value) {
            assert(when == 12345 && value == "42"); ++received; return true;
        });
    assert(ofxOceanodeScheduling::hasParameterTarget(&parameter));
    ofxOceanodeScheduledParameterEvent event;
    event.dueSteadyTimeUs = 12345; event.value = "42";
    assert(ofxOceanodeScheduling::dispatch(&parameter, event));
    assert(received == 1 && !scScheduling::isBackendSendSuppressed());
    {
        ofxOceanodeScheduling::ScopedBackendSuppression scope;
        assert(scScheduling::isBackendSendSuppressed());
    }
    assert(!scScheduling::isBackendSendSuppressed());
    scScheduling::unregisterOwner(&owner);
    assert(!ofxOceanodeScheduling::hasParameterTarget(&parameter));
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    of = of_root(); addons = ROOT.parent
    sc = addons / 'ofxOceanodeSuperCollider'
    timeline = addons / 'ofxOceanodeTimeline'
    flags = [os.environ.get('CXX', 'clang++'), '-std=c++23', '-w',
             '-DOF_NO_FMOD', '-DGL_SILENCE_DEPRECATION']
    includes = [ROOT / 'src', *sorted({p.parent for p in (ROOT / 'src').rglob('*.h')})]
    includes += sorted({p.parent for p in (of / 'libs/openFrameworks').rglob('*.h')})
    includes += [p for p in (of / 'libs').glob('*/include') if p.is_dir()]
    includes += [of / 'libs/freetype/include/freetype', of / 'libs/cairo/include/cairo']
    # Santi's registration files include texture and utility dependencies.
    # Deliberately omit every timeline include path from the core-only case.
    dependencies = ['ofxOceanodeSuperCollider', 'ofxSuperCollider', 'ofxOsc',
                    'ofxImGui', 'ofxMidi', 'ofxOceanodeMidi', 'ofxOceanodeOsc',
                    'ofxSantiNodes', 'ofxOceanodeCollection', 'ofxOceanodeTextures',
                    'ofxHapPlayer', 'ofxNDI', 'ofxSyphon', 'ofxFatLines']
    for name in dependencies:
        addon = addons / name
        includes += sorted({p.parent for p in (addon / 'src').rglob('*.h')})
        if name == 'ofxHapPlayer':
            # FFmpeg's libavutil/time.h must not shadow the system time.h.
            includes += list((addon / 'libs').glob('*/include'))
        else:
            includes += sorted({p.parent for p in (addon / 'libs').rglob('*.h')})
    base = flags + ['-I' + str(p) for p in dict.fromkeys(includes)]
    with tempfile.TemporaryDirectory(prefix='oceanode-addon-deps-') as directory:
        temp = Path(directory)
        probe = temp / 'capability.cpp'
        probe.write_text('#include "ofxOceanodeSuperColliderConfig.h"\n'
                         'static_assert(OFXOCEANODESC_HAS_TIMELINE == EXPECT_TIMELINE);\n')
        # A discoverable header without the addon build flag must stay disabled.
        exposed = ['-I' + str(timeline / 'src'), '-I' + str(timeline / 'src/Timeline')]
        for define, expected in [([], 0), (['-DOFX_OCEANODE_HAS_TIMELINE=0'], 0),
                                 (['-DOFX_OCEANODE_HAS_TIMELINE=1'], 1)]:
            subprocess.run(base + exposed + define + [f'-DEXPECT_TIMELINE={expected}',
                           '-fsyntax-only', str(probe)], check=True)
        source = temp / 'scheduling.cpp'; source.write_text(SCHEDULING)
        for enabled in (0, 1):
            binary = temp / f'scheduling-{enabled}'
            subprocess.run(flags + [f'-DOFX_OCEANODE_HAS_TIMELINE={enabled}',
                           '-fsanitize=address,undefined', *['-I' + str(p) for p in addon_include_dirs(sc)],
                           '-I' + str(ROOT / 'src/Managers'), str(source),
                           str(ROOT / 'src/Managers/ofxOceanodeScheduling.cpp'),
                           '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
        print('PASS explicit addon detection and core scheduling adapter (ASan/UBSan)', flush=True)
        jobs = []
        for enabled in (0, 1):
            mode = ['-DOFX_OCEANODE_HAS_TIMELINE=1', *exposed] if enabled else []
            for unit in SC_UNITS:
                jobs.append((f'SC timeline={enabled} {unit}', addon_source(sc, unit),
                             mode, not enabled and unit in OBJECT_UNITS))
        for unit in SANTI_UNITS:
            jobs.append((f'Santi core-only {unit}', addon_source(addons / 'ofxSantiNodes', unit), [], False))
        jobs.append(('Collection core-only', addon_source(addons / 'ofxOceanodeCollection', 'ofxOceanodeCollection.cpp'), [], False))
        def check(item):
            index, (label, path, mode, object_check) = item
            obj = temp / f'{index}.o'
            compile_flags = ['-c', '-O0', '-o', str(obj)] if object_check else ['-fsyntax-only']
            result = subprocess.run(base + mode + compile_flags + [str(path)], text=True, capture_output=True)
            if result.returncode:
                print('FAIL ' + label + '\n' + result.stderr, flush=True); return False
            if object_check:
                symbols = subprocess.check_output(['nm', '-u', str(obj)], text=True)
                assert 'ofxOceanodeTimelineManager' not in symbols and 'getTimelineManager' not in symbols, symbols
            print('PASS ' + label, flush=True); return True
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(check, enumerate(jobs)))
        failures = results.count(False)
        print(f'Checked {len(results)} addon units; {failures} failed', flush=True)
        raise SystemExit(bool(failures))

if __name__ == '__main__':
    main()
