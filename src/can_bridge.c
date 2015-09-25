#include <ch.h>
#include <hal.h>
#include "can_driver.h"
#include <serial-can-bridge/serial_can_bridge.h>
#include <serial-datagram/serial_datagram.h>

void serial_write(void *arg, const void *p, size_t len)
{
    if (len > 0) {
        BaseAsynchronousChannel *chp = (BaseAsynchronousChannel *)arg;
        chnWrite(chp, p, len);
    }
}

THD_WORKING_AREA(can_bridge_rx_thread_wa, 1000);
void can_bridge_rx_thread(void *arg)
{
    chRegSetThreadName("can_bridge_rx");

    struct can_frame *framep;
    static uint8_t outbuf[32];
    size_t outlen;
    while (1) {
        msg_t m = chMBFetch(&can_rx_queue, (msg_t *)&framep, TIME_INFINITE);
        if (m == MSG_OK) {
            // encode frame
            outlen = sizeof(outbuf);
            bool ok = can_bridge_frame_write(framep, outbuf, &outlen);
            chPoolFree(&can_rx_pool, framep);
            if (ok) {
                serial_datagram_send(outbuf, outlen, serial_write, arg);
            }
        } else if (m == MSG_RESET) {
            // send data dropped notification
        }
    }
}

THD_WORKING_AREA(can_bridge_tx_thread_wa, 1000);
void can_bridge_tx_thread(void *arg)
{
    serial_datagram_rcv_handler_t rcv;
    static uint8_t buf[32];
    static char datagram_buf[64];
    BaseAsynchronousChannel *in = (BaseAsynchronousChannel *)arg;

    serial_datagram_rcv_handler_init(
        &rcv,
        datagram_buf,
        sizeof(datagram_buf),
        can_bridge_datagram_rcv_cb,
        NULL);

    while (1) {
        int len = chnReadTimeout(in, buf, sizeof(buf), MS2ST(10));
        if (len == 0) {
            continue;
        }
        serial_datagram_receive(&rcv, buf, len);
    }
}

void can_interface_send(struct can_frame *frame)
{
    struct can_frame *tx = (struct can_frame *)chPoolAlloc(&can_tx_pool);
    if (tx == NULL) {
        return;
    }
    tx->id = frame->id;
    tx->dlc = frame->dlc;
    tx->data.u32[0] = frame->data.u32[0];
    tx->data.u32[1] = frame->data.u32[1];
    msg_t m = chMBPost(&can_tx_queue, (msg_t)tx, TIME_IMMEDIATE);
    if (m != MSG_OK) {
        // couldn't post, free memory
        chPoolFree(&can_tx_pool, tx);
        chMBReset(&can_tx_queue);
    }
    return;
}

void can_bridge_start(BaseSequentialStream *io)
{
    can_driver_start();
    chThdCreateStatic(can_bridge_rx_thread_wa, sizeof(can_bridge_rx_thread_wa), NORMALPRIO, can_bridge_rx_thread, io);
    chThdCreateStatic(can_bridge_tx_thread_wa, sizeof(can_bridge_tx_thread_wa), NORMALPRIO, can_bridge_tx_thread, io);
}
