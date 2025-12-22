#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

uint8_t slaveAddress[] = {0xD0, 0xCF, 0x13, 0x26, 0x71, 0x14}; 
#define CHANNEL 3 // Musi być taki sam jak w Master

typedef struct struct_message {
  uint8_t payload[250];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Callback: Odbiór danych
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // Sprawdź czy urządzenie, od którego otrzymaliśmy dane, jest już na liście peerów

  // Kopiuj odebrane dane do struktury
  memcpy(&myData, incomingData, sizeof(myData));
  
  // --- NATYCHMIASTOWE ODESŁANIE (ECHO) ---
 esp_err_t result = esp_now_send(slaveAddress, (uint8_t *) &myData, sizeof(myData));

  if (result == ESP_OK) {
     Serial.println("Wysłano pakiet");
  } else {
    Serial.println("Błąd wysyłania");
  }
  
  // Opcjonalnie mrugnij LEDem dla wizualizacji
  
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  pinMode(2, OUTPUT);
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

   // 2. WŁĄCZENIE protokołu 802.11n (Wymagane dla MCS0)
  // Wcześniej wyłączaliśmy 'N', teraz musimy go przywrócić.
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
  
  // Ustawienie kanału i prędkości (MCS0 dla testu, można zmienić na 2M_L)
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);



  if (esp_now_init() != ESP_OK) {
    Serial.println("Błąd ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));


  memset(&peerInfo, 0, sizeof(peerInfo));
  
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;

   peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Błąd: Nie udało się dodać Mastera jako peera!");
  } else {
    Serial.println("Master dodany do listy zaufanych.");
  }

}

void loop() {
  // Pętla pusta - wszystko dzieje się w przerwaniu
}