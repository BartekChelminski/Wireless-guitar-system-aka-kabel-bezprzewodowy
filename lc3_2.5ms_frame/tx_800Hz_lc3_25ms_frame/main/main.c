#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_timer.h"
#include "lc3.h"

#define CHANNEL             11
#define SAMPLE_RATE         48000
#define LC3_FRAME_DT_US     7500
#define SAMPLES_PER_FRAME   360     // 7.5 ms × 48000 Hz
#define LC3_FRAME_BYTES     120     // 128 kbps, 7.5 ms: floor(128000×7500/8/1_000_000)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// !!! WPISZ TUTAJ ADRES MAC TWOJEGO ODBIORNIKA !!!
static uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = { 0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88 };

typedef struct __attribute__((packed)) {
    uint32_t seq_num;
    uint16_t crc;
    uint8_t  lc3_data[LC3_FRAME_BYTES];
} audio_packet_t;  // razem: 126 bajtów (lc3: 120B + nagłówek: 6B)

// Koder LC3 — statyczna alokacja, zero malloc
static lc3_encoder_mem_48k_t s_enc_mem;
static lc3_encoder_t         s_encoder;

RingbufHandle_t audio_ringbuf;
TaskHandle_t gen_task_handle = NULL;

// --- 1. METRONOM (Wyzwalacz) ---
void metronome_timer_callback(void *arg) {
    xTaskNotifyGive(gen_task_handle);
}

// --- 2. GENERATOR DŹWIĘKU (Matematyka) ---
void audio_gen_task(void *pvParameters) {
    float phase_inc = 2.0f * (float)M_PI * 800.0f / (float)SAMPLE_RATE;
    float current_phase = 0.0f;
    int16_t generated_audio[SAMPLES_PER_FRAME];

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            generated_audio[i] = (int16_t)(15000.0f * sinf(current_phase));
            current_phase += phase_inc;
            if (current_phase >= 2.0f * (float)M_PI) current_phase -= 2.0f * (float)M_PI;
        }
        xRingbufferSend(audio_ringbuf, generated_audio, sizeof(generated_audio), 0);
    }
}

// --- 3. KODER LC3 + WYSYŁKA (Sieć) ---
void audio_tx_task(void *pvParameters) {
    uint32_t current_seq = 0;
    audio_packet_t *tx_packet = malloc(sizeof(audio_packet_t));

    while (1) {
        size_t item_size;
        void *item = xRingbufferReceive(audio_ringbuf, &item_size, portMAX_DELAY);

        if (item != NULL && item_size == (SAMPLES_PER_FRAME * sizeof(int16_t))) {
            tx_packet->seq_num = current_seq++;

            // Enkodowanie PCM → LC3 (120 bajtów zamiast 720 bajtów raw)
            lc3_encode(s_encoder, LC3_PCM_FORMAT_S16,
                       item, 1, LC3_FRAME_BYTES, tx_packet->lc3_data);

            tx_packet->crc = 0;
            tx_packet->crc = esp_crc16_le(UINT16_MAX,
                                           (uint8_t const *)tx_packet,
                                           sizeof(audio_packet_t));

            esp_now_send(receiver_mac, (uint8_t *)tx_packet, sizeof(audio_packet_t));

            vRingbufferReturnItem(audio_ringbuf, item);
        }
    }
}

void wifi_config(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);

    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peerInfo = {0};
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = CHANNEL;
    peerInfo.ifidx   = WIFI_IF_STA;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    esp_now_rate_config_t rate_cfg = {
        .phymode = WIFI_PHY_MODE_11G,
        .rate    = WIFI_PHY_RATE_24M,
        .ersu    = false,
        .dcm     = false
    };
    esp_now_set_peer_rate_config(receiver_mac, &rate_cfg);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_config();

    // Inicjalizacja kodera LC3: 7.5 ms, 48 kHz, statyczna alokacja
    s_encoder = lc3_setup_encoder(LC3_FRAME_DT_US, SAMPLE_RATE, 0, &s_enc_mem);
    // Generator 800 Hz to sygnał syntetyczny — LTPF jest zoptymalizowany pod mowę,
    // dla fal sinusoidalnych warto go wyłączyć (oszczędność CPU, lepsza jakość)
    lc3_encoder_disable_ltpf(s_encoder);

    // Ring buffer: 4 ramki × 720 bajtów = 2880 bajtów (mały bufor po stronie TX — minimalna latencja)
    audio_ringbuf = xRingbufferCreate(sizeof(int16_t) * SAMPLES_PER_FRAME * 4, RINGBUF_TYPE_BYTEBUF);

    xTaskCreatePinnedToCore(audio_gen_task, "audio_gen", 4096, NULL, configMAX_PRIORITIES - 1, &gen_task_handle, 1);
    xTaskCreatePinnedToCore(audio_tx_task,  "audio_tx",  16384, NULL, configMAX_PRIORITIES - 2, NULL, 1);

    const esp_timer_create_args_t timer_args = {
        .callback = &metronome_timer_callback,
        .name     = "audio_metronome"
    };
    esp_timer_handle_t metronome_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &metronome_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(metronome_timer, LC3_FRAME_DT_US));
}
