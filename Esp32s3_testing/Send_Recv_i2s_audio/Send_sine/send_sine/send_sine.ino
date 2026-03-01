/*
  UWAGA
  TO JEST KOD PRZESYŁAJĄCY SZTUCZNIE WYGENEROWANY CYFROWO "SINUS"
  PONIEWAŻ NIE MAM DACA.
  PO TEJ STRONIE (NADAWCZEJ) WYSYŁAMY SZTUCZNIE WYGENEROWANE CYFERKI.
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
#include <math.h>

// --- KONFIGURACJA MAC ODBIORNIKA ---
uint8_t slaveAddress[] = {0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88}; 
#define CHANNEL 3

// --- PARAMETRY AUDIO ---
#define SAMPLE_RATE 16000     // 16 kHz (kompromis dla stabilności ESP-NOW)
#define WAVE_FREQ 20         // 440 Hz (Dźwięk A4)
#define AMPLITUDE 10000       // Głośność (max 32767 dla 16-bit)
#define SAMPLES_PER_PACKET 120 // 120 próbek * 2 bajty = 240 bajtów (limit ESP-NOW to 250)

// Struktura danych (240 bajtów audio)
typedef struct struct_message {
  int16_t audioData[SAMPLES_PER_PACKET]; // 16-bitowe próbki dźwięku
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

double phase = 0.0;
double phaseIncrement = 0.0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Opcjonalnie: obsługa błędów, ale przy audio lepiej po prostu słać dalej
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Reset WiFi dla pewności
  esp_wifi_stop();
  delay(10);
  esp_wifi_start();

  // Konfiguracja regionu (zachowana z Twojego kodu)
  wifi_country_t config;
  strcpy(config.cc, "PL");
  config.schan = 1;
  config.nchan = 13;
  config.policy = WIFI_COUNTRY_POLICY_AUTO;
  esp_wifi_set_country(&config);

  // Ustawienia protokołu i kanału
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd ESP-NOW");
    return;
  }
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));

  // Rejestracja Peera
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Błąd dodawania peera");
  }

  // Obliczenie kroku fazy dla sinusa
  // 2 * PI * Freq / SampleRate
  phaseIncrement = (2.0 * PI * WAVE_FREQ) / SAMPLE_RATE;
}

void loop() {
  // 1. Generowanie bufora audio (sinusoida)
  for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
    myData.audioData[i] = (int16_t)(AMPLITUDE * sin(phase));
    phase += phaseIncrement;
    if (phase > 2.0 * PI) phase -= 2.0 * PI;
  }

  // 2. Wysłanie danych
  esp_err_t result = esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));

  if (result != ESP_OK) {
    Serial.println("Błąd wysyłania"); // Odkomentuj do debugowania, ale spowalnia
  }

  // 3. Kontrola tempa (prosty throttling)
  // Paczka zawiera 120 próbek przy 16000Hz.
  // Czas trwania paczki = 120 / 16000 = 0.0075s = 7.5ms = 7500us.
  // Odejmujemy trochę czasu na narzut transmisji.
  delayMicroseconds(6000); 
}