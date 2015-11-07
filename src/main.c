#include <ch.h>
#include <hal.h>
#include <chprintf.h>
#include "usbcfg.h"
#include "can_bridge.h"

void panic(const char *reason)
{
    (void) reason;
    while (1);
}

SerialUSBDriver SDU1, SDU2;

int main(void)
{
    halInit();
    chSysInit();

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

    can_bridge_start((BaseSequentialStream *)&SDU1);

    while (1) {
        chThdSleepMilliseconds(100);
        led_toggle(STATUS_LED);
    }
}
