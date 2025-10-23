#include "audio_extras.h"
#include "audio_synthesis.h"  // clampU8
#include "config.h"
#include <Arduino.h>
#include <math.h>

/*
  Extra audio generators to expand the palette toward the user's list.
  Notes:
  - Hardware is single-DAC (mono). True stereo illusions (binaural, Haas, phase inversion, QSound)
    are not implemented here. We provide mono-safe approximations (e.g., acoustic beating).
  - Mosquito tone (~17.4 kHz) exceeds Nyquist at 11,025 Hz. We provide a near-Nyquist piercing tone instead.
  - These functions are designed to be called at SAMPLE_RATE_HZ, returning an unsigned 8-bit sample (0..255).
  - Integration:
      1) Add new enum entries into NoiseType (include/types.h).
      2) Map track indices in src/types.cpp:getCurrentNoiseType and add getNoiseTypeName/getGainForType entries.
      3) In src/audio_synthesis.cpp:nextAudioSample, before the default return, call nextAudioSampleExtra(t) if isExtraType(t).
*/

static inline float hz_to_step(float hz) {
  return TAU_F * hz / (float)SAMPLE_RATE_HZ;
}

/* =========================
   Generators (static)
   ========================= */

// 1) Isochronic Tones (gated single tone)
static uint8_t nextIsochronicU8() {
  static float ph = 0.0f;
  static float gatePh = 0.0f;
  const float fc = 440.0f;          // carrier
  const float fg = 9.0f;            // gating Hz (perceived beat)
  ph += hz_to_step(fc);
  if (ph >= TAU_F) ph -= TAU_F;
  gatePh += hz_to_step(fg);
  if (gatePh >= TAU_F) gatePh -= TAU_F;
  float gate = (sinf(gatePh) > 0.0f) ? 1.0f : 0.0f; // hard gate (isochronic)
  float v = sinf(ph) * gate * 0.95f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 2) Acoustic Beating (sum of two close sines -> physical amplitude beating)
static uint8_t nextAcousticBeatU8() {
  static float p1 = 0.0f, p2 = 0.0f;
  const float f1 = 440.0f;
  const float f2 = 446.0f; // 6 Hz beat
  p1 += hz_to_step(f1); if (p1 >= TAU_F) p1 -= TAU_F;
  p2 += hz_to_step(f2); if (p2 >= TAU_F) p2 -= TAU_F;
  float v = 0.5f * (sinf(p1) + sinf(p2));
  v *= 0.9f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 3) Missing Fundamental (sum harmonics 2f0..5f0, brain perceives f0)
static uint8_t nextMissingFundU8() {
  static float p = 0.0f;
  const float f0 = 180.0f;
  p += hz_to_step(f0);
  if (p >= TAU_F) p -= TAU_F;
  float v = 0.0f;
  v += (1.0f / 2.0f) * sinf(2.0f * p);
  v += (1.0f / 3.0f) * sinf(3.0f * p);
  v += (1.0f / 4.0f) * sinf(4.0f * p);
  v += (1.0f / 5.0f) * sinf(5.0f * p);
  v *= 0.9f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 4) Combination (Tartini) Tones via light nonlinear saturation
static uint8_t nextCombinationToneU8() {
  static float p1 = 0.0f, p2 = 0.0f;
  const float f1 = 700.0f, f2 = 880.0f;
  p1 += hz_to_step(f1); if (p1 >= TAU_F) p1 -= TAU_F;
  p2 += hz_to_step(f2); if (p2 >= TAU_F) p2 -= TAU_F;
  float s = 0.8f * sinf(p1) + 0.8f * sinf(p2);
  // Soft clip to create intermodulation products (sum/difference)
  float v = tanhf(1.8f * s);
  v *= 0.9f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 5) Infrasound (~12 Hz sine, very low amplitude to avoid DC issues)
static uint8_t nextInfrasoundU8() {
  static float p = 0.0f;
  const float f = 12.0f;
  p += hz_to_step(f);
  if (p >= TAU_F) p -= TAU_F;
  float v = 0.35f * sinf(p);
  return clampU8((int)(v * 127.0f) + 128);
}

