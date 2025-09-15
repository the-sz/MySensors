#include "MyHwSTM32U083.h"

RTC_HandleTypeDef 						hrtc;
static RCC_ClkInitTypeDef				SavedRccClkInit;
static RCC_OscInitTypeDef				SavedRccOscInit;
static RCC_PeriphCLKInitTypeDef		SavedPeriphClkInit;
static uint32_t							SavedLatency;
extern __IO uint32_t 					uwTick;					// from C:\Users\<user>\AppData\Local\Arduino15\packages\STMicroelectronics\hardware\stm32\2.10.1\system\Drivers\STM32U0xx_HAL_Driver\Src\stm32u0xx_hal.c

bool hwInit(void)
{
#if !defined(MY_DISABLED_SERIAL)
	MY_SERIALDEVICE.begin(MY_BAUD_RATE);
#if defined(MY_GATEWAY_SERIAL)
	while (!MY_SERIALDEVICE) {}
#endif
#endif

	RCC_ClkInitTypeDef 				RCC_ClkInitStruct;
	RCC_OscInitTypeDef 				RCC_OscInitStruct;
	RCC_PeriphCLKInitTypeDef		PeriphClkInitStruct;

	// enable Power Control clock
	__HAL_RCC_PWR_CLK_ENABLE();

	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_PWREx_ControlVoltageScaling() failed.\n"));
	}

	// select MSI Oscillator as PLL source
	RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
	RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT ;
	RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_11;
	RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI; /* MSI = 48MHz */
	RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV8;
	RCC_OscInitStruct.PLL.PLLN            = 8;
	RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2; /* 24MHz */
	RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2; /* 24MHz */
	RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2; /* 24MHz */
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_RCC_OscConfig() 1 failed.\n"));
	}

	// select PLL as system clock source and configure the HCLK and PCLK1 clocks dividers
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1)!= HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_RCC_ClockConfig() failed.\n"));
	}

	// configure the RTC clock source
	// enable LSI Oscillator
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_RCC_OscConfig() 2 failed.\n"));
	}

	// select LSI as RTC clock source
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_RCCEx_PeriphCLKConfig() failed.\n"));
	}

	// enable the RTC peripheral Clock
	__HAL_RCC_RTC_ENABLE();
	__HAL_RCC_RTCAPB_CLK_ENABLE();

	// init rtc
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	if (HAL_RTC_Init(&hrtc) != HAL_OK)
	{
		DEBUG_OUTPUT(PSTR("hwInit: HAL_RTC_Init() failed.\n"));
	}

	// ensure that MSI is wake-up system clock
	__HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);

	// enable Ultra low power mode
	HAL_PWREx_EnableUltraLowPowerMode();

	// enable the fast wake up from Ultra low power mode
	HAL_PWREx_EnableInternalWakeUpLine();

/*xxx
	if (EEPROM.init() == EEPROM_OK) {
		uint16 cnt;
		EEPROM.count(&cnt);
		if(cnt>=EEPROM.maxcount()) {
			// tmp, WIP: format eeprom if full
			EEPROM.format();
		}
		return true;
	}
	return false;
*/
	return true;
}

void hwReadConfigBlock(void *buf, void *addr, size_t length)
{
	uint8_t *dst = static_cast<uint8_t *>(buf);
	int pos = reinterpret_cast<int>(addr);
	while (length-- > 0) {
		*dst++ = EEPROM.read(pos++);
	}
}

void hwWriteConfigBlock(void *buf, void *addr, size_t length)
{
	uint8_t *src = static_cast<uint8_t *>(buf);
	int pos = reinterpret_cast<int>(addr);
	while (length-- > 0) {
		EEPROM.write(pos++, *src++);
	}
}

uint8_t hwReadConfig(const int addr)
{
	uint8_t value;
	hwReadConfigBlock(&value, reinterpret_cast<void *>(addr), 1);
	return value;
}

void hwWriteConfig(const int addr, uint8_t value)
{
	hwWriteConfigBlock(&value, reinterpret_cast<void *>(addr), 1);
}

