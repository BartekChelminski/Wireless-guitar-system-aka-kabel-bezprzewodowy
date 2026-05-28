
#include <stdio.h>
#include <stdbool.h>
#include "Codec_lib.h"

WM8960 codec;

extern "C" void Setup_codec()
{
    if (!codec.begin()) {
        printf("BLAD: Nie wykryto WM8960! Sprawdz I2C.\n");
        while (true);
    }
    printf("WM8960 wykryty na I2C\n");

    // Reset do stanu domyslnego
    codec.reset();

    // --- KONFIGURACJA ZEGARA PLL ---
    // MCLK = 24 MHz (XO na plytce)

    // Dla 48 kHz: SYSCLK = 12.288 MHz
    // PLLPRESCALE = /2 -> F2 = 12 MHz
    // R = 8.192 -> N=8, K=0x3126E8
    // SYSCLKDIV = /2, fixed post-divide = /4
    codec.enablePLL();
    codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
    codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
    codec.setPLLN(8);
    codec.setPLLK(49, 38, 232);  // 0x3126E8 dla 48 kHz
    codec.setSMD(1);             // tryb ulamkowy
    codec.setCLKSEL(WM8960_CLKSEL_PLL);

    // Zasilanie ogólne
    codec.enableVREF();
    codec.enableVMID();

    // Zasilanie miksera wejsciowego i PGA prawego kanalu
    codec.enableAINR();
    codec.enableRMIC();

    // Podlaczenie RINPUT1 do PGA (wejscie odwracajace)
    codec.connectRMN1();

    // Wylaczenie wyciszenia PGA
    codec.disableRINMUTE();

    // Podlaczenie wyjscia PGA do boost mixera
    codec.connectRMIC2B();

    // Wzmocnienie PGA: 0 dB
    codec.setRINVOLDB(0.00);

    // Wlaczenie ADC prawego kanalu
    codec.enableAdcRight();

    // Dlugosc slowa: 16 bitow
    codec.setWL(WM8960_WL_16BIT);

    printf("Konfiguracja kodeka ADC zakonczona (48 kHz, prawy kanal)\n");
}
