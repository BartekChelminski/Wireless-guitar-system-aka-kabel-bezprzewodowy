#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// --- KONFIGURACJA ---
// Wpisz tu adres MAC drugiego urządzenia (SLAVE)
uint8_t slaveAddress[] = {0x80, 0xB5, 0x4E, 0xF3, 0xB2, 0x88}; 
#define CHANNEL 3

// Struktura danych (250 bajtów)
typedef struct struct_message {
  uint8_t payload[250];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

unsigned long startMicros; // Zmienna do zapisu czasu startu


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
// Callback: Odbiór danych (Odpowiedź od Slave)
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  unsigned long endMicros = micros(); // Zatrzymaj "stoper"
  unsigned long rtt = endMicros - startMicros; // Czas podróży tam i z powrotem
  
  // Obliczenia
  float oneWayLatency = rtt / 2.0;

  Serial.print("RTT: ");
  Serial.print(rtt);
  Serial.print(" us | Opóźnienie (One-Way): ");
  Serial.print(oneWayLatency);
  Serial.print(" us (");
  Serial.print(oneWayLatency / 1000.0, 3);
  Serial.println(" ms)");
}

void setup() {
  Serial.begin(115200);
delay(3000);

  WiFi.mode(WIFI_STA);


// 2. ODŁĄCZANIE OBECNEJ KONFIGURACJI
  // Czasami wymagane na S3, aby zmiany protokołu weszły w życie
  esp_wifi_stop();
  delay(10);
  esp_wifi_start();

// --- KONFIGURACJA DLA JAPONII (JEST) ---
wifi_country_t config;
  
  strcpy(config.cc, "PL"); // Kod kraju: JP (Japonia)
  config.schan = 1;
  config.nchan = 13;       // Odblokowuje kanał 14
  config.policy = WIFI_COUNTRY_POLICY_AUTO;

  esp_wifi_set_country(&config);



  // 2. WŁĄCZENIE protokołu 
  // esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
  
  // Ustawienie kanału i prędkości (MCS0 dla testu, można zmienić na 2M_L)
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  // 4. Ustawienie sztywnej prędkości MCS0 (ok. 6.5 Mbps)
  // Ponieważ używamy standardu N, funkcja powinna przyjąć stawkę MCS bez błędu AMPDU.


  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd inicjalizacji ESP-NOW");
    return;
  }
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

// 4. Rejestracja Peera - POPRAWKA DLA S3
  // Najpierw czyścimy strukturę zerami, żeby nie było śmieci
  memset(&peerInfo, 0, sizeof(peerInfo));
  
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;

   peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Błąd dodawania peera");
  }else {
    Serial.println("Peer dodany poprawnie.");
  }

  Serial.println("Adres mac:");
  Serial.println(WiFi.macAddress());



  // Wypełnij bufor danymi
  for(int i=0; i<250; i++) myData.payload[i] = i;
}

void loop() {
  // 1. Zapisz czas startu
  startMicros = micros();
  
  // 2. Wyślij ping
  esp_err_t result = esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));

  if (result == ESP_OK) {
     Serial.println("Wysłano pakiet");
  } else {
    Serial.println("Błąd wysyłania");
  }


  // Wyślij ponownie za 1 sekundę
  delay(1000);
}