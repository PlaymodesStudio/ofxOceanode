#!/usr/bin/env python3
"""Render the real MacroMixer SynthDefs offline and check float WAV samples (no audio device)."""
import math,os,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SCLANG=os.environ.get('OCEANODE_TEST_SCLANG','/Applications/SuperCollider.app/Contents/MacOS/sclang')
def main():
    with tempfile.TemporaryDirectory(prefix='macro-mixer-sc-audio-') as directory:
        run=subprocess.run([SCLANG,'-D',str(ROOT/'tests/support/macro_mixer_sc/audio.scd')],env={**os.environ,'SC_MACRO_MIXER_TEST_DIR':directory},capture_output=True,text=True,timeout=60)
        assert run.returncode==0,run.stdout+run.stderr
        data=(Path(directory)/'mix.wav').read_bytes(); assert data[:4]==b'RIFF' and data[8:12]==b'WAVE'
        chunks={};offset=12
        while offset+8<=len(data):
            name=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0];chunks[name]=data[offset+8:offset+8+size];offset+=8+size+(size%2)
        form,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',chunks[b'fmt ']);assert form==3 and channels==2 and bits==32 and rate==48000
        samples=struct.unpack('<'+'f'*(len(chunks[b'data'])//4),chunks[b'data'])
        for time,expected in [(.05,(.2,.4)),(.15,(.35,.45)),(.25,(.5,.5)),(.35,(.8,.6)),(.45,(.1,.2)),(.55,(0,0)),(.65,(.4,.3)),(.75,(math.sqrt(.5),math.sqrt(.5))),(.85,(.8,.6)),(.95,(0,0))]:
            for frame in range(int(time*rate),int(time*rate)+128):
                for c in range(2):
                    actual=samples[frame*2+c];assert math.isfinite(actual) and abs(actual-expected[c])<.001,(time,c,actual,expected[c],run.stdout,run.stderr)
        # A smoothed gain change has intermediate samples and reaches its target.
        assert .7071 < samples[int(.802*rate)*2] < .8
        assert abs(samples[int(.85*rate)*2]-.8)<.001
        assert 'FAILURE' not in run.stdout and 'ERROR:' not in run.stdout,run.stdout+run.stderr
    print('SC audio render passed: real SynthDef loading, linear/equal-power/FadeOut gains, gain smoothing, endpoint silence and source cleanup; no audio device opened.')
if __name__=='__main__':main()
