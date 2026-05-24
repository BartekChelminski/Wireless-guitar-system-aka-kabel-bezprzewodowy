

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "Codec_lib.h"
#include <driver/i2s.h>



	WM8960 codec;
	
  extern "C" void Setup_codec()
  {
    if (!codec.begin()) {
    printf("BLAD: Nie wykryto WM8960! Sprawdz I2C.\n");
    while (true);
  }else
  {printf("Wywolano codec_begin\n");}
  
  
  //Scenariusze drogi sygnału są nieźle opisane w dokumentacji. W bibliotece sparkfun są gotowe funkcje.
  codec.reset();

  codec.enableVREF();
  codec.enableVMID();
  
  //codec.enableMasterMode(); //tryb master dla sprawdzenia kontroli bufora w esp
  
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
  codec.setHeadphoneVolumeDB(6);
  
  // Włączenie wyjścia słuchawkowego (odciszenie)


  //Konfiguracja zegara - dokumentacja strona 61
  codec.enablePLL();
  codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
  codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2); 
  codec.setPLLN(8);
  codec.setPLLK(49, 38, 232); //0x86C226h
  codec.setSMD(1);
  codec.setCLKSEL(WM8960_CLKSEL_PLL);

  // Opcjonalnie: Głośniki (Zakres 0-127)
  // codec.setSpeakerVolume(120);
  // codec.enableSpeakers();
	printf("Koniec konfiguracji kodeka\n");
  
  }
  // WARNING: if program reaches end of function app_main() the MCU will restart.
