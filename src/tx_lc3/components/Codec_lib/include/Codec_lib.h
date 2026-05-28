/******************************************************************************
  SparkFun WM8960 Arduino Library (Przerobiona pod ESP-IDF)
******************************************************************************/

#ifndef __SPARKFUN_WM8960_H__
#define __SPARKFUN_WM8960_H__

// --- NATYWNE BIBLIOTEKI ESP-IDF ---
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

// I2C address (7-bit format)
#define WM8960_ADDR 0x1A 

// WM8960 register addresses
#define WM8960_REG_LEFT_INPUT_VOLUME 0x00
#define WM8960_REG_RIGHT_INPUT_VOLUME 0x01
#define WM8960_REG_LOUT1_VOLUME 0x02
#define WM8960_REG_ROUT1_VOLUME 0x03
#define WM8960_REG_CLOCKING_1 0x04
#define WM8960_REG_ADC_DAC_CTRL_1 0x05
#define WM8960_REG_ADC_DAC_CTRL_2 0x06
#define WM8960_REG_AUDIO_INTERFACE_1 0x07
#define WM8960_REG_CLOCKING_2 0x08
#define WM8960_REG_AUDIO_INTERFACE_2 0x09
#define WM8960_REG_LEFT_DAC_VOLUME 0x0A
#define WM8960_REG_RIGHT_DAC_VOLUME 0x0B
#define WM8960_REG_RESET 0x0F
#define WM8960_REG_3D_CONTROL 0x10
#define WM8960_REG_ALC1 0x11
#define WM8960_REG_ALC2 0x12
#define WM8960_REG_ALC3 0x13
#define WM8960_REG_NOISE_GATE 0x14
#define WM8960_REG_LEFT_ADC_VOLUME 0x15
#define WM8960_REG_RIGHT_ADC_VOLUME 0x16
#define WM8960_REG_ADDITIONAL_CONTROL_1 0x17
#define WM8960_REG_ADDITIONAL_CONTROL_2 0x18
#define WM8960_REG_PWR_MGMT_1 0x19
#define WM8960_REG_PWR_MGMT_2 0x1A
#define WM8960_REG_ADDITIONAL_CONTROL_3 0x1B
#define WM8960_REG_ANTI_POP_1 0x1C
#define WM8960_REG_ANTI_POP_2 0x1D
#define WM8960_REG_ADCL_SIGNAL_PATH 0x20
#define WM8960_REG_ADCR_SIGNAL_PATH 0x21
#define WM8960_REG_LEFT_OUT_MIX_1 0x22
#define WM8960_REG_RIGHT_OUT_MIX_2 0x25
#define WM8960_REG_MONO_OUT_MIX_1 0x26
#define WM8960_REG_MONO_OUT_MIX_2 0x27
#define WM8960_REG_LOUT2_VOLUME 0x28
#define WM8960_REG_ROUT2_VOLUME 0x29
#define WM8960_REG_MONO_OUT_VOLUME 0x2A
#define WM8960_REG_INPUT_BOOST_MIXER_1 0x2B
#define WM8960_REG_INPUT_BOOST_MIXER_2 0x2C
#define WM8960_REG_BYPASS_1 0x2D
#define WM8960_REG_BYPASS_2 0x2E
#define WM8960_REG_PWR_MGMT_3 0x2F
#define WM8960_REG_ADDITIONAL_CONTROL_4 0x30
#define WM8960_REG_CLASS_D_CONTROL_1 0x31
#define WM8960_REG_CLASS_D_CONTROL_3 0x33
#define WM8960_REG_PLL_N 0x34
#define WM8960_REG_PLL_K_1 0x35
#define WM8960_REG_PLL_K_2 0x36
#define WM8960_REG_PLL_K_3 0x37

// PGA input selections
#define WM8960_PGAL_LINPUT2 0
#define WM8960_PGAL_LINPUT3 1
#define WM8960_PGAL_VMID 2
#define WM8960_PGAR_RINPUT2 0
#define WM8960_PGAR_RINPUT3 1
#define WM8960_PGAR_VMID 2

