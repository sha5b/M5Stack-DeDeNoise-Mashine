#include <Arduino.h>
#include <M5Stack.h>
#include "config.h"
#include "types.h"
#include "audio_synthesis.h"
#include "visual_rendering.h"
#include "audio_extras.h"
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Application state
int currentTrack = 0;
bool isPlaying = false;
bool needsRedraw = true;

/* Shuffle mode state */
bool shuffleMode = false;
bool bLongHoldHandled = false;
uint32_t lastShuffleChangeMs = 0;

/* Master volume hold state */
bool volHoldA = false;  // holding LEFT (A) => volume down
bool volHoldC = false;  // holding RIGHT (C) => volume up
uint32_t lastVolStepMs = 0;

// Frame timing
uint32_t lastFrameMs = 0;

// UI Functions
void render() {
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  // Header
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(6, 6);
  NoiseType currentType = getCurrentNoiseType(currentTrack);
  int volPct = (int)(getMasterGain() * 100.0f + 0.5f);
  M5.Lcd.printf("Noise: %s   Shuffle: %s   Vol: %d%%", 
    getNoiseTypeName(currentType), 
    shuffleMode ? "On" : "Off",
    volPct);

  // Outline of the "image" area
  M5.Lcd.drawRect(NOISE_X - 1, NOISE_Y - 1, NOISE_W + 2, NOISE_H + 2, TFT_WHITE);

  // Footer with button hints
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, M5.Lcd.height() - 18);
  M5.Lcd.print("A=Prev (noise)   B=Play/Pause   C=Next (noise)");

  needsRedraw = false;
}

void nextTrack() {
  currentTrack = (currentTrack + 1) % TRACK_COUNT;
  setAudioNoiseType(getCurrentNoiseType(currentTrack));
  randomizeGraphColor();
  needsRedraw = true;
}

void prevTrack() {
  currentTrack = (currentTrack - 1 + TRACK_COUNT) % TRACK_COUNT;
  setAudioNoiseType(getCurrentNoiseType(currentTrack));
  randomizeGraphColor();
  needsRedraw = true;
}

void togglePlay() {
  isPlaying = !isPlaying;
  setAudioRunning(isPlaying);
  
  if (isPlaying) {
    // Enable DAC and prime at mid-level to avoid pop/hum
    pinMode(AUDIO_DAC_PIN, OUTPUT);
    dac_output_enable(DAC_CHANNEL_1);
    for (int i = 0; i < 200; ++i) {
      dacWrite(AUDIO_DAC_PIN, 128);
      delayMicroseconds(100);
    }
  } else {
    // Settle output to mid-level before disabling to avoid pop/hum
    for (int i = 0; i < 400; ++i) {
      dacWrite(AUDIO_DAC_PIN, 128);
      delayMicroseconds(100);
    }
    dac_output_disable(DAC_CHANNEL_1);
    pinMode(AUDIO_DAC_PIN, INPUT); // high-Z when not playing
  }
  needsRedraw = true;
}

// Arduino setup
void setup() {
  M5.begin(true, false, true, true); // LCD on, SD off, Serial on, I2C on
  M5.Power.begin();

  Serial.begin(115200);
  Serial.println("M5Stack Noise Player starting...");
  randomSeed((uint32_t)micros());

  // LCD setup
  M5.Lcd.setRotation(1);
  M5.Lcd.setBrightness(120);

  // Initialize state
  initAudioState();
  initVisualState();
  initAudioExtras();

  // DAC idle: disable to avoid startup hum
  dac_output_disable(DAC_CHANNEL_1);
  pinMode(AUDIO_DAC_PIN, INPUT); // high-Z when not playing

  // Start audio task on core 1
  xTaskCreatePinnedToCore(audioTask, "audioTask", 4096, nullptr, 1, nullptr, 1);

  // Set initial noise type
  setAudioNoiseType(getCurrentNoiseType(currentTrack));
  randomizeGraphColor();
  lastFrameMs = millis();
  needsRedraw = true;
}

// Arduino loop
void loop() {
  M5.update();

  // Button A: hold to decrease volume, short release = previous track
  if (M5.BtnA.pressedFor(300)) {
    volHoldA = true;
    uint32_t nowMs = millis();
    if (nowMs - lastVolStepMs >= 120) {
      float g = getMasterGain() - 0.02f;
      if (g < 0.0f) g = 0.0f;
      setMasterGain(g);
      lastVolStepMs = nowMs;
      needsRedraw = true;
    }
  }
  if (M5.BtnA.wasReleased()) {
    if (!volHoldA) {
      prevTrack();
      Serial.printf("Prev -> Track %d (%s)\n", currentTrack + 1, 
        getNoiseTypeName(getCurrentNoiseType(currentTrack)));
    }
    volHoldA = false;
  }

  // Button B: short press = Play/Pause, long-press (2s) = toggle Shuffle
  if (M5.BtnB.pressedFor(2000) && !bLongHoldHandled) {
    shuffleMode = !shuffleMode;
    bLongHoldHandled = true;
    lastShuffleChangeMs = millis();
    needsRedraw = true;
    Serial.printf("Shuffle %s\n", shuffleMode ? "ON" : "OFF");
  }
  if (M5.BtnB.wasReleased()) {
    if (!bLongHoldHandled) {
      togglePlay();
      Serial.printf("Toggle -> %s\n", isPlaying ? "Play" : "Pause");
    }
    bLongHoldHandled = false;
  }

  // Button C: hold to increase volume, short release = next track
  if (M5.BtnC.pressedFor(300)) {
    volHoldC = true;
    uint32_t nowMs = millis();
    if (nowMs - lastVolStepMs >= 120) {
      float g = getMasterGain() + 0.02f;
      if (g > 1.0f) g = 1.0f;
      setMasterGain(g);
      lastVolStepMs = nowMs;
      needsRedraw = true;
    }
  }
  if (M5.BtnC.wasReleased()) {
    if (!volHoldC) {
      nextTrack();
      Serial.printf("Next -> Track %d (%s)\n", currentTrack + 1, 
        getNoiseTypeName(getCurrentNoiseType(currentTrack)));
    }
    volHoldC = false;
  }

  // Redraw static UI if needed
  if (needsRedraw) {
    render();
  }

  // Animate noise frames
  uint32_t now = millis();
  if (isPlaying) {
    if (now - lastFrameMs >= FRAME_INTERVAL_MS) {
      drawNoiseFrame(getCurrentNoiseType(currentTrack));
      lastFrameMs = now;
    }
  } else {
    // When paused, show a single representative frame
    static int lastTrackShown = -1;
    if (lastTrackShown != currentTrack || needsRedraw) {
      drawNoiseFrame(getCurrentNoiseType(currentTrack));
      lastTrackShown = currentTrack;
      needsRedraw = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Shuffle mode timer
  if (shuffleMode) {
    if (now - lastShuffleChangeMs >= SHUFFLE_INTERVAL_MS) {
      int prev = currentTrack;
      int next = random(0, TRACK_COUNT);
      if (next == prev) next = (next + 1) % TRACK_COUNT;
      currentTrack = next;
      setAudioNoiseType(getCurrentNoiseType(currentTrack));
      randomizeGraphColor();
      needsRedraw = true;
      lastShuffleChangeMs = now;
      Serial.printf("Shuffle -> Track %d (%s)\n", currentTrack + 1, 
        getNoiseTypeName(getCurrentNoiseType(currentTrack)));
    }
  }

  // Small yield
  delay(1);
}
