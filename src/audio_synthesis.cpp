#include "audio_synthesis.h"
#include "config.h"
#include "types.h"
#include "audio_extras.h"
#include <Arduino.h>
#include <math.h>
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Audio state (volatile for FreeRTOS task access)
volatile NoiseType g_audioNoise = NoiseType::NOISE_WHITE;
volatile bool g_audioRunning = false;
volatile float g_phase = 0.0f;
volatile float g_phase_mod = 0.0f;
volatile float g_chirpFreq = 200.0f;
volatile int g_chirpDir = 1;
volatile float g_shepBaseHzUp = 110.0f;
volatile float g_shepBaseHzDown = 1760.0f;
volatile float g_shepPhaseUp[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
volatile float g_shepPhaseDown[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
volatile float g_masterGain = 1.0f;

// Oscilloscope ring buffer (shared with visual)
extern volatile uint8_t g_visRing[1024];
extern volatile uint16_t g_visWriteIdx;


uint8_t nextWhiteSample() {
  int s = random(-128, 128);
  return (uint8_t)(s + 128);
}

uint8_t nextPinkSample() {
  static const int OCTAVES = 16;
  static uint32_t counter = 0;
  static int32_t rows[OCTAVES];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < OCTAVES; ++i) rows[i] = random(-32768, 32767);
    init = true;
  }
  counter++;
  uint32_t ctz = __builtin_ctz(counter);
  if (ctz >= OCTAVES) ctz = OCTAVES - 1;
  for (uint32_t i = 0; i <= ctz; ++i) {
    rows[i] = random(-32768, 32767);
  }
  int64_t sum = 0;
  for (int i = 0; i < OCTAVES; ++i) sum += rows[i];
  float s = (float)sum / (float)(OCTAVES * 32768.0f);
  int sample = (int)(s * 127.0f);
  return clampU8(sample + 128);
}

uint8_t nextBrownSample() {
  static float acc = 0.0f;
  float step = (float)random(-64, 65) / 256.0f;
  acc += step;
  acc *= 0.995f;
  if (acc < -1.0f) acc = -1.0f;
  if (acc > 1.0f) acc = 1.0f;
  int sample = (int)(acc * 127.0f);
  return clampU8(sample + 128);
}

uint8_t nextBlueSample() {
  static int prevW = 0;
  int w = random(-128, 128);
  int diff = w - prevW;
  prevW = w;
  int sample = (w + diff) / 2;
  if (sample < -128) sample = -128;
  if (sample > 127) sample = 127;
  return (uint8_t)(sample + 128);
}

uint8_t nextVioletSample() {
  static int w1 = 0, w2 = 0;
  int w0 = random(-128, 128);
  int sample = w0 - 2 * w1 + w2;
  w2 = w1;
  w1 = w0;
  sample /= 3;
  if (sample < -128) sample = -128;
  if (sample > 127) sample = 127;
  return (uint8_t)(sample + 128);
}

