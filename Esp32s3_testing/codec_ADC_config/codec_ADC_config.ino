/*
Pin Connection
                          ESP32S3       Codec

Power	                    3.3V          VCC
Ground	                  GND	          GND
I2C Serial Clock         	D5            SCL
I2C Serial Data    	      D4    	      SDA
I2S Bit Clock             D3            CLK
I2S word select           D2    	      WS
I2S Data	                D1    	      TX_SDA

*/

#include <SparkFun_WM8960_Arduino_Library.h>
#include <Wire.h>
#include "driver/i2s_std.h"

#define I2C_SCL   GPIO_NUM_6
#define I2C_SDA   GPIO_NUM_5 
#define I2S_CLK_PIN  GPIO_NUM_4
#define I2S_WS_PIN  GPIO_NUM_3
#define I2S_TX_SDA_PIN  GPIO_NUM_2
#define SAMPLE_RATE   44100
#define SAMPLES_PER_READ 32

#define GOWNO D1

WM8960 codec;

void codec_setup(){

  //reset
  codec.reset();

  //clock configuration
  //f_MCLK = 24MHz (XO on a board)
  codec.enablePLL();
  codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
  codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2); 
  codec.setPLLN(7);
  codec.setPLLK(134, 194, 38); //0x86C226h
  codec.setSMD(1);
  codec.setCLKSEL(WM8960_CLKSEL_PLL);

  // power
  codec.enableVREF();
  codec.enableVMID();
  // power mixer
  codec.enableAINR();
  // power PGA
  codec.enableRMIC();

  // connect INPUT R1 to PGA
  codec.connectRMN1();
  // disable mute on PGA
  codec.disableRINMUTE();
  // connect PGA output to boost mixer
  codec.connectRMIC2B();

  // PGA volume in dB
  codec.setRINVOLDB(0.00);

  //ADC configuration
  codec.enableAdcRight();

  codec.setWL(WM8960_WL_16BIT);
}

i2s_chan_handle_t rx_handle;

int16_t i2s_raw_buffer[SAMPLES_PER_READ * 2];
int16_t mono_buffer[SAMPLES_PER_READ];

size_t bytes_read = 0;

void setup() {
  
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!codec.begin())
  {
    Serial.println("The device did not respond. Please check wiring.");
    while (1);
  }

  codec_setup();

  i2s_chan_config_t chan_cfg = {
  .id = I2S_NUM_0,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 4,
    .dma_frame_num = 32,
    .auto_clear_after_cb = false,
    .auto_clear_before_cb = false,
    .allow_pd = false,
    .intr_priority = 0,
  };
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    // może trzeba będzie dać --> I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits_per_sample, mono_or_stereo)
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = I2S_CLK_PIN,     //D3
      .ws = I2S_WS_PIN,       //D2
      .dout = I2S_GPIO_UNUSED,
      .din = I2S_TX_SDA_PIN,      //D1
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);
}

void loop() {
  
  esp_err_t ret = i2s_channel_read(rx_handle, i2s_raw_buffer, sizeof(i2s_raw_buffer), &bytes_read, 1000 / portTICK_PERIOD_MS);

  if (ret == ESP_OK){
    int16_t *src = &i2s_raw_buffer[1];
    int16_t *dst = mono_buffer;

    for (int i = 0; i < SAMPLES_PER_READ; i++) {
      *dst++ = *src;
      src += 2;
    }

    static int skip_counter = 0;

    if (skip_counter++ >= 10) { 
      for (int i = 0; i < SAMPLES_PER_READ; i++) {
          Serial.println(mono_buffer[i]);
      }
      skip_counter = 0;
    }
  }

  

}
