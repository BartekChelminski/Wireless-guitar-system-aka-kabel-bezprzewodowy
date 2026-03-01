#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/i2s.h>

// --- KONFIGURACJA MAC ODBIORNIKA ---
uint8_t slaveAddress[] = {0xD0, 0xCF, 0x13, 0x26, 0x71, 0x14}; 
#define CHANNEL 3

// --- KONFIGURACJA I2S (MIKROFON) ---
#define I2S_SCK_PIN   14
#define I2S_WS_PIN    15
#define I2S_SD_PIN    32  // Dane z mikrofonu

// Parametry Audio
#define SAMPLE_RATE 44100     
#define SAMPLES_PER_PACKET 120 // 120 próbek * 2 bajty = 240 bajtów

// Struktura danych
typedef struct struct_message {
  int16_t audioData[SAMPLES_PER_PACKET];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Inicjalizacja I2S w trybie ODBIORU (RX)
void setupI2S_Common() {
  i2s_config_t i2s_config = {
    // ... tryb RX lub TX zależnie od urządzenia ...
    .sample_rate = SAMPLE_RATE, // 44100
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // MONO jest kluczowe! Stereo (2ch) to 2x więcej danych (176 kB/s) - to już nie zadziała.
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    
    // ZMIANY DLA STABILNOŚCI PRZY 44.1kHz:
    .dma_buf_count = 16,     // Zwiększamy liczbę buforów (było 8)
    .dma_buf_len = 256,      // Długość bufora
    .use_apll = true,        // Włączamy APLL (Analog PLL) - lepszy zegar dla 44.1kHz
    .tx_desc_auto_clear = true
  };
  // ... reszta kodu instalacji ...
}

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE, // Nie wysyłamy dźwięku
    .data_in_num = I2S_SD_PIN          // Tu odbieramy dźwięk
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Callback pusty dla szybkości
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Restart WiFi dla pewności
  esp_wifi_stop();
  delay(10);
  esp_wifi_start();

  // Konfiguracja regionu (zachowana)
  wifi_country_t config;
  strcpy(config.cc, "PL");
  config.schan = 1;
  config.nchan = 13;
  config.policy = WIFI_COUNTRY_POLICY_AUTO;
  esp_wifi_set_country(&config);

  // Ustawienia 802.11g (stabilniejsze dla streamingu niż N)
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  // Dodawanie Peera
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Błąd dodawania peera");
  }

  // Uruchomienie mikrofonu
  setupI2S_Common();
  Serial.println("Mikrofon I2S uruchomiony. Nadawanie...");
}

void loop() {
  size_t bytesRead = 0;

  // 1. ODCZYT Z MIKROFONU (Funkcja blokująca!)
  // ESP32 czeka tutaj, aż mikrofon nazbiera 240 bajtów danych.
  // Dzięki temu nie potrzebujemy 'delay()' - tempo dyktuje próbkowanie audio.
  esp_err_t read_result = i2s_read(I2S_NUM_0, 
                                   &myData.audioData, 
                                   sizeof(myData.audioData), 
                                   &bytesRead, 
                                   portMAX_DELAY);

  if (read_result == ESP_OK && bytesRead == sizeof(myData.audioData)) {
    
    // Opcjonalnie: Zwiększenie głośności cyfrowo (Bit Shift)
    // Mikrofony MEMS bywają ciche. Odkomentuj poniższe, jeśli jest za cicho:
    /*
    for (int i=0; i < SAMPLES_PER_PACKET; i++) {
       myData.audioData[i] = myData.audioData[i] << 2; // Mnożenie głośności x4
    }
    */

    // 2. WYSYŁKA ESP-NOW
    esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));
  }
}