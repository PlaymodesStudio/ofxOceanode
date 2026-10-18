// Non-realtime regression: no audio device or realtime server is opened.
(
var directory = "SC_MACRO_MIXER_TEST_DIR".getenv;
var helper = thisProcess.nowExecutingPath.dirname.dirname.dirname.dirname.dirname +/+ "ofxOceanodeSuperCollider/data/Supercollider/Synthdefs/Defaults/macromixer/macroMixer.scsyndef";
var a = SynthDef(\macroMixerTestA, { Out.ar(16, DC.ar([0.2, 0.4])) });
var b = SynthDef(\macroMixerTestB, { Out.ar(144, DC.ar([0.8, 0.6])) });
var options = ServerOptions.new.numOutputBusChannels_(2).numInputBusChannels_(0).numAudioBusChannels_(1024).numWireBufs_(8192).memSize_(65536);
var score = Score([
    [0.0, [\g_new, 1, 0, 0]],
    [0.0, [\d_load, helper]],
    [0.0, [\d_recv, a.asBytes]], [0.0, [\d_recv, b.asBytes]],
    [0.01, [\s_new, \macroMixerTestA, 1000, 1, 1]],
    [0.01, [\s_new, \macroMixerTestB, 1001, 1, 1]],
    [0.01, [\s_new, \oceanodeMacroMixer2, 1002, 1, 1, \inA, 16, \inB, 144, \out, 0, \gainA, 1, \gainB, 0, \smoothing, 0]],
    [0.1, [\n_set, 1002, \gainA, 0.75, \gainB, 0.25]],
    [0.2, [\n_set, 1002, \gainA, 0.5, \gainB, 0.5]],
    [0.3, [\n_set, 1002, \gainA, 0, \gainB, 1]],
    [0.4, [\n_set, 1002, \gainA, 0.5, \gainB, 0]],
    [0.5, [\n_set, 1002, \gainA, 0, \gainB, 0]],
    [0.6, [\n_set, 1002, \gainA, 0, \gainB, 0.5]],
    [0.7, [\n_set, 1002, \gainA, 0.5.sqrt, \gainB, 0.5.sqrt]],
    [0.8, [\n_set, 1002, \smoothing, 0.02, \gainA, 0, \gainB, 1]],
    [0.9, [\n_free, 1000]],
    [0.9, [\n_set, 1002, \smoothing, 0, \gainA, 1, \gainB, 0]],
    [1.0, [\n_free, 1001, 1002]],
    [1.1, [\c_set, 0, 0]]
]);
File.mkdir(directory);
score.recordNRT(
    oscFilePath: directory +/+ "score.osc",
    outputFilePath: directory +/+ "mix.wav",
    sampleRate: 48000, headerFormat: "WAV", sampleFormat: "float",
    options: options, duration: 1.1, action: { 0.exit }
);
)