uint8_t nextToneSample(NoiseType t) {
  float freq = 440.0f;
  float step = 0.0f;
  float v = 0.0f;

  switch (t) {
    case NoiseType::TONE_SINE:
    case NoiseType::TONE_SQUARE:
    case NoiseType::TONE_TRIANGLE:
      freq = 440.0f;
      step = TAU_F * freq / (float)SAMPLE_RATE_HZ;
      step += ((float)random(-1, 2)) * 0.00005f;
      g_phase += step;
      if (g_phase >= TAU_F) g_phase -= TAU_F;
      if (t == NoiseType::TONE_SINE) {
        v = sinf(g_phase);
      } else if (t == NoiseType::TONE_SQUARE) {
        v = (sinf(g_phase) >= 0.0f) ? 1.0f : -1.0f;
      } else {
        float p = g_phase / TAU_F;
        float saw = 2.0f * p - 1.0f;
        v = 2.0f * fabsf(saw) - 1.0f;
      }
      break;

    case NoiseType::TONE_SAW:
      freq = 220.0f;
      step = TAU_F * freq / (float)SAMPLE_RATE_HZ;
      step += ((float)random(-1, 2)) * 0.00005f;
      g_phase += step;
      if (g_phase >= TAU_F) g_phase -= TAU_F;
      v = 2.0f * (g_phase / TAU_F) - 1.0f;
      break;

    case NoiseType::TONE_CHIRP: {
      freq = g_chirpFreq;
      const float delta = 1000.0f / (SAMPLE_RATE_HZ * 4.0f);
      g_chirpFreq += (g_chirpDir > 0 ? delta : -delta);
      if (g_chirpFreq > 1200.0f) { g_chirpFreq = 1200.0f; g_chirpDir = -1; }
      if (g_chirpFreq < 200.0f) { g_chirpFreq = 200.0f; g_chirpDir = 1; }
      step = TAU_F * freq / (float)SAMPLE_RATE_HZ;
      g_phase += step;
      if (g_phase >= TAU_F) g_phase -= TAU_F;
      v = sinf(g_phase);
      break;
    }
    
    case NoiseType::TONE_FM_BELL: {
      float fc = 440.0f, fm = 110.0f, beta = 2.0f;
      float step_m = TAU_F * fm / (float)SAMPLE_RATE_HZ;
      g_phase_mod += step_m;
      if (g_phase_mod >= TAU_F) g_phase_mod -= TAU_F;
      float instFreq = fc + beta * fm * sinf(g_phase_mod);
      float step_c = TAU_F * instFreq / (float)SAMPLE_RATE_HZ;
      g_phase += step_c;
      if (g_phase >= TAU_F) g_phase -= TAU_F;
      v = sinf(g_phase);
      break;
    }

    case NoiseType::TONE_AM_TREMOLO: {
      float fc = 440.0f, fm = 5.0f, depth = 0.8f;
      float step_c = TAU_F * fc / (float)SAMPLE_RATE_HZ;
      float step_m = TAU_F * fm / (float)SAMPLE_RATE_HZ;
      g_phase += step_c;
      if (g_phase >= TAU_F) g_phase -= TAU_F;
      g_phase_mod += step_m;
      if (g_phase_mod >= TAU_F) g_phase_mod -= TAU_F;
      float carrier = sinf(g_phase);
      float mod = 0.5f * (1.0f + sinf(g_phase_mod));
      float amp = (1.0f - depth) + depth * mod;
      v = carrier * amp;
      break;
    }

    default:
      v = 0.0f;
      break;
  }

  v *= 0.9f;
  int s = (int)(v * 127.0f);
  return clampU8(s + 128);
}

uint8_t nextShepardUpU8() {
  const int PARTS = 12;
  const float center = 440.0f;
  const float rateSec = 6.0f;
  const float rate = powf(2.0f, 1.0f / (SAMPLE_RATE_HZ * rateSec));

  g_shepBaseHzUp *= rate;
  if (g_shepBaseHzUp > center * 2.0f) g_shepBaseHzUp *= 0.5f;

  float sum = 0.0f, wsum = 0.0f;
  const float sigma = 0.55f;

  for (int j = 0; j < PARTS; ++j) {
    int k = j - (PARTS/2 - 1);
    float f = g_shepBaseHzUp * powf(2.0f, (float)k);
    if (f < 20.0f || f > 6000.0f) continue;

    float step = TAU_F * f / (float)SAMPLE_RATE_HZ;
    g_shepPhaseUp[j] += step;
    if (g_shepPhaseUp[j] >= TAU_F) g_shepPhaseUp[j] -= TAU_F;

    float o = log2f(f / center);
    float w = expf(-0.5f * (o * o) / (sigma * sigma));
    sum += w * sinf(g_shepPhaseUp[j]);
    wsum += w;
  }

  float v = (wsum > 0.0f) ? (sum / wsum) : 0.0f;
  int s = (int)(v * 127.0f);
  return clampU8(s + 128);
}

