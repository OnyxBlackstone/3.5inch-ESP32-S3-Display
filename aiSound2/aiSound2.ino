/*
 * aiSound.ino — SD WAV playback on LCDWIKI 3.5" IPS ESP32-S3 (ES3C35P)
 *
 * Path: microSD (SDIO) -> ESP32-S3 -> I2S -> ES8311 codec -> FM8002E -> speaker
 *
 * Board: ESP32S3 Dev Module
 *   Tools -> PSRAM           : OPI PSRAM
 *   Tools -> Flash Size      : 16MB (128Mb)
 *   Tools -> Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
 *
 * Library: ESP32-audioI2S (schreibfaul1), from Library Manager.
 *
 * Sketch folder must contain these three files next to this .ino:
 *   aiSound.ino
 *   es8311.h    <- Arduino class version from the library's examples/ES8311/
 *   es8311.cpp  <- same folder
 *
 * The correct es8311.h declares "class ES8311" and uses Wire.
 * If it contains "#include audio_hal.h" or "es8311_create", it is the
 * Espressif IDF version and is the WRONG file.
 *
 * Card: FAT32 / MBR.  File: /1.wav — PCM, 16-bit, mono,
 *       sample rate must match SAMPLE_RATE below.
 */

#include "Arduino.h"
#include "Audio.h"
#include "SD_MMC.h"
#include "Wire.h"
#include "es8311.h"

// ---- I2S / codec pins (spec section 4.2) ----
#define I2S_DOUT      15     // I2S bit output data
#define I2S_BCLK      18     // I2S bit clock
#define I2S_LRC       21     // I2S left/right channel select
#define I2S_MCLK      17     // I2S master clock
#define I2C_SDA       38     // shared bus: touch panel + ES8311
#define I2C_SCL       39
#define PA_ENABLE      1     // FM8002E amp enable

// ---- SD card pins (SDIO) ----
#define SD_CLK         5
#define SD_CMD         4
#define SD_D0          6
#define SD_D1          7
#define SD_D2          2
#define SD_D3          3

// ---- Playback ----
#define AUDIO_FILE   "/sausage.mp3"
#define SAMPLE_RATE  44100   // must match the WAV file
#define CODEC_VOLUME 82      // 0..100, codec side
#define AMP_VOLUME   21      // 0..21, library side

Audio  audio;
ES8311 es;

void my_audio_info(Audio::msg_t m) {
  Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
  // Must be registered before anything else the library does.
  Audio::audio_info_callback = my_audio_info;

  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- aiSound boot ---");
  Serial.printf("Chip: %s | Arduino %d.%d.%d | IDF %d.%d.%d\n",
                ESP.getChipModel(),
                ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH,
                ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR,
                ESP_IDF_VERSION_PATCH);

  // --- Amplifier: muted while everything comes up ---
  pinMode(PA_ENABLE, OUTPUT);
  digitalWrite(PA_ENABLE, HIGH);

  // --- SD card over SDIO ---
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  if (!SD_MMC.begin("/sdcard", false, false, 20000)) {
    Serial.println("SD mount FAILED - check FAT32/MBR and seating");
  } else {
    Serial.println("SD mounted OK");
    File root = SD_MMC.open("/");
    if (root) {
      while (File f = root.openNextFile()) {
        Serial.printf("  card: %s (%u bytes)\n", f.name(), (unsigned)f.size());
      }
      root.close();
    }
    Serial.printf("1.wav present: %s\n", SD_MMC.exists(AUDIO_FILE) ? "yes" : "NO");
  }

  // --- I2S ---
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(AMP_VOLUME);

  // --- Codec: this is the step that was missing ---
  if (!es.begin(I2C_SDA, I2C_SCL, 400000)) {
    Serial.println("ES8311 begin FAILED - check IO38/IO39, and that nothing else holds the bus");
  } else {
    Serial.println("ES8311 ready");
  }
  es.setVolume(CODEC_VOLUME);
  es.setBitsPerSample(16);
  es.setSampleRate(SAMPLE_RATE);

  // --- Unmute the amplifier and play ---
  digitalWrite(PA_ENABLE, LOW);      // low-enable per the spec
  Serial.println("amp unmuted (IO1 = LOW)");

  if (!audio.connecttoFS(SD_MMC, AUDIO_FILE)) {
    Serial.println("connecttoFS FAILED");
  }
}

void loop() {
  audio.loop();
  vTaskDelay(1);

  // Replay so the tone repeats instead of stopping at EOF.
  // Delete this block for single-shot playback.
  if (!audio.isRunning()) {
    delay(1000);
    audio.connecttoFS(SD_MMC, AUDIO_FILE);
  }
}
