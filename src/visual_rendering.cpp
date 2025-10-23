#include "visual_rendering.h"
#include "config.h"
#include "types.h"
#include "audio_synthesis.h"
#include <M5Stack.h>
#include <math.h>

// Visual state
static uint16_t lineBuf[NOISE_W];
volatile NoiseType g_visualType = NoiseType::NOISE_WHITE;
static bool g_useOverrideColor = false;
static uint16_t g_graphColor = TFT_WHITE;

// Oscilloscope ring buffer
volatile uint8_t g_visRing[1024] = {0};
volatile uint16_t g_visWriteIdx = 0;

// Shared with main for play state
extern bool isPlaying;

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline uint16_t gray565(uint8_t g) {
  float rf = 1.0f, gf = 1.0f, bf = 1.0f;
  switch (g_visualType) {
    case NoiseType::NOISE_WHITE:   rf = 1.0f; gf = 1.0f; bf = 1.0f; break;
    case NoiseType::NOISE_PINK:    rf = 1.00f; gf = 0.85f; bf = 0.90f; break;
    case NoiseType::NOISE_BROWN:   rf = 1.00f; gf = 0.90f; bf = 0.70f; break;
    case NoiseType::NOISE_BLUE:    rf = 0.70f; gf = 0.80f; bf = 1.00f; break;
    case NoiseType::NOISE_VIOLET:  rf = 0.95f; gf = 0.70f; bf = 1.00f; break;
    case NoiseType::TONE_SINE:     rf = 0.70f; gf = 1.00f; bf = 0.70f; break;
    case NoiseType::TONE_SQUARE:   rf = 1.00f; gf = 0.70f; bf = 0.70f; break;
    case NoiseType::TONE_TRIANGLE: rf = 0.70f; gf = 0.90f; bf = 1.00f; break;
    case NoiseType::TONE_SAW:      rf = 1.00f; gf = 1.00f; bf = 0.70f; break;
    case NoiseType::TONE_CHIRP:    rf = 0.85f; gf = 0.85f; bf = 1.00f; break;
    case NoiseType::TONE_SHEPARD:  rf = 0.90f; gf = 0.90f; bf = 1.00f; break;
    case NoiseType::TONE_FM_BELL:  rf = 0.80f; gf = 1.00f; bf = 1.00f; break;
    case NoiseType::TONE_AM_TREMOLO: rf = 1.00f; gf = 0.80f; bf = 1.00f; break;
    case NoiseType::TONE_SHEPARD_DOWN: rf = 1.00f; gf = 0.85f; bf = 1.00f; break;
    default: break;
  }
  auto clamp8 = [](float x)->uint8_t { if (x < 0.0f) return 0; if (x > 255.0f) return 255; return (uint8_t)x; };
  uint8_t r = clamp8(g * rf);
  uint8_t gg = clamp8(g * gf);
  uint8_t b = clamp8(g * bf);
  return rgb565(r, gg, b);
}

// Map grayscale brightness to a discrete TFT_eSPI color palette for better visibility on low-quality LCDs.
static const uint16_t kTftPalette[] = {
  TFT_BLACK, TFT_NAVY, TFT_BLUE, TFT_CYAN,
  TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_RED,
  TFT_MAGENTA, TFT_PINK, TFT_PURPLE, TFT_WHITE
};
inline uint16_t tftPaletteMap(uint8_t g) {
  const int n = (int)(sizeof(kTftPalette) / sizeof(kTftPalette[0]));
  // Scale 0..255 to 0..(n-1) with rounding
  int idx = (g * (n - 1) + 127) / 255;
  if (idx < 0) idx = 0;
  if (idx > n - 1) idx = n - 1;
  return kTftPalette[idx];
}