uint8_t nextShepardDownU8() {
  const int PARTS = 12;
  const float center = 440.0f;
  const float rateSec = 6.0f;
  const float rate = powf(2.0f, 1.0f / (SAMPLE_RATE_HZ * rateSec));

  g_shepBaseHzDown /= rate;
  if (g_shepBaseHzDown < center * 0.5f) g_shepBaseHzDown *= 2.0f;

  float sum = 0.0f, wsum = 0.0f;
  const float sigma = 0.55f;

  for (int j = 0; j < PARTS; ++j) {
    int k = j - (PARTS/2 - 1);
    float f = g_shepBaseHzDown * powf(2.0f, (float)k);
    if (f < 20.0f || f > 6000.0f) continue;

    float step = TAU_F * f / (float)SAMPLE_RATE_HZ;
    g_shepPhaseDown[j] += step;
    if (g_shepPhaseDown[j] >= TAU_F) g_shepPhaseDown[j] -= TAU_F;

    float o = log2f(f / center);
    float w = expf(-0.5f * (o * o) / (sigma * sigma));
    sum += w * sinf(g_shepPhaseDown[j]);
    wsum += w;
  }

  float v = (wsum > 0.0f) ? (sum / wsum) : 0.0f;
  int s = (int)(v * 127.0f);
  return clampU8(s + 128);
}

uint8_t nextKarplusU8() {
  static const int MAX_KS_LEN = 256;
  static float buf[MAX_KS_LEN];
  static int len = 0, idx = 0, repluck = 0;
  if (len == 0) {
    float f = 196.0f;
    len = (int)((float)SAMPLE_RATE_HZ / f);
    if (len < 8) len = 8;
    if (len > MAX_KS_LEN) len = MAX_KS_LEN;
    repluck = (int)(0.8f * SAMPLE_RATE_HZ);
    for (int i = 0; i < len; ++i) buf[i] = ((float)random(-128, 128)) / 256.0f;
  }
  if (--repluck <= 0) {
    for (int i = 0; i < len; ++i) buf[i] = ((float)random(-128, 128)) / 256.0f;
    repluck = (int)(0.8f * SAMPLE_RATE_HZ);
  }
  int next = (idx + 1) % len;
  float y = buf[idx];
  buf[idx] = 0.5f * (buf[idx] + buf[next]) * 0.996f;
  idx = next;
  return clampU8((int)(y * 127.0f) + 128);
}

uint8_t nextModalDrumU8() {
  static const int N = 4;
  static const float freqs[N] = {180.0f, 300.0f, 460.0f, 620.0f};
  static const float gains[N] = {1.0f, 0.6f, 0.45f, 0.35f};
  static float phase[N] = {0}, env = 0.0f;
  static int retrig = 0;
  if (env < 0.0008f && retrig <= 0) {
    env = 1.0f;
    for (int i = 0; i < N; ++i) phase[i] = (float)random(0, 1000) * 0.001f * TAU_F;
    retrig = (int)(0.6f * SAMPLE_RATE_HZ);
  }
  if (retrig > 0) retrig--;
  float sum = 0.0f;
  for (int i = 0; i < N; ++i) {
    float step = TAU_F * freqs[i] / (float)SAMPLE_RATE_HZ;
    phase[i] += step;
    if (phase[i] >= TAU_F) phase[i] -= TAU_F;
    sum += gains[i] * sinf(phase[i]);
  }
  sum *= env;
  env *= 0.9992f;
  int s = (int)(sum * 100.0f);
  if (s < -127) s = -127;
  if (s > 127) s = 127;
  return (uint8_t)(s + 128);
}

uint8_t nextGranularU8() {
  struct Grain { bool on; float phase, dphase, amp, adec; int left; };
  static Grain g[8] = {};
  if (random(0, 1000) < 6) {
    for (int i = 0; i < 8; ++i) if (!g[i].on) {
      float f = (float)random(200, 2000);
      int dur = random((int)(0.05f * SAMPLE_RATE_HZ), (int)(0.20f * SAMPLE_RATE_HZ));
      g[i].on = true;
      g[i].phase = 0.0f;
      g[i].dphase = TAU_F * f / (float)SAMPLE_RATE_HZ;
      g[i].amp = 0.15f + (float)random(0, 100) * 0.003f;
      g[i].adec = powf(0.001f, 1.0f / (float)dur);
      g[i].left = dur;
      break;
    }
  }
  float sum = 0.0f;
  for (int i = 0; i < 8; ++i) if (g[i].on) {
    sum += g[i].amp * sinf(g[i].phase);
    g[i].phase += g[i].dphase;
    if (g[i].phase >= TAU_F) g[i].phase -= TAU_F;
    g[i].amp *= g[i].adec;
    if (--g[i].left <= 0 || g[i].amp < 0.001f) g[i].on = false;
  }
  return clampU8((int)(sum * 127.0f) + 128);
}

