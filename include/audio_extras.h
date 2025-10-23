#pragma once

#include <cstdint>
#include "types.h"

// Extra audio modes and utilities integrated as an optional extension.
// These operate within the existing mono DAC pipeline.
// IMPORTANT: True stereo illusions (binaural, Haas, phase inversion, QSound) are not
// enabled on this single-DAC hardware. Where possible, mono-safe approximations are provided.

// Initialization for extra generators (call once during setup)
void initAudioExtras();

// Main sample generator for extra modes. Returns 0 if type unsupported here.
uint8_t nextAudioSampleExtra(NoiseType t);

// Convenience: query if an enum value is handled by extras switch
bool isExtraType(NoiseType t);
