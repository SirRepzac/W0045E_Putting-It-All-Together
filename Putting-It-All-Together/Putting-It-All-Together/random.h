#pragma once
#include <cstdint>

/// Produces an xorshift128 pseudo random number.
unsigned FastRandom();

/// Produces an xorshift128 psuedo based floating point random number in range 0..1
/// Note that this is not a truly random random number generator
float RandomFloat();

/// Produces an xorshift128 psuedo based floating point random number in range -1..1
/// Note that this is not a truly random random number generator
float RandomFloatNTP();

struct RNG
{
    uint32_t state;

    explicit RNG(uint32_t seed) : state(seed ? seed : 1) {}

    uint32_t NextU32()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float NextFloat01()
    {
        return (NextU32() & 0x00FFFFFF) / float(0x01000000);
    }
};