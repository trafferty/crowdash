#pragma once

// Audio engine: SD/TF card WAV playback via I2S to on-board speaker amplifier.
// Uses the ESP-IDF legacy I2S driver (compatible with arduino-esp32 2.0.3).
//
// I2S pins (confirmed from board silkscreen near J5 SPK):
//   IO42 = I2S-BCLK
//   IO18 = I2S-LRCLK
//   IO17 = I2S-SDIN  (data out of ESP32, into amplifier)
//
// SD/TF card SPI pins (confirmed from board spec):
//   IO10 = CS   IO11 = MOSI   IO12 = CLK   IO13 = MISO
//
// Place /alarm.wav at the root of the SD card.
// Recommended format: 16-bit PCM, mono or stereo, any common sample rate.

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);         // call once in setup(), before ui_app_init()
void audio_start_alarm(void);  // begin repeating playback
void audio_stop_alarm(void);   // stop playback (call on Reset)
void audio_tick(void);         // call every loop() iteration

#ifdef __cplusplus
}
#endif