uint8_t nextSuperSawU8() {
  static const int N = 6;
  static float phase[N] = {0};
  static const float det[N] = {0.985f, 0.992f, 0.998f, 1.002f, 1.008f, 1.015f};
  float base = 110.0f, sum = 0.0f;
  for (int i = 0; i < N; ++i) {
    float f = base * det[i];
    phase[i] += f / (float)SAMPLE_RATE_HZ;
    if (phase[i] >= 1.0f) phase[i] -= 1.0f;
    sum += 2.0f * phase[i] - 1.0f;
  }
  sum /= (float)N;
  return clampU8((int)(sum * 120.0f) + 128);
}

uint8_t nextPwmU8() {
  static float p = 0.0f, lfo = 0.0f;
  float fc = 110.0f, fm = 2.0f;
  p += fc / (float)SAMPLE_RATE_HZ;
  if (p >= 1.0f) p -= 1.0f;
  lfo += fm / (float)SAMPLE_RATE_HZ;
  if (lfo >= 1.0f) lfo -= 1.0f;
  float duty = 0.5f + 0.4f * sinf(TAU_F * lfo);
  float v = (p < duty) ? 1.0f : -1.0f;
  return clampU8((int)(v * 110.0f) + 128);
}

uint8_t nextBitcrushU8() {
  static float ph = 0.0f, held = 0.0f;
  static int hold = 0;
  const int holdN = 8, q = 8;
  if (hold == 0) {
    float f = 220.0f;
    ph += TAU_F * f / (float)SAMPLE_RATE_HZ;
    if (ph >= TAU_F) ph -= TAU_F;
    float v = sinf(ph);
    float x = (v * 0.5f) + 0.5f;
    int qi = (int)roundf(x * (q - 1));
    float xq = (float)qi / (float)(q - 1);
    held = (xq * 2.0f) - 1.0f;
    hold = holdN;
  }
  hold--;
  return clampU8((int)(held * 120.0f) + 128);
}

uint8_t nextPhaseDistU8() {
  static float ph = 0.0f, lfo = 0.0f;
  float fc = 220.0f, fm = 1.2f;
  ph += TAU_F * fc / (float)SAMPLE_RATE_HZ;
  if (ph >= TAU_F) ph -= TAU_F;
  lfo += TAU_F * fm / (float)SAMPLE_RATE_HZ;
  if (lfo >= TAU_F) lfo -= TAU_F;
  float amt = 1.2f * (0.5f + 0.5f * sinf(lfo));
  float v = sinf(ph + amt * sinf(ph));
  return clampU8((int)(v * 120.0f) + 128);
}

uint8_t nextWavefoldU8() {
  static float ph = 0.0f, lfo = 0.0f;
  float fc = 220.0f, fm = 0.8f;
  ph += TAU_F * fc / (float)SAMPLE_RATE_HZ;
  if (ph >= TAU_F) ph -= TAU_F;
  lfo += TAU_F * fm / (float)SAMPLE_RATE_HZ;
  if (lfo >= TAU_F) lfo -= TAU_F;
  float gain = 1.5f + 2.0f * (0.5f + 0.5f * sinf(lfo));
  float v = tanhf(gain * sinf(ph));
  return clampU8((int)(v * 120.0f) + 128);
}

