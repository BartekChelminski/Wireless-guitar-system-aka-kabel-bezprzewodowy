#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>

// --- KONFIGURACJA MAC ODBIORNIKA ---
// WPISZ TUTAJ ADRES MAC SWOJEGO ODBIORNIKA!
uint8_t slaveAddress[] = {0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88}; 
#define CHANNEL 3

// --- PARAMETRY AUDIO ---
#define SAMPLE_RATE 44100     // Standard Audio CD
#define WAVE_FREQ 800         // Częstotliwość dźwięku (Hz)
#define AMPLITUDE 10000       // Głośność
#define SAMPLES_PER_PACKET 240 // 240 bajtów (blisko limitu ESP-NOW)

// Obliczenie interwału wysyłania:
// (120 próbek / 44100 próbek na sek) * 1 000 000 = ~2721 mikrosekund
const unsigned long PACKET_INTERVAL_US = 2721;

typedef struct struct_message {
  int16_t audioData[SAMPLES_PER_PACKET]; 
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

double phase = 0.0;
double phaseIncrement = 0.0;
unsigned long lastPacketTime = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Callback wysłania - przy audio 44.1kHz nie mamy czasu na logowanie każdego pakietu
}

void setup() {
  Serial.begin(115200);
  
  // Konfiguracja WiFi (Musi być taka sama jak w odbiorniku)
  WiFi.mode(WIFI_STA);
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

  // Obliczenie kroku fazy
  phaseIncrement = (2.0 * PI * WAVE_FREQ) / SAMPLE_RATE;
  
  lastPacketTime = micros();
}

void loop() {
  unsigned long currentMicros = micros();

  // Nieblokująca kontrola czasu
  // Wysyłamy paczkę tylko wtedy, gdy minęło 2721 mikrosekund od poprzedniej

  if (currentMicros - lastPacketTime >= PACKET_INTERVAL_US) {
    
    // Utrzymujemy stabilny zegar (kompensacja driftu)
    lastPacketTime += PACKET_INTERVAL_US;

    // 1. Generowanie 120 próbek
    for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
      myData.audioData[i] = (int16_t)(AMPLITUDE * sin(phase));
      phase += phaseIncrement;
      // Zawijanie fazy
      if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }

   /* if (currentMicros - lastPacketTime >= PACKET_INTERVAL_US) {
    
    // Utrzymujemy stabilny zegar (kompensacja driftu)
    lastPacketTime += PACKET_INTERVAL_US;

    // 1. Generowanie 120 próbek
    for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
      myData.audioData[i] = AMPLITUDE;
      //phase += phaseIncrement;
      // Zawijanie fazy
      //if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }
*/
    

  // Pobierz paczkę z kolejki
    

    // 2. Wysłanie danych
    // Uwaga: esp_now_send jest asynchroniczne, ale w bibliotece Arduino
    // może przyblokować na chwilę.
    esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));
  }
  
  // Tutaj procesor ma "wolne" przez większość czasu (ok. 2ms luzu między pakietami)
  // Można tu robić inne rzeczy, byle trwały krócej niż 2ms.
}