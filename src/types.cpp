#include "types.h"
#include "config.h"

NoiseType getCurrentNoiseType(int trackIndex) {
  switch (trackIndex) {
    case 0:  return NoiseType::NOISE_WHITE;
    case 1:  return NoiseType::NOISE_PINK;
    case 2:  return NoiseType::NOISE_BROWN;
    case 3:  return NoiseType::NOISE_VIOLET;
    case 4:  return NoiseType::TONE_SINE;
    case 5:  return NoiseType::TONE_SQUARE;
    case 6:  return NoiseType::TONE_TRIANGLE;
    case 7:  return NoiseType::TONE_SAW;
    case 8:  return NoiseType::TONE_CHIRP;
    case 9:  return NoiseType::TONE_SHEPARD;
    case 10: return NoiseType::TONE_FM_BELL;
    case 11: return NoiseType::TONE_AM_TREMOLO;
    case 12: return NoiseType::TONE_SHEPARD_DOWN;
    case 13: return NoiseType::TONE_KARPLUS;
    case 14: return NoiseType::TONE_MODAL_DRUM;
    case 15: return NoiseType::TONE_GRANULAR;
    case 16: return NoiseType::TONE_SUPERSAW;
    case 17: return NoiseType::TONE_PWM;
    case 18: return NoiseType::FX_BITCRUSH;
    case 19: return NoiseType::TONE_PHASE_DIST;
    case 20: return NoiseType::TONE_WAVEFOLD;
    case 21: return NoiseType::NOISE_BANDPASS;
    case 22: return NoiseType::RHYTHM_EUCLIDEAN;
    case 23: return NoiseType::RHYTHM_EUCLIDEAN_7_16;
    case 24: return NoiseType::RHYTHM_POLY_3_4;
    case 25: return NoiseType::TONE_RING_MOD;
    case 26: return NoiseType::TONE_CHORUS;
    case 27: return NoiseType::FX_SAMPLE_HOLD;
    case 28: return NoiseType::FX_FORMANT;
    case 29: return NoiseType::TONE_SYNC;
    case 30: return NoiseType::TONE_SUPER_SQUARE;
    case 31: return NoiseType::TONE_ISOCHRONIC;
    case 32: return NoiseType::TONE_ACOUSTIC_BEAT;
    case 33: return NoiseType::TONE_MISSING_FUND;
    case 34: return NoiseType::TONE_COMBINATION_TONES;
    case 35: return NoiseType::TONE_INFRASOUND;
    case 36: return NoiseType::TONE_SOMATIC_BASS;
    case 37: return NoiseType::TONE_EAR_RESONANCE;
    case 38: return NoiseType::TONE_NEAR_NYQUIST;
    case 39: return NoiseType::TONE_FEEDBACK_HOWL;
    case 40: return NoiseType::TONE_FM_METAL;
    case 41: return NoiseType::FX_STUTTER;
    case 42: return NoiseType::FX_PHASER;
    case 43: return NoiseType::FX_DOPPLER;
    case 44: return NoiseType::FX_GATED_REVERB;
    case 45: return NoiseType::FX_ALIASING_BUZZ;
    default: return NoiseType::NOISE_WHITE;
  }
}

const char* getNoiseTypeName(NoiseType t) {
  switch (t) {
    case NoiseType::NOISE_WHITE:       return "White";
    case NoiseType::NOISE_PINK:        return "Pink";
    case NoiseType::NOISE_BROWN:       return "Brown";
    case NoiseType::NOISE_BLUE:        return "Blue";
    case NoiseType::NOISE_VIOLET:      return "Violet";
    case NoiseType::TONE_SINE:         return "Sine 440";
    case NoiseType::TONE_SQUARE:       return "Square 440";
    case NoiseType::TONE_TRIANGLE:     return "Triangle 440";
    case NoiseType::TONE_SAW:          return "Saw 220";
    case NoiseType::TONE_CHIRP:        return "Chirp 200-1200";
    case NoiseType::TONE_SHEPARD:      return "Shepard Up";
    case NoiseType::TONE_FM_BELL:      return "FM Bell";
    case NoiseType::TONE_AM_TREMOLO:   return "AM Tremolo";
    case NoiseType::TONE_SHEPARD_DOWN: return "Shepard Down";
    case NoiseType::TONE_KARPLUS:      return "Karplus (Pluck)";
    case NoiseType::TONE_MODAL_DRUM:   return "Modal Drum";
    case NoiseType::TONE_GRANULAR:     return "Granular";
    case NoiseType::TONE_SUPERSAW:     return "SuperSaw";
    case NoiseType::TONE_PWM:          return "PWM";
    case NoiseType::FX_BITCRUSH:       return "Bitcrush";
    case NoiseType::TONE_PHASE_DIST:   return "PhaseDist";
    case NoiseType::TONE_WAVEFOLD:     return "Wavefold";
    case NoiseType::NOISE_BANDPASS:    return "Bandpass Noise";
    case NoiseType::RHYTHM_EUCLIDEAN:  return "Euclid Rhythm";
    case NoiseType::RHYTHM_EUCLIDEAN_7_16: return "Euclid 7/16";
    case NoiseType::RHYTHM_POLY_3_4:       return "Poly 3:4";
    case NoiseType::TONE_RING_MOD:         return "Ring Mod";
    case NoiseType::TONE_CHORUS:           return "Chorus Sines";
    case NoiseType::FX_SAMPLE_HOLD:        return "Sample & Hold";
    case NoiseType::FX_FORMANT:            return "Formant Noise";
    case NoiseType::TONE_SYNC:             return "Sync Lead";
    case NoiseType::TONE_SUPER_SQUARE:     return "SuperSquare";
    case NoiseType::TONE_ISOCHRONIC:       return "Isochronic";
    case NoiseType::TONE_ACOUSTIC_BEAT:    return "Acoustic Beat";
    case NoiseType::TONE_MISSING_FUND:     return "Missing Fundamental";
    case NoiseType::TONE_COMBINATION_TONES:return "Combination Tones";
    case NoiseType::TONE_INFRASOUND:       return "Infrasound";
    case NoiseType::TONE_SOMATIC_BASS:     return "Somatic Bass";
    case NoiseType::TONE_EAR_RESONANCE:    return "Ear Resonance";
    case NoiseType::TONE_NEAR_NYQUIST:     return "Near-Nyquist";
    case NoiseType::TONE_FEEDBACK_HOWL:    return "Feedback Howl";
    case NoiseType::TONE_FM_METAL:         return "FM Metallic";
    case NoiseType::FX_STUTTER:            return "Stutter/Glitch";
    case NoiseType::FX_PHASER:             return "Phaser/Flanger";
    case NoiseType::FX_DOPPLER:            return "Doppler";
    case NoiseType::FX_GATED_REVERB:       return "Gated Reverb";
    case NoiseType::FX_ALIASING_BUZZ:      return "Aliasing Buzz";
    default: return "Unknown";
  }
}

