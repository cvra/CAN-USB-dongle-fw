#include <ch.h>
#include <hal.h>
#include <chprintf.h>
#include "usbcfg.h"

#include <string.h>

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

void panic(const char *reason)
{
    (void) reason;
    while (1);
}

static const CANConfig can1_config = {
    .mcr = (1 << 6)  /* Automatic bus-off management enabled. */
         | (1 << 2), /* Message are prioritized by order of arrival. */
    /* APB Clock is 36 Mhz
       36MHz / 2 / (1tq + 10tq + 7tq) = 1MHz => 1Mbit */
    .btr = (1 << 0)  /* Baudrate prescaler (10 bits) */
         | (9 << 16)/* Time segment 1 (3 bits) */
         | (6 << 20) /* Time segment 2 (3 bits) */
         | (0 << 24) /* Resync jump width (2 bits) */
};

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
    chprintf((BaseSequentialStream *)&SDU1, "start\n");

    // CAN gpio init
    iomode_t mode = PAL_STM32_MODE_ALTERNATE | PAL_STM32_OTYPE_PUSHPULL
        | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING
        | PAL_STM32_ALTERNATE(9);
    palSetPadMode(GPIOB, GPIOB_PIN8, mode); // RX
    palSetPadMode(GPIOB, GPIOB_PIN9, mode); // TX

    canStart(&CAND1, &can1_config);

    while (1) {
        CANTxFrame txf;
        txf.SID = 42;
        txf.IDE = 0;
        txf.RTR = 0;
        txf.DLC = 4;
        memcpy(&txf.data8[0], "CVRA", 4);
        canTransmit(&CAND1, CAN_ANY_MAILBOX, &txf, MS2ST(100));
        chThdSleepMilliseconds(500);
    }
}
