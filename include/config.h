#pragma once

#include <cstdint>

// Audio Configuration
static const int TRACK_COUNT = 46;
static const int SAMPLE_RATE_HZ = 11025;
static const int AUDIO_DAC_PIN = 25;
static constexpr float TAU_F = 6.28318530718f;

// Visual Configuration
static const int NOISE_W = 280;
static const int NOISE_H = 160;
static const int NOISE_X = (320 - NOISE_W) / 2;
static const int NOISE_Y = 40;
static const uint32_t FRAME_INTERVAL_MS = 67;

// Shuffle Configuration
static const uint32_t SHUFFLE_INTERVAL_MS = 12000;

// Oscilloscope Configuration
static const uint16_t VIS_RING_SIZE = 1024;
static const uint16_t VIS_RING_MASK = 1023;
