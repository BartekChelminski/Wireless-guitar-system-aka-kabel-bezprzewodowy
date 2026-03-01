/*
Podłączenie plytek
                          ESP32         Kodek

Zasilanie	                3.3V          VCC
Masa	                    GND	          GND
Sterowanie    SDA	        D4    	      SDA
Sterowanie    SCL	        D5            SCL
Zegar         BCLK	      D3            CLK
Zegar         LRCK	      D2    	      WS
Dane Audio	              D1    	      RX_SDA
MCLK	                    BRAK	        BRAK

Wymagana biblioteka SparkFun_WM8960_Arduino_Library
*/

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_WM8960_Arduino_Library.h> // Biblioteka SparkFun
#include "driver/i2s.h"
#include <math.h>

// --- KONFIGURACJA PINÓW ---
//#define I2S_MCK_PIN   1   // Zegar Master (MCLK) z ESP32
#define I2S_BCLK_PIN  D3
#define I2S_LRCK_PIN  D2
#define I2S_DOUT_PIN  D1
#define I2C_SDA   D4 
#define I2C_SCL   D5

// --- PARAMETRY ---
#define SAMPLE_RATE   44100
#define WAVE_FREQ_HZ  800
#define VOLUME_DIGITAL 10000 

// Obiekt biblioteki
WM8960 codec;

#define I2S_PORT I2S_NUM_0
#define BUFFER_LEN 1024

void codec_setup()
{
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
  codec.setHeadphoneVolumeDB(-20.0);
  
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

void setup() {
  Serial.begin(115200);
  //Serial.println("Start ESP32 + SparkFun Lib + MCLK(GPIO0)");

  // 1. Inicjalizacja I2C na naszych pinach
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

if (!codec.begin()) {
    Serial.println("BŁĄD: Nie wykryto WM8960! Sprawdź I2C.");
    while (true);
  }
  // 2. Inicjalizacja Kodeka za pomocą biblioteki
  codec_setup();

  // 
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,     
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

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  
  // Dla starszych bibliotek (gdyby MCLK nie ruszyło):
  // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);

  i2s_zero_dma_buffer(I2S_PORT);
  //Serial.println("I2S Start (MCLK Running)");
}

double phase = 0;
const double phaseIncrement = (2.0 * PI * WAVE_FREQ_HZ) / SAMPLE_RATE;

void loop() {
  size_t bytes_written;
  int16_t samples[BUFFER_LEN * 2]; 

  for (int i = 0; i < BUFFER_LEN; i++) {
    int16_t sampleValue = (int16_t)(sin(phase) * VOLUME_DIGITAL);
    samples[i * 2]     = sampleValue; 
    samples[i * 2 + 1] = sampleValue; 

    phase += phaseIncrement;
    if (phase >= 2.0 * PI) phase -= 2.0 * PI;
  }

  i2s_write(I2S_PORT, samples, sizeof(samples), &bytes_written, portMAX_DELAY);
}