// 6) Somatic Bass (50 Hz thumps with exponential hits)
static uint8_t nextSomaticBassU8() {
  static float p = 0.0f;
  static float env = 0.0f;
  static int countdown = 0;
  const float f = 55.0f;
  if (--countdown <= 0) {
    env = 1.0f;
    countdown = (int)(SAMPLE_RATE_HZ * 0.6f); // hit every ~0.6s
  }
  p += hz_to_step(f); if (p >= TAU_F) p -= TAU_F;
  float v = sinf(p) * env;
  env *= 0.996f;
  if (env < 0.0003f) env = 0.0003f;
  v *= 0.95f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 7) Ear Canal Resonance (~3 kHz prominent)
static uint8_t nextEarResonanceU8() {
  static float p = 0.0f;
  const float f = 3000.0f;
  p += hz_to_step(f); if (p >= TAU_F) p -= TAU_F;
  // Slight harmonic grit
  float v = 0.85f * sinf(p) + 0.2f * sinf(2.0f * p);
  v *= 0.7f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 8) Near-Nyquist Piercing Tone (~5 kHz for 11.025 kHz SR)
static uint8_t nextNearNyquistU8() {
  static float p = 0.0f;
  const float f = 5000.0f;
  p += hz_to_step(f); if (p >= TAU_F) p -= TAU_F;
  float v = 0.8f * sinf(p);
  return clampU8((int)(v * 127.0f) + 128);
}

// 9) Larsen-like Feedback Howl (resonator fed by noise)
static uint8_t nextFeedbackHowlU8() {
  // Second-order resonator y[n] = 2r cos(w) y[n-1] - r^2 y[n-2] + eps*x
  static float y1 = 0.0f, y2 = 0.0f;
  static float lfo = 0.0f;
  lfo += hz_to_step(0.12f); if (lfo >= TAU_F) lfo -= TAU_F;
  float fc = 2500.0f + 800.0f * sinf(lfo); // sweep
  float w = TAU_F * fc / (float)SAMPLE_RATE_HZ;
  float r = 0.9955f;                       // high-Q
  float a1 = 2.0f * r * cosf(w);
  float a2 = -r * r;
  float x = ((float)random(-128, 128)) / 128.0f * 0.0025f; // tiny noise drive
  float y = a1 * y1 + a2 * y2 + x;
  y2 = y1; y1 = y;
  if (y > 1.3f) y = 1.3f;
  if (y < -1.3f) y = -1.3f;
  float v = tanhf(1.2f * y);
  return clampU8((int)(v * 127.0f) + 128);
}

// 10) FM Metallic (audio-rate FM for clangor)
static uint8_t nextFMMetalU8() {
  static float pc = 0.0f, pm = 0.0f;
  const float fc = 330.0f;  // carrier
  const float fm = 780.0f;  // modulator
  const float beta = 3.2f;  // index
  pm += hz_to_step(fm); if (pm >= TAU_F) pm -= TAU_F;
  float inst = fc + beta * fm * sinf(pm);
  pc += hz_to_step(inst); if (pc >= TAU_F) pc -= TAU_F;
  float v = sinf(pc) * 0.95f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 11) Stutter / Glitch (repeat tiny grains)
static uint8_t nextStutterU8() {
  static int modeLeft = 0;
  static int len = 64;
  static int idx = 0;
  static float buffer[256];
  static bool inited = false;
  if (!inited) {
    for (int i = 0; i < 256; ++i) buffer[i] = 0.0f;
    inited = true;
  }
  if (modeLeft <= 0) {
    // Rebuild a small grain: either tone or noise
    len = random(18, 120);
    if (len > 256) len = 256;
    bool tone = random(0, 100) < 60;
    if (tone) {
      float ph = 0.0f, dph = hz_to_step((float)random(220, 1800));
      float env = 1.0f, dec = powf(0.01f, 1.0f / (float)len);
      for (int i = 0; i < len; ++i) {
        buffer[i] = sinf(ph) * env * 0.9f;
        ph += dph; if (ph >= TAU_F) ph -= TAU_F;
        env *= dec;
      }
    } else {
      for (int i = 0; i < len; ++i) {
        buffer[i] = ((float)random(-128, 128)) / 128.0f * 0.8f;
      }
    }
    idx = 0;
    modeLeft = random((int)(0.05f * SAMPLE_RATE_HZ), (int)(0.25f * SAMPLE_RATE_HZ));
  }
  float v = buffer[idx];
  idx = (idx + 1) % len;
  modeLeft--;
  if (v > 1.0f) v = 1.0f; if (v < -1.0f) v = -1.0f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 12) Phaser / Flanger-like comb (LFO delay on a simple tone)