float getGainForType(NoiseType t) {
  switch (t) {
    case NoiseType::NOISE_WHITE:       return 0.55f;
    case NoiseType::NOISE_PINK:        return 0.75f;
    case NoiseType::NOISE_BROWN:       return 0.85f;
    case NoiseType::NOISE_BLUE:        return 0.60f;
    case NoiseType::NOISE_VIOLET:      return 0.55f;
    case NoiseType::TONE_SINE:         return 0.70f;
    case NoiseType::TONE_SQUARE:       return 0.50f;
    case NoiseType::TONE_TRIANGLE:     return 0.70f;
    case NoiseType::TONE_SAW:          return 0.60f;
    case NoiseType::TONE_CHIRP:        return 0.65f;
    case NoiseType::TONE_SHEPARD:      return 0.70f;
    case NoiseType::TONE_FM_BELL:      return 0.60f;
    case NoiseType::TONE_AM_TREMOLO:   return 0.70f;
    case NoiseType::TONE_SHEPARD_DOWN: return 0.70f;
    case NoiseType::TONE_KARPLUS:      return 0.70f;
    case NoiseType::TONE_MODAL_DRUM:   return 0.75f;
    case NoiseType::TONE_GRANULAR:     return 0.65f;
    case NoiseType::TONE_SUPERSAW:     return 0.55f;
    case NoiseType::TONE_PWM:          return 0.60f;
    case NoiseType::FX_BITCRUSH:       return 0.55f;
    case NoiseType::TONE_PHASE_DIST:   return 0.60f;
    case NoiseType::TONE_WAVEFOLD:     return 0.60f;
    case NoiseType::NOISE_BANDPASS:    return 0.65f;
    case NoiseType::RHYTHM_EUCLIDEAN:  return 0.60f;
    case NoiseType::RHYTHM_EUCLIDEAN_7_16: return 0.60f;
    case NoiseType::RHYTHM_POLY_3_4:       return 0.60f;
    case NoiseType::TONE_RING_MOD:         return 0.60f;
    case NoiseType::TONE_CHORUS:           return 0.55f;
    case NoiseType::FX_SAMPLE_HOLD:        return 0.55f;
    case NoiseType::FX_FORMANT:            return 0.60f;
    case NoiseType::TONE_SYNC:             return 0.60f;
    case NoiseType::TONE_SUPER_SQUARE:     return 0.55f;
    case NoiseType::TONE_ISOCHRONIC:       return 0.65f;
    case NoiseType::TONE_ACOUSTIC_BEAT:    return 0.65f;
    case NoiseType::TONE_MISSING_FUND:     return 0.60f;
    case NoiseType::TONE_COMBINATION_TONES:return 0.60f;
    case NoiseType::TONE_INFRASOUND:       return 0.55f;
    case NoiseType::TONE_SOMATIC_BASS:     return 0.70f;
    case NoiseType::TONE_EAR_RESONANCE:    return 0.55f;
    case NoiseType::TONE_NEAR_NYQUIST:     return 0.50f;
    case NoiseType::TONE_FEEDBACK_HOWL:    return 0.60f;
    case NoiseType::TONE_FM_METAL:         return 0.55f;
    case NoiseType::FX_STUTTER:            return 0.55f;
    case NoiseType::FX_PHASER:             return 0.60f;
    case NoiseType::FX_DOPPLER:            return 0.60f;
    case NoiseType::FX_GATED_REVERB:       return 0.60f;
    case NoiseType::FX_ALIASING_BUZZ:      return 0.55f;
    default: return 0.65f;
  }
}
