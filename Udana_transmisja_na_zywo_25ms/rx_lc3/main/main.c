#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "lc3.h"

#define I2S_PORT_NUM    I2S_NUM_0
#define I2S_BCLK_PIN    4
#define I2S_LRCK_PIN    3
#define I2S_DOUT_PIN    2
#define I2C_SDA_PIN     5
#define I2C_SCL_PIN     6

#define CHANNEL             11
#define SAMPLE_RATE         48000
#define LC3_FRAME_DT_US     2500
#define SAMPLES_PER_FRAME   120     // 
#define LC3_FRAME_BYTES     40     //
#define ESPNOW_QUEUE_SIZE   20

// Ring buffer: 20 ramek × 360 próbek × 2 bajty = 14400 bajtów
#define RINGBUF_TOTAL_BYTES (sizeof(int16_t) * SAMPLES_PER_FRAME * 20)

// Liczniki diagnostyczne
volatile uint32_t monitor_overflow_cnt  = 0;
volatile uint32_t monitor_underflow_cnt = 0;

static const char *TAG = "Odbiornik";

extern void Setup_codec();

typedef struct __attribute__((packed)) {
    uint32_t seq_num;
    uint16_t crc;
    uint8_t  lc3_data[LC3_FRAME_BYTES];
} audio_packet_t;  // razem: 126 bajtów

typedef struct {
    uint8_t *data;
    int data_len;
} espnow_event_t;

QueueHandle_t     espnow_queue;
RingbufHandle_t   audio_ringbuf;
i2s_chan_handle_t tx_chan = NULL;

// Statyczna pula buforów — eliminuje malloc/free w callbacku ESP-NOW
static audio_packet_t s_packet_pool[ESPNOW_QUEUE_SIZE];
static bool           s_pool_used[ESPNOW_QUEUE_SIZE];

// Dekoder LC3 — statyczna alokacja, zero malloc
static lc3_decoder_mem_48k_t s_dec_mem;
static lc3_decoder_t         s_decoder;

// Roboczy bufor PCM — statyczny, nie zajmuje stosu taska (360×2 = 720 bajtów)
static int16_t s_pcm_buf[SAMPLES_PER_FRAME];

// Zmienne robocze audio_rx_task — wyciągnięte z pętli while(1)
static audio_packet_t *rx_packet;
static uint16_t        rx_received_crc;
static uint16_t        rx_calculated_crc;
static uint32_t        rx_seq;

// Zmienne robocze buffer_monitor_task
static size_t   mon_free_space;
static size_t   mon_used_space;
static uint32_t mon_fill_percent;

void setup_i2c(void) {
    i2c_config_t i2c_conf = {};
    i2c_conf.mode             = I2C_MODE_MASTER;
    i2c_conf.sda_io_num       = I2C_SDA_PIN;
    i2c_conf.scl_io_num       = I2C_SCL_PIN;
    i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = 400000;

    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0);
}

void setup_i2s(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 60;
    chan_cfg.auto_clear = true;

    i2s_new_channel(&chan_cfg, &tx_chan, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCLK_PIN,
            .ws   = (gpio_num_t)I2S_LRCK_PIN,
            .dout = (gpio_num_t)I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    i2s_channel_init_std_mode(tx_chan, &std_cfg);
    i2s_channel_enable(tx_chan);
}

// Znajdź wolny slot w puli — wołane tylko z callbacku (jeden wątek WiFi)
static audio_packet_t *pool_alloc(void) {
    for (int i = 0; i < ESPNOW_QUEUE_SIZE; i++) {
        if (!s_pool_used[i]) {
            s_pool_used[i] = true;
            return &s_packet_pool[i];
        }
    }
    return NULL;
}

static void pool_free(audio_packet_t *pkt) {
    int i = pkt - s_packet_pool;
    if (i >= 0 && i < ESPNOW_QUEUE_SIZE) {
        s_pool_used[i] = false;
    }
}

// --- 1. BRAMKARZ (Warstwa Sprzętowa) ---
IRAM_ATTR void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info,
                               const uint8_t *data, int data_len) {
    if (data_len == sizeof(audio_packet_t)) {
        audio_packet_t *buf = pool_alloc();
        if (buf == NULL) return;

        memcpy(buf, data, data_len);

        espnow_event_t evt = { .data = (uint8_t *)buf, .data_len = data_len };
        if (xQueueSend(espnow_queue, &evt, 0) != pdTRUE) {
            pool_free(buf);
        }
    }
}

