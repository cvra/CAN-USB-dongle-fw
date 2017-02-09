#include <ch.h>
#include <hal.h>
#include "usbcfg.h"
#include "uart_bridge.h"
#include "bus_power.h"
#include "can_driver.h"
#include "slcan.h"
#include "slcan_thread.h"
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

SerialUSBDriver SDU1, SDU2;

int main(void)
{
    halInit();
    chSysInit();

    chSysLock();
    timestamp_stm32_init();
    chSysUnlock();

    bus_power_init();

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
    slcan_start((BaseChannel *)&SDU1);

    while (1) {
        user_button_poll();
        bus_voltage_adc_conversion();
        chThdSleepMilliseconds(100);
        led_clear(STATUS_LED);
        led_clear(CAN1_STATUS_LED);
    }
}