// Mic (aka PGA) BOOST gain options
#define WM8960_MIC_BOOST_GAIN_0DB 0
#define WM8960_MIC_BOOST_GAIN_13DB 1
#define WM8960_MIC_BOOST_GAIN_20DB 2
#define WM8960_MIC_BOOST_GAIN_29DB 3

// Boost Mixer gain options
#define WM8960_BOOST_MIXER_GAIN_MUTE 0
#define WM8960_BOOST_MIXER_GAIN_NEG_12DB 1
#define WM8960_BOOST_MIXER_GAIN_NEG_9DB 2
#define WM8960_BOOST_MIXER_GAIN_NEG_6DB 3
#define WM8960_BOOST_MIXER_GAIN_NEG_3DB 4
#define WM8960_BOOST_MIXER_GAIN_0DB 5
#define WM8960_BOOST_MIXER_GAIN_3DB 6
#define WM8960_BOOST_MIXER_GAIN_6DB 7

// Output Mixer gain options
#define WM8960_OUTPUT_MIXER_GAIN_0DB 0
#define WM8960_OUTPUT_MIXER_GAIN_NEG_3DB 1
#define WM8960_OUTPUT_MIXER_GAIN_NEG_6DB 2
#define WM8960_OUTPUT_MIXER_GAIN_NEG_9DB 3
#define WM8960_OUTPUT_MIXER_GAIN_NEG_12DB 4
#define WM8960_OUTPUT_MIXER_GAIN_NEG_15DB 5
#define WM8960_OUTPUT_MIXER_GAIN_NEG_18DB 6
#define WM8960_OUTPUT_MIXER_GAIN_NEG_21DB 7

// Mic Bias voltage options
#define WM8960_MIC_BIAS_VOLTAGE_0_9_AVDD 0
#define WM8960_MIC_BIAS_VOLTAGE_0_65_AVDD 1

// SYSCLK divide
#define WM8960_SYSCLK_DIV_BY_1 0
#define WM8960_SYSCLK_DIV_BY_2 2
#define WM8960_CLKSEL_MCLK 0
#define WM8960_CLKSEL_PLL 1
#define WM8960_PLL_MODE_INTEGER 0
#define WM8960_PLL_MODE_FRACTIONAL 1
#define WM8960_PLLPRESCALE_DIV_1 0
#define WM8960_PLLPRESCALE_DIV_2 1

// Class d clock divide
#define WM8960_DCLKDIV_16 7

// Word length settings (aka bits per sample)
#define WM8960_WL_16BIT 0
#define WM8960_WL_20BIT 1
#define WM8960_WL_24BIT 2
#define WM8960_WL_32BIT 3

#define WM8960_LR_POLARITY_NORMAL 0
#define WM8960_LR_POLARITY_INVERT 1

#define WM8960_ALRSWAP_NORMAL 0
#define WM8960_ALRSWAP_SWAP 1

#define WM8960_PGA_GAIN_MIN -17.25
#define WM8960_PGA_GAIN_MAX 30.00
#define WM8960_PGA_GAIN_OFFSET 17.25
#define WM8960_PGA_GAIN_STEPSIZE 0.75
#define WM8960_HP_GAIN_MIN -73.00
#define WM8960_HP_GAIN_MAX 6.00
#define WM8960_HP_GAIN_OFFSET 121.00
#define WM8960_HP_GAIN_STEPSIZE 1.00
#define WM8960_SPEAKER_GAIN_MIN -73.00
#define WM8960_SPEAKER_GAIN_MAX 6.00
#define WM8960_SPEAKER_GAIN_OFFSET 121.00
#define WM8960_SPEAKER_GAIN_STEPSIZE 1.00
#define WM8960_ADC_GAIN_MIN -97.00
#define WM8960_ADC_GAIN_MAX 30.00
#define WM8960_ADC_GAIN_OFFSET 97.50
#define WM8960_ADC_GAIN_STEPSIZE 0.50
#define WM8960_DAC_GAIN_MIN -97.00
#define WM8960_DAC_GAIN_MAX 30.00
#define WM8960_DAC_GAIN_OFFSET 97.50
#define WM8960_DAC_GAIN_STEPSIZE 0.50

