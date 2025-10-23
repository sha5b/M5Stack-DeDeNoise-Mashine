#pragma once

#include <cstdint>

enum class NoiseType : uint8_t {
  NOISE_WHITE       = 0,
  NOISE_PINK        = 1,
  NOISE_BROWN       = 2,
  NOISE_BLUE        = 3,
  NOISE_VIOLET      = 4,
  TONE_SINE         = 5,
  TONE_SQUARE       = 6,
  TONE_TRIANGLE     = 7,
  TONE_SAW          = 8,
  TONE_CHIRP        = 9,
  TONE_SHEPARD      = 10,
  TONE_FM_BELL      = 11,
  TONE_AM_TREMOLO   = 12,
  TONE_SHEPARD_DOWN = 13,
  TONE_KARPLUS      = 14,
  TONE_MODAL_DRUM   = 15,
  TONE_GRANULAR     = 16,
  TONE_SUPERSAW     = 17,
  TONE_PWM          = 18,
  FX_BITCRUSH       = 19,
  TONE_PHASE_DIST   = 20,
  TONE_WAVEFOLD     = 21,
  NOISE_BANDPASS    = 22,
  RHYTHM_EUCLIDEAN  = 23,
  RHYTHM_EUCLIDEAN_7_16,
  RHYTHM_POLY_3_4,
  TONE_RING_MOD,
  TONE_CHORUS,
  FX_SAMPLE_HOLD,
  FX_FORMANT,
  TONE_SYNC,
  TONE_SUPER_SQUARE,
  TONE_ISOCHRONIC,
  TONE_ACOUSTIC_BEAT,
  TONE_MISSING_FUND,
  TONE_COMBINATION_TONES,
  TONE_INFRASOUND,
  TONE_SOMATIC_BASS,
  TONE_EAR_RESONANCE,
  TONE_NEAR_NYQUIST,
  TONE_FEEDBACK_HOWL,
  TONE_FM_METAL,
  FX_STUTTER,
  FX_PHASER,
  FX_DOPPLER,
  FX_GATED_REVERB,
  FX_ALIASING_BUZZ
};

// Get noise type for current track
NoiseType getCurrentNoiseType(int trackIndex);

// Get display name for noise type
const char* getNoiseTypeName(NoiseType t);

// Get gain normalization factor for noise type
float getGainForType(NoiseType t);
