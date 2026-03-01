/*
  UWAGA
  TO JEST KOD PRZESYŁAJĄCY SZTUCZNIE WYGENEROWANY CYFROWO "SINUS"
  PONIEWAŻ NIE MAM DACA.
  PO TEJ STRONIE (ODBIORCZEJ) WYŚWIETLAMY CYFERKI NA SERIAL PLOTTER.
  ~Kuba

  KOD DO OBSŁUGI STRUMIENIA I2S Z KODEKA (nadajnik i odbiornik)

  //ZMIANA Z 16000 NA 44100
  #define SAMPLE_RATE 44100


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

*/






#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/i2s.h>


#define CHANNEL 3
// --- KONFIGURACJA I2S (Zmień piny wg połączeń) ---
#define I2S_BCLK_PIN  4
#define I2S_LRCK_PIN  5
#define I2S_DOUT_PIN  6

#define SAMPLE_RATE 16000
#define SAMPLES_PER_PACKET 120

// Kolejka do buforowania pakietów audio między przerwaniem a pętlą loop
QueueHandle_t audioQueue;

// Struktura danych (musi być identyczna jak w nadajniku)
typedef struct struct_message {
  int16_t audioData[SAMPLES_PER_PACKET];
} struct_message;

struct_message incomingMessage;

// Callback: Odbiór danych
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // Sprawdzenie długości pakietu
  if (len != sizeof(struct_message)) return;

  // Skopiowanie danych do lokalnej struktury
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

  // Wysłanie całej struktury do kolejki (z kontekstu przerwania)
  // Jeśli kolejka pełna, gubimy pakiet (najstarszy), żeby zachować czas rzeczywisty
  xQueueSendFromISR(audioQueue, &incomingMessage, NULL);
}

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Lub RIGHT/STEREO zależnie od DAC
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256, // Rozmiar bufora DMA
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRCK_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Restart WiFi i konfiguracja
  esp_wifi_stop();
  delay(10);
  esp_wifi_start();

  wifi_country_t config;
  strcpy(config.cc, "PL");
  config.schan = 1;
  config.nchan = 13;
  config.policy = WIFI_COUNTRY_POLICY_AUTO;
  esp_wifi_set_country(&config);

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE); // Kanał 3

  // Inicjalizacja kolejki (pomieści 10 pakietów po 240 bajtów)
  audioQueue = xQueueCreate(10, sizeof(struct_message));

  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Inicjalizacja I2S
  setupI2S();
  Serial.println("Odbiornik I2S gotowy");
}

void loop() {

struct_message currentPacket;

  // Pobierz z kolejki
  if (xQueueReceive(audioQueue, &currentPacket, portMAX_DELAY) == pdPASS) {
    
    // Wypisz każdą próbkę w nowej linii - to format dla Serial Plottera
    for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
        Serial.println(currentPacket.audioData[i]);
    }
  }

  /*
  struct_message currentPacket;
  size_t bytesWritten;
  // Oczekiwanie na dane w kolejce (blokuje pętlę do momentu przyjścia danych)
  if (xQueueReceive(audioQueue, &currentPacket, portMAX_DELAY) == pdPASS) {
    
    // Zapis danych do bufora DMA I2S
    // Funkcja ta blokuje wykonanie, jeśli bufor DMA jest pełny,
    // co zapewnia synchronizację tempa odtwarzania.
    i2s_write(I2S_NUM_0, 
              currentPacket.audioData, 
              sizeof(currentPacket.audioData), 
              &bytesWritten, 
              portMAX_DELAY);
  }
  */
}