// Automatic Level Control Modes
#define WM8960_ALC_MODE_OFF 0
#define WM8960_ALC_MODE_RIGHT_ONLY 1
#define WM8960_ALC_MODE_LEFT_ONLY 2
#define WM8960_ALC_MODE_STEREO 3

#define WM8960_ALC_TARGET_LEVEL_NEG_22_5DB 0
#define WM8960_ALC_TARGET_LEVEL_NEG_21DB 1
#define WM8960_ALC_TARGET_LEVEL_NEG_19_5DB 2
#define WM8960_ALC_TARGET_LEVEL_NEG_18DB 3
#define WM8960_ALC_TARGET_LEVEL_NEG_16_5DB 4
#define WM8960_ALC_TARGET_LEVEL_NEG_15DB 5
#define WM8960_ALC_TARGET_LEVEL_NEG_13_5DB 6
#define WM8960_ALC_TARGET_LEVEL_NEG_12DB 7
#define WM8960_ALC_TARGET_LEVEL_NEG_10_5DB 8
#define WM8960_ALC_TARGET_LEVEL_NEG_9DB 9
#define WM8960_ALC_TARGET_LEVEL_NEG_7_5DB 10
#define WM8960_ALC_TARGET_LEVEL_NEG_6DB 11
#define WM8960_ALC_TARGET_LEVEL_NEG_4_5DB 12
#define WM8960_ALC_TARGET_LEVEL_NEG_3DB 13
#define WM8960_ALC_TARGET_LEVEL_NEG_1_5DB 14

#define WM8960_ALC_MAX_GAIN_LEVEL_NEG_12DB 0
#define WM8960_ALC_MAX_GAIN_LEVEL_NEG_6DB 1
#define WM8960_ALC_MAX_GAIN_LEVEL_0DB 2
#define WM8960_ALC_MAX_GAIN_LEVEL_6DB 3
#define WM8960_ALC_MAX_GAIN_LEVEL_12DB 4
#define WM8960_ALC_MAX_GAIN_LEVEL_18DB 5
#define WM8960_ALC_MAX_GAIN_LEVEL_24DB 6
#define WM8960_ALC_MAX_GAIN_LEVEL_30DB 7

#define WM8960_ALC_MIN_GAIN_LEVEL_NEG_17_25DB 0
#define WM8960_ALC_MIN_GAIN_LEVEL_NEG_11_25DB 1
#define WM8960_ALC_MIN_GAIN_LEVEL_NEG_5_25DB 2
#define WM8960_ALC_MIN_GAIN_LEVEL_0_75DB 3
#define WM8960_ALC_MIN_GAIN_LEVEL_6_75DB 4
#define WM8960_ALC_MIN_GAIN_LEVEL_12_75DB 5
#define WM8960_ALC_MIN_GAIN_LEVEL_18_75DB 6
#define WM8960_ALC_MIN_GAIN_LEVEL_24_75DB 7

#define WM8960_ALC_HOLD_TIME_0MS 0
#define WM8960_ALC_HOLD_TIME_3MS 1
#define WM8960_ALC_HOLD_TIME_5MS 2
#define WM8960_ALC_HOLD_TIME_11MS 3
#define WM8960_ALC_HOLD_TIME_21MS 4
#define WM8960_ALC_HOLD_TIME_43MS 5
#define WM8960_ALC_HOLD_TIME_85MS 6
#define WM8960_ALC_HOLD_TIME_170MS 7
#define WM8960_ALC_HOLD_TIME_341MS 8
#define WM8960_ALC_HOLD_TIME_682MS 9
#define WM8960_ALC_HOLD_TIME_1365MS 10
#define WM8960_ALC_HOLD_TIME_3SEC 11
#define WM8960_ALC_HOLD_TIME_5SEC 12
#define WM8960_ALC_HOLD_TIME_10SEC 13
#define WM8960_ALC_HOLD_TIME_23SEC 14
#define WM8960_ALC_HOLD_TIME_44SEC 15