static uint8_t nextPhaserU8() {
  static float ph = 0.0f, lfo = 0.0f;
  static const int BUF_SZ = 512;
  static float buf[BUF_SZ] = {0};
  static int w = 0;
  const float base = 330.0f;
  ph += hz_to_step(base); if (ph >= TAU_F) ph -= TAU_F;
  lfo += hz_to_step(0.2f); if (lfo >= TAU_F) lfo -= TAU_F;
  float vIn = sinf(ph);
  // Delay between [2..40] samples
  float d = 2.0f + 19.0f * (0.5f + 0.5f * sinf(lfo));
  int di = (int)d;
  int r = (w - di + BUF_SZ) & (BUF_SZ - 1);
  float vDel = buf[r];
  // Write with small feedback
  buf[w] = vIn + 0.6f * vDel;
  w = (w + 1) & (BUF_SZ - 1);
  float vOut = 0.6f * vIn + 0.6f * vDel;
  if (vOut > 1.0f) vOut = 1.0f; if (vOut < -1.0f) vOut = -1.0f;
  return clampU8((int)(vOut * 127.0f) + 128);
}

// 13) Doppler Effect (approach -> pass -> depart)
static uint8_t nextDopplerU8() {
  static float ph = 0.0f;
  static float t = 0.0f;
  t += 1.0f / (float)SAMPLE_RATE_HZ;
  if (t > 2.5f) t = 0.0f; // period ~2.5s
  // Triangular velocity profile -> frequency scaling
  float x = t < 1.25f ? (t / 1.25f) : (2.0f - t / 1.25f);
  // β in [-vmax, vmax], choose vmax ~ 0.25 (abstract)
  float beta = 0.5f * (2.0f * x - 1.0f) * 0.25f;
  float scale = sqrtf((1.0f + beta) / (1.0f - beta));
  float fc = 660.0f * scale;
  ph += hz_to_step(fc); if (ph >= TAU_F) ph -= TAU_F;
  // Amplitude loudest at closest approach
  float amp = 0.4f + 0.6f * (1.0f - fabsf(2.0f * x - 1.0f));
  float v = sinf(ph) * amp * 0.95f;
  return clampU8((int)(v * 127.0f) + 128);
}

// 14) Gated Reverb-ish burst
static uint8_t nextGatedReverbU8() {
  static const int D = 900;  // comb delay
  static float comb[D] = {0};
  static int w = 0;
  static float env = 0.0f;
  static int retrig = 0;

  if (--retrig <= 0) {
    env = 1.0f;
    retrig = (int)(SAMPLE_RATE_HZ * 0.9f);
  }
  // Excitation: short click/noise burst shaped
  float x = ((float)random(-128, 128)) / 128.0f * env;
  env *= 0.985f;

  int r = (w + 1) % D;
  float y = x + 0.80f * comb[r];
  // Hard gate tail
  static int tail = 0;
  if (env < 0.03f) {
    if (tail++ > (int)(0.18f * SAMPLE_RATE_HZ)) {
      y = 0.0f; // gate shut
    }
  } else {
    tail = 0;
  }

  comb[w] = y * 0.88f;
  w = r;
  if (y > 1.0f) y = 1.0f; if (y < -1.0f) y = -1.0f;
  return clampU8((int)(y * 127.0f) + 128);
}

