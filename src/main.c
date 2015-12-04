#include <ch.h>
#include <hal.h>
#include <chprintf.h>
#include "usbcfg.h"
#include "can_bridge.h"
#include "uart_bridge.h"
#include "protocol.h"
#include "bus_power.h"
#include <timestamp/timestamp.h>
#include <timestamp/timestamp_stm32.h>

void panic(const char *reason)
{
    (void) reason;
    led_set(STATUS_LED | CAN1_STATUS_LED | CAN1_PWR_LED);
    palSetPad(GPIOA, GPIOA_CAN_SILENT); // CAN silent
    palClearPad(GPIOB, GPIOB_V_BUS_ENABLE); // bus power disable
    while (1);
}

void user_button_poll(void)
{
    static timestamp_t last_press = 0;
    static bool active = false;
    if (palReadPad(GPIOA, GPIOA_USER_BUTTON) != 0) {
        if (active && 1.0f < timestamp_duration_s(last_press, timestamp_get())) {
            bus_power_toggle();
            active = false;
        } else if (!active) {
            active = true;
            last_press = timestamp_get();
        }
    } else {
        active = false;
    }
}

void crc32_stm32f3_init(void)
{
    chSysLock();
    rccEnableAHB(RCC_AHBENR_CRCEN, 0);
    chSysUnlock();
    CRC->CR = 0;
    CRC->CR = CRC_CR_REV_OUT | CRC_CR_REV_IN_0 | CRC_CR_REV_IN_1; // bit reverse
    CRC->POL = 0x04C11DB7;
}

uint32_t crc32(uint32_t init, const void *data, size_t len)
{
    CRC->INIT = ~init;
    // word aling pointer
    uint8_t align = ((uintptr_t)data) & 3;
    const uint8_t *b = data;
    while (align > 0) {
        *(uint8_t *)(&CRC->DR) = *b++;
        align--;
    }
    len -= align;
    // calculate by word
    const uint32_t *w = (uint32_t *)b;
    while (len >= 4) {
        CRC->DR = *w++;
        len -= 4;
    }
    // calculate rest
    b = (uint8_t *)w;
    while (len--) {
        *(uint8_t *)(&CRC->DR) = *b++;
    }
    return ~CRC->DR;
}

SerialUSBDriver SDU1, SDU2;

int main(void)
{
    halInit();
    chSysInit();

    chSysLock();
    timestamp_stm32_init();
    chSysUnlock();

    bus_power_init();
    crc32_stm32f3_init();

    // USB CDC
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg1);
    sduObjectInit(&SDU2);
    sduStart(&SDU2, &serusbcfg2);

    usbDisconnectBus(serusbcfg1.usbp);
    chThdSleepMilliseconds(1500);
    usbStart(serusbcfg1.usbp, &usbcfg);
    usbConnectBus(serusbcfg1.usbp);

    while (SDU1.config->usbp->state != USB_ACTIVE) {
        chThdSleepMilliseconds(10);
    }

    uart_bridge_start((BaseChannel *)&SDU2);
    can_bridge_start((BaseChannel *)&SDU1);

    while (1) {
        user_button_poll();
        bus_voltage_adc_conversion();
        chThdSleepMilliseconds(100);
    }
}
