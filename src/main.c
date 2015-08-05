#include <ch.h>
#include <hal.h>
#include <chprintf.h>
#include "usbcfg.h"

static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
    (void)arg;
    chRegSetThreadName("blinker");
    while (true) {
        palClearPad(GPIOB, GPIOB_LED_GREEN);
        chThdSleepMilliseconds(500);
        palSetPad(GPIOB, GPIOB_LED_GREEN);
        chThdSleepMilliseconds(500);
    }
}

int main(void)
{
    halInit();
    chSysInit();

    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

    // USB CDC
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    // usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1500);
    usbStart(serusbcfg.usbp, &usbcfg);
    // usbConnectBus(serusbcfg.usbp);

    while (SDU1.config->usbp->state != USB_ACTIVE) {
        chThdSleepMilliseconds(10);
    }

    while (1) {
        chprintf((BaseSequentialStream *)&SDU1, "hello world!\n");
        chThdSleepMilliseconds(1000);
    }
}
