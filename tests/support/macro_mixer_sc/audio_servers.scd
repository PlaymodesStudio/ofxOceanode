// Non-realtime regression for Audio Servers: Alternate. Each server renders
// its own copy of the MacroMixer bridge as a separate scsynth job (as the
// controller's NRT render does per server): server 0 hears only side A, server
// 1 only side B, the other input reading a bus nothing writes (the server's
// silent bus). The two files must sum to the single-server crossfade.
(
var directory = "SC_MACRO_MIXER_TEST_DIR".getenv;
var helper = thisProcess.nowExecutingPath.dirname.dirname.dirname.dirname.dirname +/+ "ofxOceanodeSuperCollider/data/Supercollider/Synthdefs/Defaults/macromixer/macroMixer.scsyndef";
var a = SynthDef(\macroMixerTestA, { Out.ar(16, DC.ar([0.2, 0.4])) });
var b = SynthDef(\macroMixerTestB, { Out.ar(144, DC.ar([0.8, 0.6])) });
var options = ServerOptions.new.numOutputBusChannels_(2).numInputBusChannels_(0).numAudioBusChannels_(1024).numWireBufs_(8192).memSize_(65536);
var gains = [[0.1, 0.75, 0.25], [0.2, 0.5, 0.5], [0.3, 0.5.sqrt, 0.5.sqrt], [0.4, 0, 1]];
// side: \a reads bus 16 on inA, \b bus 144 on inB; the other input reads 512 (silent).
var score = { |side|
    var source = if(side == \a) { \macroMixerTestA } { \macroMixerTestB };
    var inA = if(side == \a) { 16 } { 512 }, inB = if(side == \a) { 512 } { 144 };
    Score([
        [0.0, [\g_new, 1, 0, 0]],
        [0.0, [\d_load, helper]],
        [0.0, [\d_recv, a.asBytes]], [0.0, [\d_recv, b.asBytes]],
        [0.01, [\s_new, source, 1000, 1, 1]],
        [0.01, [\s_new, \oceanodeMacroMixer2, 1002, 1, 1, \inA, inA, \inB, inB, \out, 0, \gainA, 1, \gainB, 0, \smoothing, 0]]
    ] ++ gains.collect { |g| [g[0], [\n_set, 1002, \gainA, g[1], \gainB, g[2]]] } ++ [
        [0.5, [\n_free, 1000, 1002]],
        [0.6, [\c_set, 0, 0]]
    ])
};
File.mkdir(directory);
score.(\a).recordNRT(
    oscFilePath: directory +/+ "server0.osc",
    outputFilePath: directory +/+ "server0.wav",
    sampleRate: 48000, headerFormat: "WAV", sampleFormat: "float",
    options: options, duration: 0.6, action: {
        score.(\b).recordNRT(
            oscFilePath: directory +/+ "server1.osc",
            outputFilePath: directory +/+ "server1.wav",
            sampleRate: 48000, headerFormat: "WAV", sampleFormat: "float",
            options: options, duration: 0.6, action: { 0.exit }
        );
    }
);
)