#define WM8960_ALC_DECAY_TIME_24MS 0
#define WM8960_ALC_DECAY_TIME_48MS 1
#define WM8960_ALC_DECAY_TIME_96MS 2
#define WM8960_ALC_DECAY_TIME_192MS 3
#define WM8960_ALC_DECAY_TIME_384MS 4
#define WM8960_ALC_DECAY_TIME_768MS 5
#define WM8960_ALC_DECAY_TIME_1536MS 6
#define WM8960_ALC_DECAY_TIME_3SEC 7
#define WM8960_ALC_DECAY_TIME_6SEC 8
#define WM8960_ALC_DECAY_TIME_12SEC 9
#define WM8960_ALC_DECAY_TIME_24SEC 10

#define WM8960_ALC_ATTACK_TIME_6MS 0
#define WM8960_ALC_ATTACK_TIME_12MS 1
#define WM8960_ALC_ATTACK_TIME_24MS 2
#define WM8960_ALC_ATTACK_TIME_482MS 3
#define WM8960_ALC_ATTACK_TIME_964MS 4
#define WM8960_ALC_ATTACK_TIME_1928MS 5
#define WM8960_ALC_ATTACK_TIME_3846MS 6
#define WM8960_ALC_ATTACK_TIME_768MS 7
#define WM8960_ALC_ATTACK_TIME_1536MS 8
#define WM8960_ALC_ATTACK_TIME_3SEC 9
#define WM8960_ALC_ATTACK_TIME_6SEC 10

#define WM8960_SPEAKER_BOOST_GAIN_0DB 0
#define WM8960_SPEAKER_BOOST_GAIN_2_1DB 1
#define WM8960_SPEAKER_BOOST_GAIN_2_9DB 2
#define WM8960_SPEAKER_BOOST_GAIN_3_6DB 3
#define WM8960_SPEAKER_BOOST_GAIN_4_5DB 4
#define WM8960_SPEAKER_BOOST_GAIN_5_1DB 5

#define WM8960_VMIDSEL_DISABLED 0
#define WM8960_VMIDSEL_2X50KOHM 1
#define WM8960_VMIDSEL_2X250KOHM 2
#define WM8960_VMIDSEL_2X5KOHM 3

#define WM8960_VROI_500 0
#define WM8960_VROI_20K 1

#define WM8960_VSEL_INCREASED_BIAS_CURRENT 1
#define WM8960_VSEL_LOWEST_BIAS_CURRENT 3

#define WM8960_JACKDETECT_GPIO1 0
#define WM8960_JACKDETECT_LINPUT3 1
#define WM8960_JACKDETECT_RINPUT3 2

class WM8960
{
	public:
		WM8960();
		bool begin(i2c_port_t wirePort = I2C_NUM_0);
		bool isConnected();

		bool enableVREF(); 
		bool disableVREF(); 

		bool reset(); 

		bool enableAINL();
		bool disableAINL();
		bool enableAINR();
		bool disableAINR();

		bool enableLMIC();
		bool disableLMIC();
		bool enableRMIC();
		bool disableRMIC();

		bool enableLMICBOOST();
		bool disableLMICBOOST();
		bool enableRMICBOOST();
		bool disableRMICBOOST();

		bool pgaLeftNonInvSignalSelect(uint8_t signal); 
		bool pgaRightNonInvSignalSelect(uint8_t signal); 

		bool connectLMN1(); 
		bool disconnectLMN1(); 	

		bool connectRMN1(); 	
		bool disconnectRMN1(); 		

		bool connectLMIC2B(); 		
		bool disconnectLMIC2B();	

		bool connectRMIC2B(); 	
		bool disconnectRMIC2B();	

		bool setLINVOL(uint8_t volume); 
		bool setLINVOLDB(float dB);

		bool setRINVOL(uint8_t volume); 
		bool setRINVOLDB(float dB);

