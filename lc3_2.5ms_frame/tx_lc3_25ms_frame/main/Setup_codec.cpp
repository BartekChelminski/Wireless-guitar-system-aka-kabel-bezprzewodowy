#include "Codec_lib.h"
#include <stdio.h>

static WM8960 codec;

extern "C" void Setup_codec()
{
    if (!codec.begin()) {
        printf("BLAD: Nie wykryto WM8960! Sprawdz I2C.\n");
        while (true);
    }
    printf("WM8960 wykryty\n");

    codec.reset();

    // Zasilanie referencji analogowej
    codec.enableVREF();
    codec.enableVMID();

    // --- Konfiguracja PLL dla 48 kHz ---
    // MCLK = 24 MHz, PLLPRESCALE = /2 → F2 = 12 MHz
    // PLLN = 8, PLLK = 0x313128 → SYSCLK = 12.288 MHz
    // SYSCLKDIV = /2 → SYSCLK/2 = 6.144 MHz (= 48000 * 256 / 2)
    codec.enablePLL();
    codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
    codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
    codec.setPLLN(8);
    codec.setPLLK(49, 38, 232);   // 0x313128h — identycznie jak RX dla 48 kHz
    codec.setSMD(1);               // tryb ułamkowy
    codec.setCLKSEL(WM8960_CLKSEL_PLL);

    // --- Ścieżka sygnału: RINPUT1 → PGA → Boost Mixer → ADC ---

    // Włącz prawy wzmacniacz wejściowy (AINR)
    codec.enableAINR();

    // Włącz prawostronny PGA
    codec.enableRMIC();

    // Połącz RINPUT1 (pin R1 kodeków, podłączony do gitary przez jack 6.35mm)
    // z wejściem odwracającym prawego PGA
    codec.connectRMN1();

    // Odciszy wejście PGA (domyślnie wyciszone po resecie)
    codec.disableRINMUTE();

    // Połącz wyjście PGA z prawym Boost Mixerem
    codec.connectRMIC2B();

    // Wzmocnienie PGA: 0 dB (punkt startowy — reguluj w razie potrzeby)
    codec.setRINVOLDB(0.00f);

    // Włącz prawy ADC
    codec.enableAdcRight();

    // Format słowa: 16-bit I2S Philips
    codec.setWL(WM8960_WL_16BIT);

    printf("Konfiguracja kodeka (ADC, prawy kanal, 48 kHz) zakonczona\n");
}
