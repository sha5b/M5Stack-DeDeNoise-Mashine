#pragma once

#include <cstdint>
#include "types.h"

// Color utilities
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
inline uint16_t gray565(uint8_t g);

// Noise visualization frames
void drawWhiteNoiseFrame();
void drawPinkNoiseFrame();
void drawBrownNoiseFrame();
void drawBlueNoiseFrame();
void drawVioletNoiseFrame();
void drawBandpassNoiseVisualFrame();

// Rhythm visualization
void drawEuclidVisualFrame();

// Tone visualization
void drawToneVisualFrame(NoiseType t);

// Waveform visualization (oscilloscope)
void drawWaveformFrame(NoiseType t);

// Main drawing dispatcher
void drawNoiseFrame(NoiseType t);

// Visual state management
void setVisualType(NoiseType type);
void pushToVisRingBuffer(uint8_t sample);
void initVisualState();
void randomizeGraphColor();
