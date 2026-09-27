//
//  ofxOceanodeDeterministicRandom.h
//  ofxOceanode
//
//  Counter-based ("stateless") random numbers for transport-locked nodes.
//  A value is a pure function of (seed, step, stream), so any timeline position
//  yields the same result regardless of how the playhead got there (play, scrub,
//  loop, seek). Use together with the Phasor "Cycle" output in Sync To Transport mode.
//

#ifndef ofxOceanodeDeterministicRandom_h
#define ofxOceanodeDeterministicRandom_h

#include <cstdint>
#include <cmath>
#include <random>
#include <vector>

namespace ofxOceanodeDeterministicRandom {

    // SplitMix64 finalizer: good avalanche, cheap, fully portable.
    inline uint64_t mix(uint64_t z){
        z += 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    inline uint64_t hash(uint64_t seedKey, int64_t step, uint64_t stream = 0){
        uint64_t h = mix(seedKey);
        h = mix(h ^ static_cast<uint64_t>(step));
        h = mix(h ^ stream);
        return h;
    }

    // Uniform float in [0, 1).
    inline float uniform(uint64_t seedKey, int64_t step, uint64_t stream = 0){
        return static_cast<float>(hash(seedKey, step, stream) >> 40) * (1.0f / 16777216.0f);
    }

    // Maps a user seed to a hash key. Seed 0 keeps its "random" meaning by using a
    // per-node salt: different every app launch, but stable while scrubbing.
    inline uint64_t seedKey(int64_t seed, uint64_t sessionSalt){
        return seed == 0 ? sessionSalt : mix(static_cast<uint64_t>(seed) ^ 0xD1B54A32D192ED03ull);
    }

    inline uint64_t makeSessionSalt(){
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    }

    // Integer step from a float input (e.g. Phasor "Cycle"), tolerant to float noise.
    inline int64_t stepFromFloat(float s){
        return static_cast<int64_t>(std::floor(static_cast<double>(s) + 1e-4));
    }

    // Weighted index selection (same semantics as std::discrete_distribution) from u in [0,1).
    inline int discreteIndex(const std::vector<float> &weights, float u){
        double total = 0;
        for(float w : weights) if(w > 0) total += w;
        if(total <= 0) return 0;
        double target = static_cast<double>(u) * total;
        double acc = 0;
        for(size_t i = 0; i < weights.size(); i++){
            if(weights[i] <= 0) continue;
            acc += weights[i];
            if(target < acc) return static_cast<int>(i);
        }
        for(int i = static_cast<int>(weights.size()) - 1; i >= 0; i--) if(weights[i] > 0) return i;
        return 0;
    }
}

#endif /* ofxOceanodeDeterministicRandom_h */