int8_t hwSleep(uint32_t ms)
{
	DEBUG_OUTPUT(PSTR("hwSleep() start.\n"));

	// get the oscillators configuration according to the internal RCC registers
	HAL_RCC_GetOscConfig(&SavedRccOscInit);
	// get the clock configuration according to the internal RCC registers
	HAL_RCC_GetClockConfig(&SavedRccClkInit, &SavedLatency);
	// get the peripheral clock configuration according to the internal RCC registers
	HAL_RCCEx_GetPeriphCLKConfig(&SavedPeriphClkInit);

	// disable all used wakeup source
	HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

	// disable systick
	HAL_SuspendTick();

	uint32_t wakeUpCounter = (32768 * ms) / 16 /* RTC_WAKEUPCLOCK_RTCCLK_DIV16 */ / 1000;
	do
	{
		/* max sleep time is 33s if wakeUpCounter is set to 0xFFFF:
		Wakeup Time Base = 16 /(~32 kHz RC) = ~0.5 ms
		Wakeup Time = 0.5 ms * WakeUpCounter
		Therefore, with wake-up counter =  0xFFFF  = 65,535
		Wakeup Time =  0.5 ms *  65,535 = ~ 33 sec. */
		uint32_t wakeUpCounterCurrent = wakeUpCounter;
		if (wakeUpCounterCurrent > 0xFFFF)
			wakeUpCounterCurrent = 0xFFFF;
		// enable wakeup source
		HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeUpCounterCurrent, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 1);

		// enter STOP 2 mode
		HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFE);

		wakeUpCounter -= wakeUpCounterCurrent;

	} while (wakeUpCounter > 0);

	// deactivate rtc wakeup interrupts
	HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

	// enable power clock
	__HAL_RCC_PWR_CLK_ENABLE();

	// restore system clock settings after wake-up from STOP
	// setup oscillator as before stop
	HAL_RCC_OscConfig(&SavedRccOscInit);
	// setup clocking as before stop
	HAL_RCC_ClockConfig(&SavedRccClkInit, SavedLatency);
	// setup peripheral clock as before stop
	HAL_RCCEx_PeriphCLKConfig(&SavedPeriphClkInit);

	// adjust tick counter for sleep'd time
	DEBUG_OUTPUT(PSTR("hwSleep() Adjust tick counter for %u ms.\n"), ms);
	uwTick += ms;

	// enable systick
	HAL_ResumeTick();

	return MY_WAKE_UP_BY_TIMER;
}

int8_t hwSleep(const uint8_t interrupt, const uint8_t mode, uint32_t ms)
{
	DEBUG_OUTPUT(PSTR("hwSleep() #2 not supported.\n"));

	// TODO: Not supported!
	(void)interrupt;
	(void)mode;
	(void)ms;
	return MY_SLEEP_NOT_POSSIBLE;
}

int8_t hwSleep(const uint8_t interrupt1, const uint8_t mode1, const uint8_t interrupt2,
               const uint8_t mode2,
               uint32_t ms)
{
	DEBUG_OUTPUT(PSTR("hwSleep() #3 not supported.\n"));

	// TODO: Not supported!
	(void)interrupt1;
	(void)mode1;
	(void)interrupt2;
	(void)mode2;
	(void)ms;
	return MY_SLEEP_NOT_POSSIBLE;
}

void hwRandomNumberInit(void)
{
/*xxx
	// use internal temperature sensor as noise source
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE;
	regs->SMPR1 |= ADC_SMPR1_SMP16;

	uint32_t seed = 0;
	uint16_t currentValue = 0;
	uint16_t newValue = 0;

	for (uint8_t i = 0; i < 32; i++) {
		const uint32_t timeout = hwMillis() + 20;
		while (timeout >= hwMillis()) {
			newValue = adc_read(ADC1, 16);
			if (newValue != currentValue) {
				currentValue = newValue;
				break;
			}
		}
		seed ^= ( (newValue + hwMillis()) & 7) << i;
	}
	randomSeed(seed);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
*/
}

bool hwUniqueID(unique_id_t *uniqueID)
{
	(void)memcpy((uint8_t *)uniqueID, (uint32_t *)0x1FFFF7E0, 16); // FlashID + ChipID
	return true;
}

uint16_t hwCPUVoltage(void)
{
/*xxx
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE; // enable VREFINT and temp sensor
	regs->SMPR1 =  ADC_SMPR1_SMP17; // sample rate for VREFINT ADC channel
	adc_calibrate(ADC1);

	const uint16_t vdd = adc_read(ADC1, 17);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
	return (uint16_t)(1200u * 4096u / vdd);
*/
	return 3000;
}

uint16_t hwCPUFrequency(void)
{
	return F_CPU/100000UL;
}

int8_t hwCPUTemperature(void)
{
/*xxx
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE; // enable VREFINT and Temperature sensor
	regs->SMPR1 |= ADC_SMPR1_SMP16 | ADC_SMPR1_SMP17;
	adc_calibrate(ADC1);

	//const uint16_t adc_temp = adc_read(ADC1, 16);
	//const uint16_t vref = 1200 * 4096 / adc_read(ADC1, 17);
	// calibrated at 25°C, ADC output = 1430mV, avg slope = 4.3mV / °C, increasing temp ~ lower voltage
	const int8_t temp = static_cast<int8_t>((1430.0 - (adc_read(ADC1, 16) * 1200 / adc_read(ADC1,
	                                        17))) / 4.3 + 25.0);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
	return (temp - MY_STM32F1_TEMPERATURE_OFFSET) / MY_STM32F1_TEMPERATURE_GAIN;
*/
	return 20;

}

uint16_t hwFreeMem(void)
{
	//Not yet implemented
	return FUNCTION_NOT_SUPPORTED;
}