uint8_t nextBandpassNoiseU8() {
  static float low = 0.0f, band = 0.0f, lfo = 0.0f;
  float x = ((float)random(-128, 128)) / 128.0f;
  lfo += 0.3f / (float)SAMPLE_RATE_HZ;
  if (lfo >= 1.0f) lfo -= 1.0f;
  float fc = 200.0f + 1800.0f * (0.5f + 0.5f * sinf(TAU_F * lfo));
  float f = 2.0f * sinf(3.14159265f * fc / (float)SAMPLE_RATE_HZ);
  float q = 0.3f;
  low += f * band;
  float high = x - low - q * band;
  band += f * high;
  float bp = band;
  if (bp < -1.0f) bp = -1.0f;
  if (bp > 1.0f) bp = 1.0f;
  return clampU8((int)(bp * 127.0f) + 128);
}

uint8_t nextEuclidU8() {
  static const bool pat[16] = {1,0,0,1,0,0,1,0,1,0,0,1,0,1,0,0};
  static int idx = 0, toStep = 0;
  static float env = 0.0f, ph = 0.0f;
  const int stepSamples = (int)((float)SAMPLE_RATE_HZ / 8.0f);
  if (--toStep <= 0) {
    if (pat[idx]) env = 1.0f;
    idx = (idx + 1) & 15;
    toStep = stepSamples;
  }
  float f = 1000.0f;
  ph += TAU_F * f / (float)SAMPLE_RATE_HZ;
  if (ph >= TAU_F) ph -= TAU_F;
  float v = env * sinf(ph);
  env *= 0.995f;
  return clampU8((int)(v * 127.0f) + 128);
}

uint8_t nextEuclid716U8() {
  static const bool pat[16] = {1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0};
  static int idx = 0, toStep = 0;
  static float env = 0.0f, ph = 0.0f;
  const int stepSamples = (int)((float)SAMPLE_RATE_HZ / 8.0f);
  if (--toStep <= 0) {
    if (pat[idx]) env = 1.0f;
    idx = (idx + 1) & 15;
    toStep = stepSamples;
  }
  float f = 1600.0f;
  ph += TAU_F * f / (float)SAMPLE_RATE_HZ;
  if (ph >= TAU_F) ph -= TAU_F;
  float v = env * sinf(ph);
  env *= 0.994f;
  return clampU8((int)(v * 127.0f) + 128);
}

uint8_t nextPoly34U8() {
  static int toA = 0, toB = 0;
  static float envA = 0.0f, envB = 0.0f, ph = 0.0f;
  const int stepA = (int)((float)SAMPLE_RATE_HZ / 3.0f);
  const int stepB = (int)((float)SAMPLE_RATE_HZ / 4.0f);
  if (--toA <= 0) { envA = 1.0f; toA = stepA; }
  if (--toB <= 0) { envB = 1.0f; toB = stepB; }
  float f = 1200.0f;
  ph += TAU_F * f / (float)SAMPLE_RATE_HZ;
  if (ph >= TAU_F) ph -= TAU_F;
  float v = (envA + envB) * 0.5f * sinf(ph);
  envA *= 0.994f;
  envB *= 0.994f;
  return clampU8((int)(v * 127.0f) + 128);
}

uint8_t nextRingModU8() {
  static float phc = 0.0f, phm = 0.0f;
  float fc = 220.0f, fm = 60.0f;
  phc += TAU_F * fc / (float)SAMPLE_RATE_HZ;
  if (phc >= TAU_F) phc -= TAU_F;
  phm += TAU_F * fm / (float)SAMPLE_RATE_HZ;
  if (phm >= TAU_F) phm -= TAU_F;
  float v = sinf(phc) * sinf(phm);
  return clampU8((int)(v * 120.0f) + 128);
}

uint8_t nextChorusU8() {
  static float ph1 = 0.0f, ph2 = 0.0f, ph3 = 0.0f;
  static float l1 = 0.0f, l2 = 1.3f;
  float base = 220.0f;
  l1 += 0.002f; if (l1 >= TAU_F) l1 -= TAU_F;
  l2 += 0.0013f; if (l2 >= TAU_F) l2 -= TAU_F;
  float f1 = base * (1.0f + 0.004f * sinf(l1));
  float f2 = base * (1.0f - 0.005f * sinf(l2));
  float f3 = base;
  ph1 += TAU_F * f1 / (float)SAMPLE_RATE_HZ; if (ph1 >= TAU_F) ph1 -= TAU_F;
  ph2 += TAU_F * f2 / (float)SAMPLE_RATE_HZ; if (ph2 >= TAU_F) ph2 -= TAU_F;
  ph3 += TAU_F * f3 / (float)SAMPLE_RATE_HZ; if (ph3 >= TAU_F) ph3 -= TAU_F;
  float v = (sinf(ph1) + sinf(ph2) + sinf(ph3)) / 3.0f;
  return clampU8((int)(v * 120.0f) + 128);
}

