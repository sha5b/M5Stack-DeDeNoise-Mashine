#pragma once

#include <cstdint>
#include "types.h"

/* Utility functions */
inline uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<uint8_t>(v);
}

// Basic noise generators
uint8_t nextWhiteSample();
uint8_t nextPinkSample();
uint8_t nextBrownSample();
uint8_t nextBlueSample();
uint8_t nextVioletSample();

// Tone generators
uint8_t nextToneSample(NoiseType t);
uint8_t nextShepardUpU8();
uint8_t nextShepardDownU8();

// Advanced synthesis
uint8_t nextKarplusU8();
uint8_t nextModalDrumU8();
uint8_t nextGranularU8();
uint8_t nextSuperSawU8();
uint8_t nextPwmU8();
uint8_t nextBitcrushU8();
uint8_t nextPhaseDistU8();
uint8_t nextWavefoldU8();
uint8_t nextBandpassNoiseU8();

// Rhythm generators
uint8_t nextEuclidU8();
uint8_t nextEuclid716U8();
uint8_t nextPoly34U8();

// Effects and modulation
uint8_t nextRingModU8();
uint8_t nextChorusU8();
uint8_t nextSampleHoldU8();
uint8_t nextFormantU8();
uint8_t nextSyncU8();
uint8_t nextSuperSquareU8();

// Main sample generator
uint8_t nextAudioSample(NoiseType t);

// Audio task function
void audioTask(void* param);

// Audio state management
void initAudioState();
void setAudioRunning(bool running);
void setAudioNoiseType(NoiseType type);

// Master volume control (0.0 .. 1.0, clamped)
void setMasterGain(float g);
float getMasterGain();
