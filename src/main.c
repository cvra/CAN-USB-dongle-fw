#include <ch.h>
#include <hal.h>
#include <chprintf.h>
#include "usbcfg.h"
#include "can_bridge.h"
#include "uart_bridge.h"
#include "protocol.h"
#include <timestamp/timestamp_stm32.h>

void panic(const char *reason)
{
    (void) reason;
    led_set(STATUS_LED | CAN1_STATUS_LED | CAN1_PWR_LED);
    palSetPad(GPIOA, GPIOA_CAN_SILENT); // CAN silent
    palClearPad(GPIOB, GPIOB_V_BUS_ENABLE); // bus power disable
    while (1);
}

bool bus_power(bool enable)
{
    // todo: check bus voltage first
    if (enable) {
        led_set(CAN1_PWR_LED);
        palSetPad(GPIOB, GPIOB_V_BUS_ENABLE);
    } else {
        led_clear(CAN1_PWR_LED);
        palClearPad(GPIOB, GPIOB_V_BUS_ENABLE);
    }
    return true;
}

float bus_voltage_get(void)
{
    return 0;
}

SerialUSBDriver SDU1, SDU2;

int main(void)
{
    halInit();
    chSysInit();

    chSysLock();
    timestamp_stm32_init();
    chSysUnlock();

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
        led_toggle(STATUS_LED);
        chThdSleepMilliseconds(100);
    }
}
