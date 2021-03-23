#include <ch.h>
#include <hal.h>
#include <stddef.h>
#include "can_driver.h"
#include "slcan.h"
#include "slcan_thread.h"

char* slcan_getline(void* arg)
{
    static char line_buffer[500];
    static size_t pos = 0;
    size_t i;
    for (i = pos; i < sizeof(line_buffer); i++) {
        int c = chnGetTimeout((BaseChannel*)arg, TIME_INFINITE);
        if (c == STM_TIMEOUT) {
            /* no more data, continue */
            pos = i;
            return NULL;
        }
        if (c == '\n' || c == '\r' || c == '\0') {
            /* line found */
            line_buffer[i] = 0;
            pos = 0;
            led_set(STATUS_LED); // show USB activity
            return line_buffer;
        } else {
            line_buffer[i] = c;
        }
    }

    /* reset */
    pos = 0;
    return NULL;
}

MUTEX_DECL(serial_lock);

int slcan_serial_write(void* arg, const char* buf, size_t len)
{
    if (len == 0) {
        return 0;
    }
    chMtxLock(&serial_lock);
    int ret = chnWriteTimeout((BaseChannel*)arg, (const uint8_t*)buf, len, MS2ST(100));
    chMtxUnlock(&serial_lock);
    return ret;
}

THD_WORKING_AREA(slcan_thread, 1000);
void slcan_thread_main(void* arg)
{
    chRegSetThreadName("USB receiver");
    while (1) {
        slcan_spin(arg);
    }
}

void slcan_rx_spin(void* arg);

THD_WORKING_AREA(slcan_rx_thread, 1000);
void slcan_rx_thread_main(void* arg)
{
    chRegSetThreadName("CAN receiver");
    while (1) {
        slcan_rx_spin(arg);
    }
}

void slcan_start(BaseChannel* ch)
{
    can_init();
    chThdCreateStatic(slcan_thread, sizeof(slcan_thread), NORMALPRIO, slcan_thread_main, ch);
    chThdCreateStatic(slcan_rx_thread, sizeof(slcan_rx_thread), NORMALPRIO, slcan_rx_thread_main, ch);
}