void drawWhiteNoiseFrame() {
  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    for (int x = 0; x < NOISE_W; ++x) {
      uint8_t g = (uint8_t)random(0, 256);
      lineBuf[x] = tftPaletteMap(g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
  }
  M5.Lcd.endWrite();
}

void drawPinkNoiseFrame() {
  static const int OCTAVES = 16;
  static uint32_t rows_counters[NOISE_H];
  static uint32_t rows_values[NOISE_H][OCTAVES];
  static bool initialized = false;
  if (!initialized) {
    for (int y = 0; y < NOISE_H; ++y) {
      rows_counters[y] = 0;
      for (int i = 0; i < OCTAVES; ++i) {
        rows_values[y][i] = random(0, 1 << 16);
      }
    }
    initialized = true;
  }

  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    uint32_t counter = rows_counters[y];
    for (int x = 0; x < NOISE_W; ++x) {
      if ((x & 0x03) == 0) {
        counter++;
        uint32_t ctz = __builtin_ctz(counter);
        if (ctz >= OCTAVES) ctz = OCTAVES - 1;
        for (uint32_t k = 0; k <= ctz; ++k) {
          rows_values[y][k] = random(0, 1 << 16);
        }
      }
      uint32_t sum = 0;
      for (int i = 0; i < OCTAVES; ++i) sum += rows_values[y][i];
      float norm = (float)sum / (float)(OCTAVES * (1 << 16));
      uint8_t g = (uint8_t)(norm * 255.0f);
      lineBuf[x] = tftPaletteMap(g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
    rows_counters[y] = counter;
  }
  M5.Lcd.endWrite();
}

void drawBrownNoiseFrame() {
  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    float v = (float)random(-128, 127);
    for (int x = 0; x < NOISE_W; ++x) {
      v += (float)random(-10, 11);
      if (v < -128) v = -128;
      if (v > 127) v = 127;
      uint8_t g = (uint8_t)(v + 128.0f);
      lineBuf[x] = tftPaletteMap(g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
  }
  M5.Lcd.endWrite();
}

void drawBlueNoiseFrame() {
  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    int prev = random(0, 256);
    for (int x = 0; x < NOISE_W; ++x) {
      int w = random(0, 256);
      int d = w - prev;
      prev = w;
      int g = (d / 2) + 128;
      if (g < 0) g = 0; if (g > 255) g = 255;
      lineBuf[x] = tftPaletteMap((uint8_t)g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
  }
  M5.Lcd.endWrite();
}

void drawVioletNoiseFrame() {
  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    int prev2 = random(0, 256);
    int prev1 = prev2;
    for (int x = 0; x < NOISE_W; ++x) {
      int w = random(0, 256);
      int d1 = w - prev1;
      int d2 = d1 - (prev1 - prev2);
      prev2 = prev1;
      prev1 = w;
      int g = (d2 / 3) + 128;
      if (g < 0) g = 0; if (g > 255) g = 255;
      lineBuf[x] = tftPaletteMap((uint8_t)g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
  }
  M5.Lcd.endWrite();
}

void drawBandpassNoiseVisualFrame() {
  static float low[NOISE_H] = {0};
  static float band[NOISE_H] = {0};
  static float lfo = 0.0f;
  lfo += 0.003f;
  if (lfo > 1.0f) lfo -= 1.0f;

  M5.Lcd.startWrite();
  for (int y = 0; y < NOISE_H; ++y) {
    float fc = 200.0f + 1800.0f * (0.5f + 0.5f * sinf(TAU_F * (lfo + y * 0.002f)));
    float f = 2.0f * sinf(3.14159265f * fc / (float)SAMPLE_RATE_HZ);
    float q = 0.3f;
    for (int x = 0; x < NOISE_W; ++x) {
      float in = ((float)random(-128, 128)) / 128.0f;
      low[y] += f * band[y];
      float high = in - low[y] - q * band[y];
      band[y] += f * high;
      float bp = band[y];
      if (bp < -1.0f) bp = -1.0f; if (bp > 1.0f) bp = 1.0f;
      uint8_t g = (uint8_t)((bp * 0.5f + 0.5f) * 255.0f);
      lineBuf[x] = tftPaletteMap(g);
    }
    M5.Lcd.pushImage(NOISE_X, NOISE_Y + y, NOISE_W, 1, lineBuf);
  }
  M5.Lcd.endWrite();
}

void drawEuclidVisualFrame() {
  static uint32_t lastStepMs = 0;
  static int stepIdx = 0;
  static const bool pat[16] = {1,0,0,1,0,0,1,0,1,0,0,1,0,1,0,0};

  uint32_t now = millis();
  if (now - lastStepMs >= 125) {
    lastStepMs = now;
    stepIdx = (stepIdx + 1) & 15;
  }

  M5.Lcd.fillRect(NOISE_X, NOISE_Y, NOISE_W, NOISE_H, TFT_BLACK);

  int cols = 16;
  int pad = 3;
  int w = (NOISE_W - (cols + 1) * pad) / cols;
  if (w < 4) w = 4;
  int h = NOISE_H - 2 * pad;
  if (h < 12) h = 12;

  for (int i = 0; i < cols; ++i) {
    int x = NOISE_X + pad + i * (w + pad);
    int y = NOISE_Y + pad;
    uint16_t fill = TFT_DARKGREY;
    if (pat[i]) fill = TFT_CYAN;
    if (i == stepIdx) {
      fill = pat[i] ? TFT_YELLOW : TFT_NAVY;
    }
    M5.Lcd.fillRect(x, y, w, h, fill);
    M5.Lcd.drawRect(x, y, w, h, TFT_BLACK);
  }
}

void drawToneVisualFrame(NoiseType t) {
  static float visPhase = 0.0f;
  static float lfo1 = 0.0f;
  static float lfo2 = 0.0f;

  M5.Lcd.fillRect(NOISE_X, NOISE_Y, NOISE_W, NOISE_H, TFT_BLACK);

  float cycles = 2.0f;
  switch (t) {
    case NoiseType::TONE_SAW: cycles = 3.0f; break;
    case NoiseType::TONE_CHIRP: cycles = 1.2f + 1.2f * sinf((float)millis() * 0.0012f); break;
    case NoiseType::TONE_SUPERSAW: cycles = 2.5f; break;
    case NoiseType::TONE_PWM: cycles = 2.0f; break;
    case NoiseType::FX_BITCRUSH: cycles = 2.0f; break;
    default: break;
  }

  float phasePerPixel = TAU_F * cycles / (float)NOISE_W;
  int yCenter = NOISE_Y + (NOISE_H / 2);
  int amp = (NOISE_H / 2) - 4;

  uint16_t color = TFT_WHITE;
  switch (t) {
    case NoiseType::TONE_SINE: color = TFT_GREEN; break;
    case NoiseType::TONE_SQUARE: color = TFT_RED; break;
    case NoiseType::TONE_TRIANGLE: color = TFT_CYAN; break;
    case NoiseType::TONE_SAW: color = TFT_YELLOW; break;
    case NoiseType::TONE_CHIRP: color = TFT_CYAN; break;
    case NoiseType::TONE_SHEPARD: color = TFT_MAGENTA; break;
    case NoiseType::TONE_SHEPARD_DOWN: color = TFT_PINK; break;
    case NoiseType::TONE_FM_BELL: color = TFT_NAVY; break;
    case NoiseType::TONE_AM_TREMOLO: color = TFT_MAGENTA; break;
    case NoiseType::TONE_KARPLUS: color = TFT_ORANGE; break;
    case NoiseType::TONE_MODAL_DRUM: color = TFT_ORANGE; break;
    case NoiseType::TONE_GRANULAR: color = TFT_CYAN; break;
    case NoiseType::TONE_SUPERSAW: color = TFT_YELLOW; break;
    case NoiseType::TONE_PWM: color = TFT_RED; break;
    case NoiseType::FX_BITCRUSH: color = TFT_PURPLE; break;
    case NoiseType::TONE_PHASE_DIST: color = TFT_DARKGREEN; break;
    case NoiseType::TONE_WAVEFOLD: color = TFT_LIGHTGREY; break;
    default: break;
  }
  if (g_useOverrideColor) color = g_graphColor;

  for (int x = 0; x < NOISE_W; ++x) {
    float ph = visPhase + phasePerPixel * x;
    float v = 0.0f;

    if (t == NoiseType::TONE_SINE || t == NoiseType::TONE_CHIRP) {
      v = sinf(ph);
    } else if (t == NoiseType::TONE_SQUARE) {
      v = (sinf(ph) >= 0.0f) ? 1.0f : -1.0f;
    } else if (t == NoiseType::TONE_TRIANGLE) {
      float p = fmodf(ph, TAU_F) / TAU_F;
      float saw = 2.0f * p - 1.0f;
      v = 2.0f * fabsf(saw) - 1.0f;
    } else if (t == NoiseType::TONE_SAW) {
      float p = fmodf(ph, TAU_F) / TAU_F;
      v = 2.0f * p - 1.0f;
    } else if (t == NoiseType::TONE_SHEPARD || t == NoiseType::TONE_SHEPARD_DOWN) {
      v = 0.6f * sinf(ph) + 0.3f * sinf(2.0f * ph + lfo1) + 0.15f * sinf(4.0f * ph + 2.0f * lfo1);
      v *= 0.9f;
    } else if (t == NoiseType::TONE_FM_BELL) {
      float mod = 0.25f;
      v = sinf(ph + 1.8f * sinf(mod * ph + lfo2));
    } else if (t == NoiseType::TONE_AM_TREMOLO) {
      float ampEnv = 0.3f + 0.7f * (0.5f * (1.0f + sinf(lfo1)));
      v = sinf(ph) * ampEnv;
    } else if (t == NoiseType::TONE_KARPLUS) {
      float p = fmodf(ph, TAU_F) / TAU_F;
      float env = expf(-3.0f * p);
      v = sinf(ph) * env;
    } else if (t == NoiseType::TONE_MODAL_DRUM) {
      float p = fmodf(ph, TAU_F) / TAU_F;
      float env = expf(-4.0f * p);
      v = (1.0f * sinf(ph) + 0.6f * sinf(1.7f * ph + 0.5f) + 0.45f * sinf(2.5f * ph + 1.1f)) * env * 0.8f;
    } else if (t == NoiseType::TONE_GRANULAR) {
      float gate = (sinf(5.0f * ph + lfo2) > 0.75f) ? 1.0f : 0.0f;
      v = sinf(ph) * gate;
    } else if (t == NoiseType::TONE_SUPERSAW) {
      float s1 = 2.0f * fmodf(0.985f * ph, TAU_F) / TAU_F - 1.0f;
      float s2 = 2.0f * fmodf(0.995f * ph, TAU_F) / TAU_F - 1.0f;
      float s3 = 2.0f * fmodf(1.000f * ph, TAU_F) / TAU_F - 1.0f;
      float s4 = 2.0f * fmodf(1.005f * ph, TAU_F) / TAU_F - 1.0f;
      float s5 = 2.0f * fmodf(1.015f * ph, TAU_F) / TAU_F - 1.0f;
      v = (s1 + s2 + s3 + s4 + s5) / 5.0f;
    } else if (t == NoiseType::TONE_PWM) {
      float duty = 0.5f + 0.4f * sinf(lfo1);
      float p = fmodf(ph, TAU_F) / TAU_F;
      v = (p < duty) ? 1.0f : -1.0f;
    } else if (t == NoiseType::FX_BITCRUSH) {
      float x = 0.5f * (sinf(ph) + 1.0f);
      const int q = 8;
      int qi = (int)roundf(x * (q - 1));
      float xq = (float)qi / (float)(q - 1);
      v = 2.0f * xq - 1.0f;
    } else if (t == NoiseType::TONE_PHASE_DIST) {
      float amt = 1.0f + 0.2f * sinf(lfo2);
      v = sinf(ph + amt * sinf(ph));
    } else if (t == NoiseType::TONE_WAVEFOLD) {
      v = tanhf(2.5f * sinf(ph));
    } else {
      v = sinf(ph);
    }

    int y = yCenter - (int)(v * amp);
    if (y < NOISE_Y) y = NOISE_Y;
    if (y > NOISE_Y + NOISE_H - 3) y = NOISE_Y + NOISE_H - 3;
    M5.Lcd.drawFastVLine(NOISE_X + x, y - 1, 3, color);
  }

  float scroll = 0.12f;
  if (t == NoiseType::TONE_SHEPARD_DOWN) scroll = -scroll;
  visPhase += scroll;
  if (visPhase > TAU_F) visPhase -= TAU_F;
  if (visPhase < 0.0f) visPhase += TAU_F;

  lfo1 += 0.04f;
  if (lfo1 > TAU_F) lfo1 -= TAU_F;
  lfo2 += 0.02f;
  if (lfo2 > TAU_F) lfo2 -= TAU_F;
}

void drawWaveformFrame(NoiseType t) {
  uint16_t widx = g_visWriteIdx;
  uint16_t start = (uint16_t)((widx - (uint16_t)NOISE_W) & VIS_RING_MASK);

  M5.Lcd.fillRect(NOISE_X, NOISE_Y, NOISE_W, NOISE_H, TFT_BLACK);

  int yCenter = NOISE_Y + (NOISE_H / 2);
  int amp = (NOISE_H / 2) - 3;

  uint16_t color = TFT_WHITE;
  switch (t) {
    case NoiseType::TONE_SINE: color = TFT_GREEN; break;
    case NoiseType::TONE_SQUARE: color = TFT_RED; break;
    case NoiseType::TONE_TRIANGLE: color = TFT_CYAN; break;
    case NoiseType::TONE_SAW: color = TFT_YELLOW; break;
    case NoiseType::TONE_CHIRP: color = TFT_CYAN; break;
    case NoiseType::TONE_SHEPARD: color = TFT_MAGENTA; break;
    case NoiseType::TONE_SHEPARD_DOWN: color = TFT_MAGENTA; break;
    case NoiseType::TONE_FM_BELL: color = TFT_BLUE; break;
    case NoiseType::TONE_AM_TREMOLO: color = TFT_MAGENTA; break;
    case NoiseType::TONE_KARPLUS: color = TFT_ORANGE; break;
    case NoiseType::TONE_MODAL_DRUM: color = TFT_ORANGE; break;
    case NoiseType::TONE_GRANULAR: color = TFT_CYAN; break;
    case NoiseType::TONE_SUPERSAW: color = TFT_YELLOW; break;
    case NoiseType::TONE_PWM: color = TFT_RED; break;
    case NoiseType::FX_BITCRUSH: color = TFT_WHITE; break;
    case NoiseType::TONE_PHASE_DIST: color = TFT_GREEN; break;
    case NoiseType::TONE_WAVEFOLD: color = TFT_WHITE; break;
    default: break;
  }
  if (g_useOverrideColor) color = g_graphColor;


  int prevX = NOISE_X;
  int prevY = yCenter;
  for (int x = 0; x < NOISE_W; ++x) {
    uint8_t s = isPlaying ? g_visRing[(uint16_t)((start + x) & VIS_RING_MASK)] : nextAudioSample(t);
    int centered = (int)s - 128;
    int y = yCenter - (centered * amp) / 127;
    if (y < NOISE_Y) y = NOISE_Y;
    if (y > NOISE_Y + NOISE_H - 1) y = NOISE_Y + NOISE_H - 1;
    if (x > 0) {
      M5.Lcd.drawLine(prevX, prevY, NOISE_X + x, y, color);
    }
    prevX = NOISE_X + x;
    prevY = y;
  }
}

void drawNoiseFrame(NoiseType t) {
  g_visualType = t;
  // Always render the oscilloscope waveform so paused frames represent the real audio waveform.
  drawWaveformFrame(t);
}

void setVisualType(NoiseType type) {
  g_visualType = type;
}

void pushToVisRingBuffer(uint8_t sample) {
  g_visRing[(g_visWriteIdx + 1) & VIS_RING_MASK] = sample;
  g_visWriteIdx = (g_visWriteIdx + 1) & VIS_RING_MASK;
}

void initVisualState() {
  g_visualType = NoiseType::NOISE_WHITE;
  g_visWriteIdx = 0;
}

void randomizeGraphColor() {
  static const uint16_t palette[] = {
    TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE,
    TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE,
    TFT_PINK, TFT_PURPLE, TFT_NAVY, TFT_LIGHTGREY
  };
  static uint16_t last = TFT_WHITE;
  int n = (int)(sizeof(palette) / sizeof(palette[0]));
  uint16_t chosen = last;
  for (int tries = 0; tries < 5 && chosen == last; ++tries) {
    chosen = palette[random(0, n)];
  }
  last = chosen;
  g_graphColor = chosen;
  g_useOverrideColor = true;
}