// --- 2. DEKODER LC3 + ŁATACZ (Rdzeń 0) ---
void audio_rx_task(void *pvParameter) {
    espnow_event_t evt;
    uint32_t expected_seq = 0;
    bool first_packet = true;

    while (1) {
        if (xQueueReceive(espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
            rx_packet = (audio_packet_t *)evt.data;

            rx_received_crc   = rx_packet->crc;
            rx_packet->crc    = 0;
            rx_calculated_crc = esp_crc16_le(UINT16_MAX, evt.data, evt.data_len);

            if (rx_received_crc == rx_calculated_crc) {
                rx_seq = rx_packet->seq_num;

                if (first_packet) {
                    expected_seq = rx_seq;
                    first_packet = false;
                }

                // A) DUPLIKATY (seq < expected) — odrzucamy, dekoder jest stanowy
                if (rx_seq < expected_seq) {
                    pool_free(rx_packet);
                    continue;
                }

                // B) LUKI — wypełniamy przez wbudowany PLC dekodera LC3
                // Przy NULL/0 dekoder syntetyzuje zastępczy sygnał na podstawie
                // ostatnio zdekodowanych współczynników widmowych (plc.c)
                if (rx_seq > expected_seq) {
                    uint32_t lost = rx_seq - expected_seq;
                    for (uint32_t i = 0; i < lost; i++) {
                        lc3_decode(s_decoder, NULL, 0,
                                   LC3_PCM_FORMAT_S16, s_pcm_buf, 1);
                        xRingbufferSend(audio_ringbuf, s_pcm_buf, sizeof(s_pcm_buf), 0);
                    }
                    ESP_LOGI(TAG, "PLC: zalazano %lu ramek", lost);
                }

                // C) DEKODOWANIE AKTUALNEJ RAMKI
                lc3_decode(s_decoder, rx_packet->lc3_data, LC3_FRAME_BYTES,
                           LC3_PCM_FORMAT_S16, s_pcm_buf, 1);

                if (xRingbufferSend(audio_ringbuf, s_pcm_buf, sizeof(s_pcm_buf), 0) != pdTRUE) {
                    monitor_overflow_cnt++;
                }
                expected_seq = rx_seq + 1;
            }

            pool_free(rx_packet);
        }
    }
}

// --- 3. ODTWARZACZ SPRZĘTOWY (Rdzeń 1) ---
void audio_playback_task(void *pvParameters) {
    size_t item_size;
    size_t bytes_written;
    bool buffering = true;

    while (1) {
        // Pre-buffering: czekaj na 25% zapełnienia przed (re)startem odtwarzania.
        if (buffering) {
            if (xRingbufferGetCurFreeSize(audio_ringbuf) > (RINGBUF_TOTAL_BYTES * 0.1)) {
                vTaskDelay(1);
                continue;
            }
            buffering = false;
        }

        // Timeout: 2× czas ramki — jeśli przez 15 ms nic nie przyszło, to underflow
        void *audio_data = xRingbufferReceive(audio_ringbuf, &item_size, pdMS_TO_TICKS(15));

        if (audio_data != NULL) {
            i2s_channel_write(tx_chan, audio_data, item_size, &bytes_written, 1000);
            vRingbufferReturnItem(audio_ringbuf, audio_data);
        } else {
            monitor_underflow_cnt++;
            buffering = true;
        }
    }
}

void buffer_monitor_task(void *pvParameters) {
    while (1) {
        mon_free_space  = xRingbufferGetCurFreeSize(audio_ringbuf);
        mon_used_space  = RINGBUF_TOTAL_BYTES - mon_free_space;
        mon_fill_percent = (mon_used_space * 100) / RINGBUF_TOTAL_BYTES;

        ESP_LOGI("DIAGNOSTYKA", "Bufor: %lu%% | Zgubione(Ovf): %lu | Luki(Udf): %lu",
                 mon_fill_percent, monitor_overflow_cnt, monitor_underflow_cnt);

        monitor_overflow_cnt  = 0;
        monitor_underflow_cnt = 0;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_wifi(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    setup_wifi();
    setup_i2c();
    Setup_codec();
    setup_i2s();

    // Inicjalizacja dekodera LC3: 7.5 ms, 48 kHz, statyczna alokacja
    s_decoder = lc3_setup_decoder(LC3_FRAME_DT_US, SAMPLE_RATE, 0, &s_dec_mem);

    espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    audio_ringbuf = xRingbufferCreate(RINGBUF_TOTAL_BYTES, RINGBUF_TYPE_BYTEBUF);

    // audio_rx_task: 8192 bajtów stosu — LC3 decode potrzebuje ~4 KB + s_pcm_buf jest static
    xTaskCreatePinnedToCore(audio_rx_task,       "audio_rx",   8192, NULL, configMAX_PRIORITIES - 1, NULL, 0);
    xTaskCreatePinnedToCore(audio_playback_task, "audio_play", 4096, NULL, configMAX_PRIORITIES - 2, NULL, 1);
    xTaskCreatePinnedToCore(buffer_monitor_task, "monitor",    4096, NULL, 1,                        NULL, 0);
}