uint8_t nextSampleHoldU8() {
  static int hold = 0;
  static float target = 0.0f, current = 0.0f;
  if (--hold <= 0) {
    target = ((float)random(-128, 128)) / 128.0f;
    hold = random(30, 800);
  }
  current += 0.05f * (target - current);
  if (current < -1.0f) current = -1.0f;
  if (current > 1.0f) current = 1.0f;
  return clampU8((int)(current * 127.0f) + 128);
}

uint8_t nextFormantU8() {
  static float low1=0, band1=0, low2=0, band2=0, low3=0, band3=0;
  const float fc1 = 700.0f, fc2 = 1200.0f, fc3 = 2400.0f, q = 0.2f;
  float x = ((float)random(-128, 128)) / 128.0f;
  auto svf = [q](float in, float fc, float& low, float& band) -> float {
    float f = 2.0f * sinf(3.14159265f * fc / (float)SAMPLE_RATE_HZ);
    low += f * band;
    float high = in - low - q * band;
    band += f * high;
    return band;
  };
  float y1 = svf(x, fc1, low1, band1);
  float y2 = svf(x, fc2, low2, band2);
  float y3 = svf(x, fc3, low3, band3);
  float v = (y1 * 0.9f + y2 * 0.7f + y3 * 0.5f) * 0.7f;
  if (v < -1.0f) v = -1.0f;
  if (v > 1.0f) v = 1.0f;
  return clampU8((int)(v * 127.0f) + 128);
}

uint8_t nextSyncU8() {
  static float phM = 0.0f, phS = 0.0f;
  float fM = 110.0f, fS = 330.0f;
  phM += fM / (float)SAMPLE_RATE_HZ;
  if (phM >= 1.0f) { phM -= 1.0f; phS = 0.0f; }
  phS += fS / (float)SAMPLE_RATE_HZ;
  if (phS >= 1.0f) phS -= 1.0f;
  float v = 2.0f * phS - 1.0f;
  return clampU8((int)(v * 120.0f) + 128);
}

uint8_t nextSuperSquareU8() {
  static float p1=0.0f, p2=0.0f, p3=0.0f, p4=0.0f;
  float base = 110.0f;
  float d1 = 0.985f, d2 = 0.997f, d3 = 1.003f, d4 = 1.015f;
  p1 += (base*d1) / (float)SAMPLE_RATE_HZ; if (p1 >= 1.0f) p1 -= 1.0f;
  p2 += (base*d2) / (float)SAMPLE_RATE_HZ; if (p2 >= 1.0f) p2 -= 1.0f;
  p3 += (base*d3) / (float)SAMPLE_RATE_HZ; if (p3 >= 1.0f) p3 -= 1.0f;
  p4 += (base*d4) / (float)SAMPLE_RATE_HZ; if (p4 >= 1.0f) p4 -= 1.0f;
  auto sq = [](float ph)->float { return (ph < 0.5f) ? 1.0f : -1.0f; };
  float v = (sq(p1)+sq(p2)+sq(p3)+sq(p4))/4.0f;
  return clampU8((int)(v * 110.0f) + 128);
}

