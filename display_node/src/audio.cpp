#include "audio.h"
#include <driver/i2s.h>
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ── Pin config ────────────────────────────────────────────────────────────────
#define I2S_PORT    I2S_NUM_0
#define I2S_BCLK    42
#define I2S_LRCLK   18
#define I2S_DOUT    17

#define SD_CS    10
#define SD_MOSI  11
#define SD_SCK   12
#define SD_MISO  13

// ── Tuning ────────────────────────────────────────────────────────────────────
#define ALARM_FILE       "/alarm.wav"
#define REPEAT_DELAY_MS  3000
#define CHUNK_BYTES      2048
#define MAX_WAV_BYTES    (3 * 1024 * 1024)   // 3 MB ceiling (~17 s at CD quality)

// ── Shared state (written from Core 1, read from audio task on Core 0) ────────
static uint8_t          *psram_buf    = nullptr; // entire WAV file
static size_t            psram_len    = 0;
static size_t            pcm_offset   = 0;       // byte offset to raw PCM inside buf

static volatile bool     alarm_active = false;
static volatile bool     i2s_ready    = false;
static TaskHandle_t      audio_task_h = nullptr;

// ── WAV chunk scanner ─────────────────────────────────────────────────────────
// Searches for fmt  and data chunks regardless of chunk ordering or padding.
// On success: configures I2S, sets pcm_offset, returns true.
static bool scan_wav_and_install_i2s(void)
{
    if (!psram_buf || psram_len < 12) return false;
    if (memcmp(psram_buf,     "RIFF", 4) ||
        memcmp(psram_buf + 8, "WAVE", 4)) {
        Serial.println("[audio] Not a RIFF/WAVE file");
        return false;
    }

    uint32_t sample_rate = 44100;
    uint16_t channels    = 2;
    uint16_t bits        = 16;
    bool     found_fmt   = false;
    bool     found_data  = false;

    size_t pos = 12;
    while (pos + 8 <= psram_len) {
        const char    *id   = (const char *)(psram_buf + pos);
        const uint32_t csz  = *(uint32_t  *)(psram_buf + pos + 4);
        pos += 8;

        if (memcmp(id, "fmt ", 4) == 0 && csz >= 16) {
            channels    = *(uint16_t *)(psram_buf + pos + 2);
            sample_rate = *(uint32_t *)(psram_buf + pos + 4);
            bits        = *(uint16_t *)(psram_buf + pos + 14);
            found_fmt   = true;
        } else if (memcmp(id, "data", 4) == 0) {
            pcm_offset = pos;      // file position now points at raw PCM
            found_data = true;
            break;
        }
        pos += csz + (csz & 1);   // word-align to next chunk
    }

    if (!found_fmt || !found_data) {
        Serial.println("[audio] Could not find fmt/data chunks");
        return false;
    }

    Serial.printf("[audio] WAV: %lu Hz, %u ch, %u-bit, PCM at byte %u\n",
                  sample_rate, channels, bits, (unsigned)pcm_offset);

    // Install I2S driver — called from Core 0 so GDMA interrupt is pinned
    // to Core 0 and never fires on Core 1 (where the display DMA runs).
    i2s_config_t cfg     = {};
    cfg.mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate      = sample_rate;
    cfg.bits_per_sample  = (i2s_bits_per_sample_t)bits;
    cfg.channel_format   = (channels == 1) ? I2S_CHANNEL_FMT_ONLY_LEFT
                                           : I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count    = 8;
    cfg.dma_buf_len      = 1024;
    cfg.tx_desc_auto_clear = true;
    cfg.use_apll         = false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = I2S_BCLK;
    pins.ws_io_num    = I2S_LRCLK;
    pins.data_out_num = I2S_DOUT;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[audio] I2S driver install failed");
        return false;
    }
    i2s_set_pin(I2S_PORT, &pins);
    i2s_stop(I2S_PORT);
    return true;
}

// ── Audio task (Core 0) ───────────────────────────────────────────────────────
static void audio_task(void *arg)
{
    // Install I2S from Core 0 so its GDMA interrupt never runs on Core 1.
    if (!scan_wav_and_install_i2s()) {
        Serial.println("[audio] task: I2S setup failed, task exiting");
        i2s_ready = true;           // unblock audio_init() even on failure
        vTaskDelete(nullptr);
        return;
    }
    i2s_ready = true;

    const uint8_t *pcm     = psram_buf + pcm_offset;
    const size_t   pcm_len = psram_len - pcm_offset;

    while (true) {
        if (!alarm_active) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // ── Play one pass through the WAV ──────────────────────────────────
        size_t pos = 0;
        i2s_start(I2S_PORT);

        while (pos < pcm_len && alarm_active) {
            size_t  chunk   = min(pcm_len - pos, (size_t)CHUNK_BYTES);
            size_t  written = 0;
            i2s_write(I2S_PORT, pcm + pos, chunk, &written, pdMS_TO_TICKS(50));
            pos += written;
        }

        i2s_stop(I2S_PORT);
        i2s_zero_dma_buffer(I2S_PORT);

        if (!alarm_active) continue;    // stopped mid-play; re-check flag

        // ── Natural end: wait 3 s then replay ─────────────────────────────
        uint32_t until = millis() + REPEAT_DELAY_MS;
        while (millis() < until && alarm_active) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void audio_init(void)
{
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("[audio] SD mount failed — check wiring/card");
        return;
    }

    File f = SD.open(ALARM_FILE);
    if (!f) {
        Serial.println("[audio] " ALARM_FILE " not found on SD card");
        return;
    }

    psram_len = min((size_t)f.size(), (size_t)MAX_WAV_BYTES);
    psram_buf = (uint8_t *)ps_malloc(psram_len);
    if (!psram_buf) {
        Serial.println("[audio] PSRAM alloc failed — file too large?");
        f.close();
        return;
    }

    size_t got = f.read(psram_buf, psram_len);
    f.close();
    Serial.printf("[audio] Loaded %u / %u bytes from SD\n",
                  (unsigned)got, (unsigned)f.size());

    // Create the audio task on Core 0. It installs the I2S driver from there,
    // which pins the GDMA interrupt to Core 0 (away from the display on Core 1).
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, nullptr, 2,
                            &audio_task_h, /*core=*/0);

    // Block until I2S is ready before the display initialises.
    while (!i2s_ready) vTaskDelay(pdMS_TO_TICKS(10));
    Serial.println("[audio] ready");
}

void audio_start_alarm(void)
{
    if (!i2s_ready || !psram_buf || !pcm_offset) return;
    alarm_active = true;
}

void audio_stop_alarm(void)
{
    alarm_active = false;
    // The audio task stops at the next chunk boundary (~11 ms at CD quality).
    // i2s_stop() is called by the task itself after the write loop exits.
}

void audio_tick(void)
{
    // No-op: audio task handles everything on Core 0.
}