		bool enablePgaZeroCross(); 
		bool disablePgaZeroCross(); 

		bool enableLINMUTE();
		bool disableLINMUTE();
		bool enableRINMUTE();
		bool disableRINMUTE();

		bool pgaLeftIPVUSet(); 
		bool pgaRightIPVUSet(); 

		bool setLMICBOOST(uint8_t boost_gain); 
		bool setRMICBOOST(uint8_t boost_gain); 

		bool setLIN3BOOST(uint8_t boost_gain); 
		bool setLIN2BOOST(uint8_t boost_gain); 
		bool setRIN3BOOST(uint8_t boost_gain); 
		bool setRIN2BOOST(uint8_t boost_gain); 

		bool enableMicBias();
		bool disableMicBias();

		bool setMicBiasVoltage(bool voltage); 

		bool enableAdcLeft();
		bool disableAdcLeft();
		bool enableAdcRight();
		bool disableAdcRight();

		bool setAdcLeftDigitalVolume(uint8_t volume); 
		bool setAdcRightDigitalVolume(uint8_t volume);
		bool setAdcLeftDigitalVolumeDB(float dB); 
		bool setAdcRightDigitalVolumeDB(float dB);

		bool adcLeftADCVUSet(); 
		bool adcRightADCVUSet(); 

		bool enableAlc(uint8_t mode = WM8960_ALC_MODE_STEREO); 
		bool disableAlc();

		bool setAlcTarget(uint8_t target); 
		bool setAlcDecay(uint8_t decay); 
		bool setAlcAttack(uint8_t attack); 
		bool setAlcMaxGain(uint8_t maxGain);
		bool setAlcMinGain(uint8_t attack); 
		bool setAlcHold(uint8_t attack); 

		bool enablePeakLimiter();
		bool disablePeakLimiter();

		bool enableNoiseGate();
		bool disableNoiseGate();
		bool setNoiseGateThreshold(uint8_t threshold); 

		bool enableDacLeft();
		bool disableDacLeft();
		bool enableDacRight();
		bool disableDacRight();

		bool setDacLeftDigitalVolume(uint8_t volume); 
		bool setDacRightDigitalVolume(uint8_t volume);	
		bool setDacLeftDigitalVolumeDB(float dB); 
		bool setDacRightDigitalVolumeDB(float dB);	

		bool dacLeftDACVUSet(); 
		bool dacRightDACVUSet(); 

		bool enableDacMute();
		bool disableDacMute();

		bool enable3d();
		bool disable3d();
		bool set3dDepth(uint8_t depth); 

		bool enableDac6dbAttenuation();
		bool disableDac6dbAttentuation();

		bool enableLOMIX();
		bool disableLOMIX();
		bool enableROMIX();
		bool disableROMIX();
		bool enableOUT3MIX();
		bool disableOUT3MIX();

		bool enableLI2LO();
		bool disableLI2LO();

		bool setLI2LOVOL(uint8_t volume); 

		bool enableLB2LO();
		bool disableLB2LO();

		bool setLB2LOVOL(uint8_t volume); 

		bool enableLD2LO();
		bool disableLD2LO();

		bool enableRI2RO();
		bool disableRI2RO();

		bool setRI2ROVOL(uint8_t volume); 

		bool enableRB2RO();
		bool disableRB2RO();

		bool setRB2ROVOL(uint8_t volume); 

		bool enableRD2RO();
		bool disableRD2RO();

		bool enableLI2MO();
		bool disableLI2MO();
		bool enableRI2MO();
		bool disableRI2MO();

		bool enableOUT3asVMID(); 

		bool enableVMID(); 
		bool disableVMID();
		bool setVMID(uint8_t setting = WM8960_VMIDSEL_2X50KOHM);

		bool enableHeadphones();
		bool disableHeadphones();
		bool enableRightHeadphone();
		bool disableRightHeadphone();
		bool enableLeftHeadphone();
		bool disableLeftHeadphone();

		bool enableHeadphoneStandby();
		bool disableHeadphoneStandby();