uint8_t nextAudioSample(NoiseType t) {
  // Handle extra modes first (uses same gain normalization path)
  if (isExtraType(t)) {
    uint8_t rawExtra = nextAudioSampleExtra(t);
    int centeredExtra = (int)rawExtra - 128;
    float gExtra = getGainForType(t);
    int scaledExtra = (int)(centeredExtra * gExtra * g_masterGain);
    return clampU8(scaledExtra + 128);
  }
  uint8_t raw = 128;
  switch (t) {
    case NoiseType::NOISE_WHITE:       raw = nextWhiteSample();   break;
    case NoiseType::NOISE_PINK:        raw = nextPinkSample();    break;
    case NoiseType::NOISE_BROWN:       raw = nextBrownSample();   break;
    case NoiseType::NOISE_BLUE:        raw = nextBlueSample();    break;
    case NoiseType::NOISE_VIOLET:      raw = nextVioletSample();  break;
    case NoiseType::TONE_SINE:
    case NoiseType::TONE_SQUARE:
    case NoiseType::TONE_TRIANGLE:
    case NoiseType::TONE_SAW:
    case NoiseType::TONE_CHIRP:
    case NoiseType::TONE_FM_BELL:
    case NoiseType::TONE_AM_TREMOLO:   raw = nextToneSample(t);   break;
    case NoiseType::TONE_KARPLUS:      raw = nextKarplusU8();     break;
    case NoiseType::TONE_MODAL_DRUM:   raw = nextModalDrumU8();   break;
    case NoiseType::TONE_GRANULAR:     raw = nextGranularU8();    break;
    case NoiseType::TONE_SUPERSAW:     raw = nextSuperSawU8();    break;
    case NoiseType::TONE_PWM:          raw = nextPwmU8();         break;
    case NoiseType::FX_BITCRUSH:       raw = nextBitcrushU8();    break;
    case NoiseType::TONE_PHASE_DIST:   raw = nextPhaseDistU8();   break;
    case NoiseType::TONE_WAVEFOLD:     raw = nextWavefoldU8();    break;
    case NoiseType::NOISE_BANDPASS:    raw = nextBandpassNoiseU8(); break;
    case NoiseType::RHYTHM_EUCLIDEAN:  raw = nextEuclidU8();      break;
    case NoiseType::TONE_SHEPARD:      raw = nextShepardUpU8();   break;
    case NoiseType::TONE_SHEPARD_DOWN: raw = nextShepardDownU8(); break;
    case NoiseType::RHYTHM_EUCLIDEAN_7_16: raw = nextEuclid716U8();   break;
    case NoiseType::RHYTHM_POLY_3_4:       raw = nextPoly34U8();      break;
    case NoiseType::TONE_RING_MOD:         raw = nextRingModU8();     break;
    case NoiseType::TONE_CHORUS:           raw = nextChorusU8();      break;
    case NoiseType::FX_SAMPLE_HOLD:        raw = nextSampleHoldU8();  break;
    case NoiseType::FX_FORMANT:            raw = nextFormantU8();     break;
    case NoiseType::TONE_SYNC:             raw = nextSyncU8();        break;
    case NoiseType::TONE_SUPER_SQUARE:     raw = nextSuperSquareU8(); break;
    default: break;
  }
  int centered = (int)raw - 128;
  float g = getGainForType(t);
  int scaled = (int)(centered * g * g_masterGain);
  return clampU8(scaled + 128);
}

void audioTask(void* param) {
  const uint32_t samplePeriodUs = 1000000UL / SAMPLE_RATE_HZ;
  while (true) {
    if (g_audioRunning) {
      for (int i = 0; i < 256; ++i) {
        uint8_t s = nextAudioSample((NoiseType)g_audioNoise);
        g_visRing[(g_visWriteIdx + 1) & VIS_RING_MASK] = s;
        g_visWriteIdx = (g_visWriteIdx + 1) & VIS_RING_MASK;
        dacWrite(AUDIO_DAC_PIN, s);
        delayMicroseconds(samplePeriodUs);
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void initAudioState() {
  g_audioNoise = NoiseType::NOISE_WHITE;
  g_audioRunning = false;
  g_masterGain = 1.0f;
}

void setAudioRunning(bool running) {
  g_audioRunning = running;
}

void setAudioNoiseType(NoiseType type) {
  g_audioNoise = type;
}

void setMasterGain(float g) {
  if (g < 0.0f) g = 0.0f;
  if (g > 1.0f) g = 1.0f;
  g_masterGain = g;
}

float getMasterGain() {
  return g_masterGain;
}
