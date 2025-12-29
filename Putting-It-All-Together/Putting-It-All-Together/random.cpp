#include "random.h"
#include <iostream>

//------------------------------------------------------------------------------
/**
	XorShift128 implementation.
*/
unsigned
FastRandom()
{
	// These are predefined to give us the largest
	// possible sequence of random numbers
    static unsigned x = 123456789;
    static unsigned y = 362436069;
    static unsigned z = 521288629;
    static unsigned w = 88675123;

    unsigned t = x ^ (x << 11);
    x = y;
	y = z;
	z = w;
    w = w ^ (w >> 19) ^ (t ^ (t >> 8));
    return w;
}

//------------------------------------------------------------------------------
/**
    Thanks to Nic Werneck (https://xor0110.wordpress.com/2010/09/24/how-to-generate-floating-point-random-numbers-efficiently/)
*/
float
RandomFloat()
{
    static union
    {
        unsigned int i;
        float f;
    } r;
    r.i = FastRandom() & 0x007fffff | 0x3f800000;
    return r.f - 1.0f;
}

//------------------------------------------------------------------------------
/**
*/
float
RandomFloatNTP()
{
    static union
    {
        unsigned int i;
        float f;
    } r;
    r.i = FastRandom() & 0x007fffff | 0x40000000;
    return r.f - 3.0f;
}


