#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "lc3.h"

// --- Piny sprzętowe ---
#define I2S_BCLK_PIN    GPIO_NUM_4   // D3
#define I2S_WS_PIN      GPIO_NUM_3   // D2
#define I2S_DIN_PIN     GPIO_NUM_2   // D1 (dane z ADC kodekiem do ESP)
#define I2C_SDA_PIN     GPIO_NUM_5   // D4
#define I2C_SCL_PIN     GPIO_NUM_6   // D5

// --- ESP-NOW ---
#define CHANNEL         11

// !!! WPISZ TUTAJ ADRES MAC TWOJEGO ODBIORNIKA !!!
static uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = { 0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88 };

// --- Parametry audio ---
#define SAMPLE_RATE         48000
#define LC3_FRAME_DT_US     2500        // 2.5 ms
#define SAMPLES_PER_FRAME   120         // 2.5 ms × 48000 Hz = 120 próbek
#define LC3_FRAME_BYTES     40          // floor(128000 × 2500 / 8 / 1_000_000) = 40 B

typedef struct __attribute__((packed)) {
    uint32_t seq_num;
    uint16_t crc;
    uint8_t  lc3_data[LC3_FRAME_BYTES];
} audio_packet_t;  // 46 bajtów (40 B LC3 + 6 B nagłówek)

// Koder LC3 — statyczna alokacja, zero malloc
static lc3_encoder_mem_48k_t s_enc_mem;
static lc3_encoder_t         s_encoder;

// Roboczy bufor stereo I2S — static, nie zajmuje stosu taska
// Odczytujemy STEREO (L+R), żeby wyciągnąć tylko prawy kanał
static int16_t s_stereo_buf[SAMPLES_PER_FRAME * 2];

static i2s_chan_handle_t rx_chan = NULL;
static RingbufHandle_t   audio_ringbuf;

extern void Setup_codec(void);

// --- 1. CAPTURE: Odczyt I2S + ekstrakcja prawego kanału ---
void audio_capture_task(void *pvParameters)
{
    int16_t mono_buf[SAMPLES_PER_FRAME];
    size_t bytes_read;

    while (1) {
        // Odczyt jednej ramki LC3 w formacie STEREO: 120 próbek × 2 kanały × 2 bajty = 480 B
        esp_err_t ret = i2s_channel_read(rx_chan, s_stereo_buf,
                                         sizeof(s_stereo_buf), &bytes_read,
                                         portMAX_DELAY);
        if (ret != ESP_OK) continue;

        // Ekstrakcja prawego kanału (indeksy nieparzyste: L=0, R=1, L=2, R=3, ...)
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            mono_buf[i] = s_stereo_buf[i * 2 + 1];
        }

        xRingbufferSend(audio_ringbuf, mono_buf, sizeof(mono_buf), 0);
    }
}

// --- 2. KODER LC3 + WYSYŁKA ---
void audio_tx_task(void *pvParameters)
{
    uint32_t current_seq = 0;
    static audio_packet_t tx_packet;  // static — nie zajmuje stosu

    while (1) {
        size_t item_size;
        void *item = xRingbufferReceive(audio_ringbuf, &item_size, portMAX_DELAY);

        if (item != NULL && item_size == (SAMPLES_PER_FRAME * sizeof(int16_t))) {
            tx_packet.seq_num = current_seq++;

            lc3_encode(s_encoder, LC3_PCM_FORMAT_S16,
                       item, 1, LC3_FRAME_BYTES, tx_packet.lc3_data);

            tx_packet.crc = 0;
            tx_packet.crc = esp_crc16_le(UINT16_MAX,
                                          (uint8_t const *)&tx_packet,
                                          sizeof(audio_packet_t));

            esp_now_send(receiver_mac, (uint8_t *)&tx_packet, sizeof(audio_packet_t));

            vRingbufferReturnItem(audio_ringbuf, item);
        }
    }
}

static void setup_i2c(void)
{
    i2c_config_t conf = {};
    conf.mode             = I2C_MODE_MASTER;
    conf.sda_io_num       = I2C_SDA_PIN;
    conf.scl_io_num       = I2C_SCL_PIN;
    conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
}

static void setup_i2s_rx(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 4;
    chan_cfg.dma_frame_num = 120;   // identycznie jak w RX

    // Tworzymy tylko kanał odbiorczy (ESP odbiera dane z ADC kodekiem)
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}

static void wifi_config(void)
{
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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    setup_i2c();
    Setup_codec();
    setup_i2s_rx();
    wifi_config();

    // Inicjalizacja kodera LC3: 2.5 ms, 48 kHz, statyczna alokacja
    s_encoder = lc3_setup_encoder(LC3_FRAME_DT_US, SAMPLE_RATE, 0, &s_enc_mem);
    // LTPF zostawiamy włączone — gitara ma harmoniki i LTPF może pomóc

    // Ring buffer: 4 ramki × 120 próbek × 2 bajty = 960 bajtów
    audio_ringbuf = xRingbufferCreate(sizeof(int16_t) * SAMPLES_PER_FRAME * 4,
                                       RINGBUF_TYPE_BYTEBUF);

    // audio_capture_task: Core 1, najwyższy priorytet — I2S musi być opróżniany na czas
    xTaskCreatePinnedToCore(audio_capture_task, "audio_cap",  4096, NULL,
                            configMAX_PRIORITIES - 1, NULL, 1);
    // audio_tx_task: Core 1, nieco niższy priorytet — LC3 encode + ESP-NOW
    xTaskCreatePinnedToCore(audio_tx_task,      "audio_tx",  8192, NULL,
                            configMAX_PRIORITIES - 2, NULL, 1);
}
