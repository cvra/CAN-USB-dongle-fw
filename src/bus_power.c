#include <ch.h>
#include <hal.h>
#include "bus_power.h"

uint16_t bus_voltage = 0;
#define ADC_NUM_CHANNELS   1
#define ADC_BUF_DEPTH      1
static adcsample_t adc_samples[ADC_NUM_CHANNELS * ADC_BUF_DEPTH];
binary_semaphore_t adc_wait;

static void adc_callback(ADCDriver *adcp, adcsample_t *buffer, size_t n)
{
    (void)adcp;
    (void)n;
    bus_voltage = buffer[0];
    chSysLockFromISR();
    chBSemSignalI(&adc_wait);
    chSysUnlockFromISR();
}

static void adc_error_callback(ADCDriver *adcp, adcerror_t err)
{
    (void)adcp;
    (void)err;
    chSysHalt("ADC error");
}

/*
 * ADC conversion group.
 * Mode:        Continuous, 1 sample of 1 channel, SW triggered.
 * Channels:    IN3
 */
static const ADCConversionGroup adcgrpcfg = {
    false,
    ADC_NUM_CHANNELS,
    adc_callback,
    adc_error_callback,
    ADC_CFGR_CONT | ADC_CFGR_RES_12BITS, /* CFGR    */
    ADC_TR(0, 4095), /* TR1     */
    0, /* CCR     */
    { /* SMPR[2] */
        ADC_SMPR1_SMP_AN3(ADC_SMPR_SMP_601P5),
        0
    },
    { /* SQR[4]  */
        ADC_SQR1_SQ1_N(ADC_CHANNEL_IN3),
        0,
        0,
        0
    }
};

float bus_voltage_get(void)
{
    return ((float)bus_voltage)/((1<<12)-1)*3.3*2;
}

void bus_voltage_adc_conversion(void)
{
    // read bus voltage
    adcStartConversion(&ADCD1, &adcgrpcfg, adc_samples, ADC_BUF_DEPTH);
    chBSemWait(&adc_wait);

    // set LED
    float voltage = bus_voltage_get();
    if (voltage > 4.5f && voltage < 5.5f) {
        led_set(CAN1_PWR_LED);
    } else if (voltage > 0.5f || voltage > 5.5f) {
        // voltage warning
        led_toggle(CAN1_PWR_LED);
    } else {
        led_clear(CAN1_PWR_LED);
    }
}

bool bus_power(bool enable)
{
    if (enable) {
        if (bus_voltage_get() > 0.5 && palReadPad(GPIOB, GPIOB_V_BUS_ENABLE) == 0) {
            return false;
        }
        palSetPad(GPIOB, GPIOB_V_BUS_ENABLE);
    } else {
        led_clear(CAN1_PWR_LED);
        palClearPad(GPIOB, GPIOB_V_BUS_ENABLE);
    }
    return true;
}

void bus_power_toggle(void)
{
    if (palReadPad(GPIOB, GPIOB_V_BUS_ENABLE)) {
        bus_power(false);
    } else {
        bus_power(true);
    }
}

void bus_power_init(void)
{
    chBSemObjectInit(&adc_wait, true);
    adcStart(&ADCD1, NULL);
}