		bool setHeadphoneVolume(uint8_t volume); 
		bool enableHeadphoneZeroCross(); 
		bool disableHeadphoneZeroCross();
		bool setHeadphoneVolumeDB(float dB);
		
		bool enableHeadphoneJackDetect();
		bool disableHeadphoneJackDetect();
		bool setHeadphoneJackDetectInput(uint8_t setting = WM8960_JACKDETECT_LINPUT3);

		bool enableSpeakers();
		bool disableSpeakers();
		bool enableRightSpeaker();
		bool disableRightSpeaker();
		bool enableLeftSpeaker();
		bool disableLeftSpeaker();

		bool setSpeakerVolume(uint8_t volume); 
		bool setSpeakerVolumeDB(float dB);

		bool enableSpeakerZeroCross(); 
		bool disableSpeakerZeroCross();	

		bool setSpeakerDcGain(uint8_t gain);
		bool setSpeakerAcGain(uint8_t gain);

		bool enableLoopBack();
		bool disableLoopBack();

		bool enablePLL();
		bool disablePLL();

		bool setPLLPRESCALE(bool div); 
		bool setPLLN(uint8_t n);
		bool setPLLK(uint8_t one, uint8_t two, uint8_t three); 

		bool setSMD(bool mode); 
		bool setCLKSEL(bool sel); 

		bool setSYSCLKDIV(uint8_t div); 
		bool setADCDIV(uint8_t div); 
		bool setDACDIV(uint8_t div); 
		bool setBCLKDIV(uint8_t div); 
		bool setDCLKDIV(uint8_t div); 

		bool setALRCGPIO(); 

		bool enableMasterMode();
		bool enablePeripheralMode();

		bool setWL(uint8_t word_length);
		bool setLRP(bool polarity);
		bool setALRSWAP(bool swap);
		bool setVROI(bool setting);
		bool setVSEL(uint8_t setting);

		bool writeRegister(uint8_t reg, uint16_t value);

	private:
		i2c_port_t _i2cPort;
		uint8_t _deviceAddress = WM8960_ADDR;
		bool _writeRegisterBit(uint8_t registerAddress, uint8_t bitNumber, bool bitValue);
		bool _writeRegisterMultiBits(uint8_t registerAddress, uint8_t settingMsbNum, uint8_t settingLsbNum, uint8_t setting);
		uint8_t convertDBtoSetting(float dB, float offset, float stepSize, float minDB, float maxDB);

		uint16_t _registerLocalCopy[56] = {
			0x0097, 0x0097, 0x0000, 0x0000, 0x0000, 0x0008, 0x0000, 0x000A, 
			0x01C0, 0x0000, 0x00FF, 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 
			0x0000, 0x007B, 0x0100, 0x0032, 0x0000, 0x00C3, 0x00C3, 0x01C0, 
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
			0x0100, 0x0100, 0x0050, 0x0000, 0x0000, 0x0050, 0x0000, 0x0000, 
			0x0000, 0x0000, 0x0040, 0x0000, 0x0000, 0x0050, 0x0050, 0x0000, 
			0x0002, 0x0037, 0x0000, 0x0080, 0x0008, 0x0031, 0x0026, 0x00e9, 
		};

		const uint16_t _registerDefaults[56] = {
			0x0097, 0x0097, 0x0000, 0x0000, 0x0000, 0x0008, 0x0000, 0x000A, 
			0x01C0, 0x0000, 0x00FF, 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 
			0x0000, 0x007B, 0x0100, 0x0032, 0x0000, 0x00C3, 0x00C3, 0x01C0, 
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
			0x0100, 0x0100, 0x0050, 0x0000, 0x0000, 0x0050, 0x0000, 0x0000, 
			0x0000, 0x0000, 0x0040, 0x0000, 0x0000, 0x0050, 0x0050, 0x0000, 
			0x0002, 0x0037, 0x0000, 0x0080, 0x0008, 0x0031, 0x0026, 0x00e9, 
		};		
};
#endif