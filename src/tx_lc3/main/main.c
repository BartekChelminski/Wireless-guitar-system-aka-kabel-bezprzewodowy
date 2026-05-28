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
#include "esp_timer.h"

// Piny I2S
#define I2S_BCLK_PIN    4   // D3
#define I2S_LRCK_PIN    3   // D2
#define I2S_DIN_PIN     2   // D1 (TX_SDA kodeka)

// Piny I2C
#define I2C_SDA_PIN     5   // D4
#define I2C_SCL_PIN     6   // D5

#define CHANNEL             11
#define SAMPLE_RATE         48000
#define LC3_FRAME_DT_US     2500
#define SAMPLES_PER_FRAME   120     // 2.5 ms × 48000 Hz
#define LC3_FRAME_BYTES     40      // 128 kbps, 2.5 ms

// Ring buffer: 24 ramki × 120 próbek × 2 bajty = 5760 bajtów
#define RINGBUF_FRAMES      128
#define RINGBUF_TOTAL_BYTES (sizeof(int16_t) * SAMPLES_PER_FRAME * RINGBUF_FRAMES)

// Liczniki diagnostyczne do monitorowania procentu zajęcia bufora
volatile uint32_t monitor_overflow_cnt  = 0;
volatile uint32_t monitor_underflow_cnt = 0;

static const char *TAG = "Nadajnik";

extern void Setup_codec();

//ADRES MAC ODBIORNIKA
static uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = { 0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88 };

typedef struct __attribute__((packed)) {
    uint32_t seq_num;
    uint16_t crc;
    uint8_t  lc3_data[LC3_FRAME_BYTES];
} audio_packet_t;  // razem: 126 bajtów

// Koder LC3
static lc3_encoder_mem_48k_t s_enc_mem;
static lc3_encoder_t         s_encoder;

RingbufHandle_t   audio_ringbuf;
QueueHandle_t     encoded_queue;
i2s_chan_handle_t  rx_chan = NULL;

// Bufor stereo I2S: 360 ramek × 2 kanały = 720 próbek
static int16_t s_stereo_buf[SAMPLES_PER_FRAME * 2];
// Bufor mono (prawy kanał)
static int16_t s_mono_buf[SAMPLES_PER_FRAME];

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
    
    i2s_new_channel(&chan_cfg, NULL, &rx_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCLK_PIN,
            .ws   = (gpio_num_t)I2S_LRCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    i2s_channel_init_std_mode(rx_chan, &std_cfg);
    i2s_channel_enable(rx_chan);
}

// PRZECHWYTYWANIE AUDIO Z I2S (Rdzeń 1)
// Odczytuje ramkę stereo, wyodrębnia prawy kanał, wrzuca mono do ring buffera
void audio_capture_task(void *pvParameters) {
    size_t bytes_read;
    const size_t stereo_frame_bytes = sizeof(s_stereo_buf);

    while (1) {
        esp_err_t ret = i2s_channel_read(rx_chan, s_stereo_buf, stereo_frame_bytes,
                                         &bytes_read, portMAX_DELAY);

        if (ret == ESP_OK && bytes_read == stereo_frame_bytes) {
            // Ekstrakcja prawego kanału: [0]=L, [1]=R, [2]=L, [3]=R, ...
            for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
                s_mono_buf[i] = s_stereo_buf[i * 2 + 1];
            }

            if (xRingbufferSend(audio_ringbuf, s_mono_buf, sizeof(s_mono_buf), 0) != pdTRUE) {
                monitor_overflow_cnt++;
            }
        }
    }
}

// KODER LC3 (Rdzeń 0)
void audio_encode_task(void *pvParameters) {
    uint32_t current_seq = 0;
    audio_packet_t tx_packet;

     
      		while (1) {
           	size_t item_size;
         	void *item = xRingbufferReceiveUpTo(audio_ringbuf, &item_size, portMAX_DELAY,
                                                  SAMPLES_PER_FRAME * sizeof(int16_t));
 
           	if (item != NULL && item_size == (SAMPLES_PER_FRAME * sizeof(int16_t))) {
            tx_packet.seq_num = current_seq++;

            //int64_t t0 = esp_timer_get_time();
            lc3_encode(s_encoder, LC3_PCM_FORMAT_S16,
                               item, 1, LC3_FRAME_BYTES, tx_packet.lc3_data);
            //int64_t dt = esp_timer_get_time() - t0;
            
            //ESP_LOGI(TAG, "encode: %lld us (limit %d)", dt, LC3_FRAME_DT_US);
      
            vRingbufferReturnItem(audio_ringbuf, item);

            tx_packet.crc = 0;
            tx_packet.crc = esp_crc16_le(UINT16_MAX,
                                          (uint8_t const *)&tx_packet,
                                          sizeof(audio_packet_t));

            xQueueSend(encoded_queue, &tx_packet, 0);
        }
    }
}

// WYSYŁKA ESP-NOW (Rdzeń 1)
void audio_send_task(void *pvParameters) {
    audio_packet_t tx_packet;

    while (1) {
        if (xQueueReceive(encoded_queue, &tx_packet, portMAX_DELAY) == pdTRUE) {
            esp_now_send(receiver_mac, (uint8_t *)&tx_packet, sizeof(audio_packet_t));
        }
    }
}

// DIAGNOSTYKA
void buffer_monitor_task(void *pvParameters) {
    size_t   free_space, used_space;
    uint32_t fill_percent;

    while (1) {
        free_space   = xRingbufferGetCurFreeSize(audio_ringbuf);
        used_space   = RINGBUF_TOTAL_BYTES - free_space;
        fill_percent = (used_space * 100) / RINGBUF_TOTAL_BYTES;

        ESP_LOGI("DIAGNOSTYKA", "Bufor: %lu%% | Overflow: %lu | Underflow: %lu",
                 fill_percent, monitor_overflow_cnt, monitor_underflow_cnt);

        monitor_overflow_cnt  = 0;
        monitor_underflow_cnt = 0;

        vTaskDelay(pdMS_TO_TICKS(1000));
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
        .rate    = WIFI_PHY_RATE_36M,
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
    setup_i2c();
    Setup_codec();
    setup_i2s();

    s_encoder = lc3_setup_encoder(LC3_FRAME_DT_US, SAMPLE_RATE, 0, &s_enc_mem);
    // wyłączenie filtrowania LPTF
    lc3_encoder_disable_ltpf(s_encoder);

    audio_ringbuf = xRingbufferCreate(RINGBUF_TOTAL_BYTES, RINGBUF_TYPE_BYTEBUF);

    // Kolejka zakodowanych pakietów
    encoded_queue = xQueueCreate(8, sizeof(audio_packet_t));

    
    // Rdzeń 1: I2S capture + LC3 encode (capture czeka na DMA, encode używa CPU między odczytami)
    // Rdzeń 0: ESP-NOW send (stos WiFi również jest na rdzeniu 0)
    xTaskCreatePinnedToCore(audio_capture_task, "audio_cap",  4096,  NULL, configMAX_PRIORITIES - 2, NULL, 1);
    xTaskCreatePinnedToCore(audio_encode_task,  "audio_enc",  16384, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(audio_send_task,    "audio_send", 4096,  NULL, configMAX_PRIORITIES - 1, NULL, 0);

    // tylko do diagnostyki
    //xTaskCreatePinnedToCore(buffer_monitor_task,"monitor",    4096,  NULL, 1,                        NULL, 0);
}