// 15) Auditory Aliasing (sample-rate reduction on a bright tone)
static uint8_t nextAliasingBuzzU8() {
  static float ph = 0.0f;
  static float held = 0.0f;
  static int hold = 0;
  // High-ish source tone
  const float f = 1800.0f;
  // Downsample factor swept for moving alias texture
  static float lfo = 0.0f;
  lfo += hz_to_step(0.15f); if (lfo >= TAU_F) lfo -= TAU_F;
  int holdN = 2 + (int)roundf(12.0f * (0.5f + 0.5f * sinf(lfo))); // 2..14

  if (hold <= 0) {
    ph += hz_to_step(f); if (ph >= TAU_F) ph -= TAU_F;
    float v = sinf(ph) * 0.95f;
    held = v;
    hold = holdN;
  }
  hold--;
  return clampU8((int)(held * 127.0f) + 128);
}

/* =========================
   Integration helpers
   ========================= */

void initAudioExtras() {
  // Nothing required yet; static initializers handle start-up.
  // Keep for symmetry and future state resets if needed.
}

static bool extraTypeSupported(NoiseType t) {
  switch (t) {
    case NoiseType::TONE_ISOCHRONIC:
    case NoiseType::TONE_ACOUSTIC_BEAT:
    case NoiseType::TONE_MISSING_FUND:
    case NoiseType::TONE_COMBINATION_TONES:
    case NoiseType::TONE_INFRASOUND:
    case NoiseType::TONE_SOMATIC_BASS:
    case NoiseType::TONE_EAR_RESONANCE:
    case NoiseType::TONE_NEAR_NYQUIST:
    case NoiseType::TONE_FEEDBACK_HOWL:
    case NoiseType::TONE_FM_METAL:
    case NoiseType::FX_STUTTER:
    case NoiseType::FX_PHASER:
    case NoiseType::FX_DOPPLER:
    case NoiseType::FX_GATED_REVERB:
    case NoiseType::FX_ALIASING_BUZZ:
      return true;
    default:
      return false;
  }
}

bool isExtraType(NoiseType t) {
  return extraTypeSupported(t);
}

uint8_t nextAudioSampleExtra(NoiseType t) {
  switch (t) {
    case NoiseType::TONE_ISOCHRONIC:       return nextIsochronicU8();
    case NoiseType::TONE_ACOUSTIC_BEAT:    return nextAcousticBeatU8();
    case NoiseType::TONE_MISSING_FUND:     return nextMissingFundU8();
    case NoiseType::TONE_COMBINATION_TONES:return nextCombinationToneU8();
    case NoiseType::TONE_INFRASOUND:       return nextInfrasoundU8();
    case NoiseType::TONE_SOMATIC_BASS:     return nextSomaticBassU8();
    case NoiseType::TONE_EAR_RESONANCE:    return nextEarResonanceU8();
    case NoiseType::TONE_NEAR_NYQUIST:     return nextNearNyquistU8();
    case NoiseType::TONE_FEEDBACK_HOWL:    return nextFeedbackHowlU8();
    case NoiseType::TONE_FM_METAL:         return nextFMMetalU8();
    case NoiseType::FX_STUTTER:            return nextStutterU8();
    case NoiseType::FX_PHASER:             return nextPhaserU8();
    case NoiseType::FX_DOPPLER:            return nextDopplerU8();
    case NoiseType::FX_GATED_REVERB:       return nextGatedReverbU8();
    case NoiseType::FX_ALIASING_BUZZ:      return nextAliasingBuzzU8();
    default:
      return 128; // mid-level (acts as silence) for unsupported
  }
}

/*
  Suggested enum additions (include/types.h), names and gains (src/types.cpp):

  // enum class NoiseType : uint8_t { ... add after last existing ... }
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

  // getNoiseTypeName mappings (examples)
  "Isochronic", "Acoustic Beat", "Missing Fundamental", "Combination Tones",
  "Infrasound", "Somatic Bass", "Ear Resonance", "Near-Nyquist",
  "Feedback Howl", "FM Metallic", "Stutter/Glitch", "Phaser/Flanger",
  "Doppler", "Gated Reverb", "Aliasing Buzz"

  // getGainForType suggestions (balanced vs existing)
  0.65f, 0.65f, 0.60f, 0.60f, 0.55f, 0.70f, 0.55f, 0.50f,
  0.60f, 0.55f, 0.55f, 0.60f, 0.60f, 0.60f, 0.55f

  // src/audio_synthesis.cpp -> nextAudioSample:
  //   if (isExtraType(t)) { raw = nextAudioSampleExtra(t); break; }
*/
