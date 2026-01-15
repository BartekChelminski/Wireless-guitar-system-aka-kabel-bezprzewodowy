#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <SparkFun_WM8960_Arduino_Library.h>

// --- KONFIGURACJA PINÓW (S3) ---
#define I2S_BCLK_PIN  D3
#define I2S_LRCK_PIN  D2
#define I2S_DOUT_PIN  D1
#define I2C_SDA_PIN   D4 
#define I2C_SCL_PIN   D5


#define CHANNEL 3

// --- ZMIANA NA 44100 Hz ---
#define SAMPLE_RATE 44100 
#define SAMPLES_PER_PACKET 240 

// Kolejka
QueueHandle_t audioQueue = NULL;

typedef struct struct_message {
  int16_t audioData[SAMPLES_PER_PACKET];
} struct_message;

struct_message incomingMessage;

WM8960 codec;

void codec_setup() {
   //Scenariusze drogi sygnału są nieźle opisane w dokumentacji. W bibliotece sparkfun są gotowe funkcje.
  codec.reset();

  codec.enableVREF();
  codec.enableVMID();
  
  codec.setWL(WM8960_WL_16BIT);
  // --- KONFIGURACJA PRZEZ FUNKCJE BIBLIOTEKI ---


  // Włączenie DAC i wyjść
  codec.enableDacRight();
  codec.enableDacLeft();

  codec.disableDacMute();

  codec.enableLOMIX();//  Umożliwienienie sterowania mikserami
  codec.enableROMIX(); //

  codec.enableLD2LO();// ldac - lmixer
  codec.enableRD2RO();//  rdac - rmixer



  codec.enableHeadphones();
  // Głośność Słuchawek (L i R) - Zakres 0-127
  codec.setHeadphoneVolumeDB(-30.0);
  
  // Włączenie wyjścia słuchawkowego (odciszenie)


  //Konfiguracja zegara - dokumentacja strona 61
  codec.enablePLL();
  codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
  codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2); 
  codec.setPLLN(7);
  codec.setPLLK(134, 194, 38); //0x86C226h
  codec.setSMD(1);
  codec.setCLKSEL(WM8960_CLKSEL_PLL);

  // Opcjonalnie: Głośniki (Zakres 0-127)
  // codec.setSpeakerVolume(120);
  // codec.enableSpeakers();

  //Serial.println("Kodek skonfigurowany przez bibliotekę.");
}

// Callback ESP-NOW
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) return;
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
  // Send to queue
  xQueueSendFromISR(audioQueue, &incomingMessage, NULL);
}

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo Output
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    
    // ZWIĘKSZONE BUFORY DLA 44.1 kHz
    // Przy 44.1kHz dane płyną szybciej, większy bufor zapobiega trzaskom (underflow)
    .dma_buf_count = 16,  
    .dma_buf_len = 256,   
    
    .use_apll = false, // false, bo BCLK generuje ESP z dzielnika, a PLL jest w kodeku
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0  
  };

  i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRCK_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL)==ESP_OK) Serial.println(" I2s driver installed");
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void sprawdz_pol_i2c()
{
  byte error, address;
  int nDevices = 0;

  Serial.println("Rozpoczynam skanowanie...");

  for (address = 1; address < 127; address++) {
    // Próba rozpoczęcia transmisji do danego adresu
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      // Sukces - urządzenie odpowiedziało (ACK)
      Serial.print("ZNALEZIONO urządzenie I2C pod adresem: 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      // Podpowiedź dla WM8960
      if (address == 0x1A) {
        Serial.print("  <-- To prawdopodobnie Twój kodek WM8960!");
      }
      Serial.println();
      nDevices++;
    } 
    else if (error == 4) {
      Serial.print("Nieznany błąd przy adresie 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("Nie znaleziono żadnych urządzeń I2C.\n");
  } else {
    Serial.println("Skanowanie zakończone.\n");
  }

  delay(1000); // Czekaj 1 sekund przed kolejnym skanem
}

void setup() {
  Serial.begin(115200);


  Serial.println("Kofnfiguracja i2c");
   

  // 1. Setup Codec
  if(Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN))
  {
    Serial.println("Skonfigurowano i2c");
  }else Serial.println("Blad konfiguracji i2c");

  delay(100);

  if (!codec.begin()) {
    Serial.println("BŁĄD: Nie wykryto WM8960! Sprawdź I2C.");
    while (true);
  }
/*
  for(int i =0; i<5; i++)
  {
    sprawdz_pol_i2c();
  }
*/
  

  delay(100);
  codec_setup();

  Serial.println("Zakonczono codec setup");



  // 2. Setup WiFi
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

  // 3. Setup Queue & ESP-NOW
  audioQueue = xQueueCreate(20, sizeof(struct_message));

  if (audioQueue == NULL) {
    Serial.println(" !!! CRITICAL ERROR: Nie udało się utworzyć kolejki (Brak RAM?) !!!");
    while(1); // Zatrzymaj program, nie idź dalej
  }Serial.println(" -> Kolejka utworzona pomyślnie");
  

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Error");
    return;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // 4. Setup I2S
    setupI2S();
  Serial.println("I2S Running");
}

void loop() {
  struct_message currentPacket;
  size_t bytesWritten;


  // Wyswietlanie sygnalu na ploterze
 /* if (audioQueue != NULL && xQueueReceive(audioQueue, &currentPacket, 100) == pdPASS) {
    
    // Pętla po wszystkich próbkach w paczce
    for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
      
      // TRICK DLA PLOTTERA:
      // Wypisujemy 3 wartości w jednej linii oddzielone przecinkami.
      // 1. Stała dolna granica (-32000)
      // 2. Stała górna granica (32000)
      // 3. Twój sygnał
      // Dzięki temu wykres nie będzie "skakał" (autoscale) tylko będzie stabilny.
      
      Serial.print("-32000,"); // Min Y
      Serial.print("32000,");  // Max Y
      Serial.println(currentPacket.audioData[i]); // Sygnał
    }
  }
*/
  // Timeout 
  if (xQueueReceive(audioQueue, &currentPacket, 500 / portTICK_PERIOD_MS) == pdPASS) {
    
    // Konwersja Mono -> Stereo
    int16_t stereoBuffer[SAMPLES_PER_PACKET * 2];
    
    for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
      int16_t sample = currentPacket.audioData[i];
      stereoBuffer[i * 2]     = sample; 
      stereoBuffer[i * 2 + 1] = sample; 
    }

    i2s_write(I2S_NUM_0, stereoBuffer, sizeof(stereoBuffer), &bytesWritten, portMAX_DELAY);
    }
    
  
}