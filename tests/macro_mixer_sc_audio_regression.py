#!/usr/bin/env python3
"""Render the real MacroMixer SynthDefs offline and check float WAV samples (no audio device)."""
import math,os,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SCLANG=os.environ.get('OCEANODE_TEST_SCLANG','/Applications/SuperCollider.app/Contents/MacOS/sclang')
def render(script,directory):
    run=subprocess.run([SCLANG,'-D',str(ROOT/'tests/support/macro_mixer_sc'/script)],env={**os.environ,'SC_MACRO_MIXER_TEST_DIR':directory},capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'FAILURE' not in run.stdout and 'ERROR:' not in run.stdout,run.stdout+run.stderr
    return run
def samples(path):
    data=Path(path).read_bytes(); assert data[:4]==b'RIFF' and data[8:12]==b'WAVE'
    chunks={};offset=12
    while offset+8<=len(data):
        name=data[offset:offset+4];size=struct.unpack_from('<I',data,offset+4)[0];chunks[name]=data[offset+8:offset+8+size];offset+=8+size+(size%2)
    form,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',chunks[b'fmt ']);assert form==3 and channels==2 and bits==32 and rate==48000
    return struct.unpack('<'+'f'*(len(chunks[b'data'])//4),chunks[b'data']),rate
def expect(values,rate,checks,context):
    for time,expected in checks:
        for frame in range(int(time*rate),int(time*rate)+128):
            for c in range(2):
                actual=values[frame*2+c];assert math.isfinite(actual) and abs(actual-expected[c])<.001,(time,c,actual,expected[c],context)
def main():
    with tempfile.TemporaryDirectory(prefix='macro-mixer-sc-audio-') as directory:
        run=render('audio.scd',directory)
        mix,rate=samples(Path(directory)/'mix.wav')
        expect(mix,rate,[(.05,(.2,.4)),(.15,(.35,.45)),(.25,(.5,.5)),(.35,(.8,.6)),(.45,(.1,.2)),(.55,(0,0)),(.65,(.4,.3)),(.75,(math.sqrt(.5),math.sqrt(.5))),(.85,(.8,.6)),(.95,(0,0))],run.stdout+run.stderr)
        # A smoothed gain change has intermediate samples and reaches its target.
        assert .7071 < mix[int(.802*rate)*2] < .8
        assert abs(mix[int(.85*rate)*2]-.8)<.001
    with tempfile.TemporaryDirectory(prefix='macro-mixer-sc-servers-') as directory:
        # Audio Servers: Alternate. Each server's bridge copy hears one side
        # (the other input is a bus nothing writes) and applies that side's
        # gain; the device sums the servers into the single-server crossfade.
        run=render('audio_servers.scd',directory)
        server0,rate=samples(Path(directory)/'server0.wav'); server1,_=samples(Path(directory)/'server1.wav')
        assert len(server0)==len(server1)
        root=math.sqrt(.5)
        expect(server0,rate,[(.05,(.2,.4)),(.15,(.15,.3)),(.25,(.1,.2)),(.35,(.2*root,.4*root)),(.45,(0,0)),(.55,(0,0))],run.stdout+run.stderr)
        expect(server1,rate,[(.05,(0,0)),(.15,(.2,.15)),(.25,(.4,.3)),(.35,(.8*root,.6*root)),(.45,(.8,.6)),(.55,(0,0))],run.stdout+run.stderr)
        summed=[x+y for x,y in zip(server0,server1)]
        expect(summed,rate,[(.05,(.2,.4)),(.15,(.35,.45)),(.25,(.5,.5)),(.35,(root,root)),(.45,(.8,.6)),(.55,(0,0))],run.stdout+run.stderr)
    print('SC audio render passed: real SynthDef loading, linear/equal-power/FadeOut gains, gain smoothing, endpoint silence and source cleanup; '
          'two servers each rendering one side sum to the single-server crossfade; no audio device opened.')
if __name__=='__main__':main()
