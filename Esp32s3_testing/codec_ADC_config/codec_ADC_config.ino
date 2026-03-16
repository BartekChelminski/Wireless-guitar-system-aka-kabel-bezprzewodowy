#include <SparkFun_WM8960_Arduino_Library.h>
#include "driver/i2s.h"
#include <Wire.h>

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

void setup() {
  
  Serial.begin(115200);

  if (!codec.begin())
  {
    Serial.println("The device did not respond. Please check wiring.");
    while (1);
  }

  codec_setup();
  
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
