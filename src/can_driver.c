#include <ch.h>
#include <hal.h>
#include <serial-can-bridge/can_frame.h>
#include <serial-can-bridge/serial_can_bridge.h>
#include "can_bridge.h"
#include "can_driver.h"

#define CAN_RX_BUFFER_SIZE   10
#define CAN_TX_BUFFER_SIZE   300

memory_pool_t can_rx_pool;
memory_pool_t can_tx_pool;
mailbox_t can_rx_queue;
mailbox_t can_tx_queue;
msg_t rx_mbox_buf[CAN_RX_BUFFER_SIZE];
msg_t tx_mbox_buf[CAN_TX_BUFFER_SIZE];
struct can_frame rx_pool_buf[CAN_RX_BUFFER_SIZE];
struct can_frame tx_pool_buf[CAN_TX_BUFFER_SIZE];

static THD_WORKING_AREA(can_tx_thread_wa, 256);
static THD_FUNCTION(can_tx_thread, arg) {
    (void)arg;
    chRegSetThreadName("CAN tx");
    while (1) {
        struct can_frame *framep;
        msg_t m = chMBFetch(&can_tx_queue, (msg_t *)&framep, TIME_INFINITE);
        if (m != MSG_OK) {
            continue;
        }
        CANTxFrame txf;
        uint32_t id = framep->id;
        txf.RTR = 0;
        if (id & CAN_FRAME_EXT_FLAG) {
            txf.EID = id & CAN_FRAME_EXT_ID_MASK;
            txf.IDE = 1;
        } else {
            txf.SID = id & CAN_FRAME_STD_ID_MASK;
            txf.IDE = 0;
        }

        if (id & CAN_FRAME_RTR_FLAG) {
            txf.RTR = 1;
        }

        txf.DLC = framep->dlc;
        txf.data32[0] = framep->data.u32[0];
        txf.data32[1] = framep->data.u32[1];

        chPoolFree(&can_tx_pool, framep);
        canTransmit(&CAND1, CAN_ANY_MAILBOX, &txf, MS2ST(100));
    }
}

static THD_WORKING_AREA(can_rx_thread_wa, 256);
static THD_FUNCTION(can_rx_thread, arg) {
    (void)arg;
    chRegSetThreadName("CAN rx");
    while (1) {
        uint32_t id;
        CANRxFrame rxf;
        msg_t m = canReceive(&CAND1, CAN_ANY_MAILBOX, &rxf, MS2ST(1000));
        if (m != MSG_OK) {
            continue;
        }
        if (rxf.IDE) {
            id = rxf.EID | CAN_FRAME_EXT_FLAG;
        } else {
            id = rxf.SID;
        }
        if (rxf.RTR) {
            id |= CAN_FRAME_RTR_FLAG;
        }
        if (!can_bridge_id_passes_filter(id)) {
            continue;
        }
        struct can_frame *f = (struct can_frame *)chPoolAlloc(&can_rx_pool);
        if (f == NULL) {
            continue;
        }
        f->id = id;
        f->dlc = rxf.DLC;
        f->data.u32[0] = rxf.data32[0];
        f->data.u32[1] = rxf.data32[1];

        m = chMBPost(&can_rx_queue, (msg_t)f, TIME_INFINITE);
        if (m != MSG_OK) {
            // buffer full -> reset rx buffer
            chMBReset(&can_rx_queue);
            chPoolFree(&can_rx_pool, f);
        }
    }
}

static const CANConfig can_config = {
    .mcr = (1 << 6)  /* Automatic bus-off management enabled. */
         | (1 << 2), /* Message are prioritized by order of arrival. */
    /* APB Clock is 36 Mhz
       36MHz / 2 / (1tq + 10tq + 7tq) = 1MHz => 1Mbit */
    .btr = (1 << 0)  /* Baudrate prescaler (10 bits) */
         | (9 << 16)/* Time segment 1 (3 bits) */
         | (6 << 20) /* Time segment 2 (3 bits) */
         | (0 << 24) /* Resync jump width (2 bits) */
};

void can_driver_start(void)
{
    // only accept standard CAN frames, as default
    can_bridge_filter_mask = CAN_FRAME_EXT_FLAG;
    can_bridge_filter_id = 0;

    // rx queue
    chMBObjectInit(&can_rx_queue, rx_mbox_buf, CAN_RX_BUFFER_SIZE);
    chPoolObjectInit(&can_rx_pool, sizeof(struct can_frame), NULL);
    chPoolLoadArray(&can_rx_pool, rx_pool_buf, sizeof(rx_pool_buf)/sizeof(struct can_frame));

    // tx queue
    chMBObjectInit(&can_tx_queue, tx_mbox_buf, CAN_TX_BUFFER_SIZE);
    chPoolObjectInit(&can_tx_pool, sizeof(struct can_frame), NULL);
    chPoolLoadArray(&can_tx_pool, tx_pool_buf, sizeof(tx_pool_buf)/sizeof(struct can_frame));

    // CAN gpio init
    iomode_t mode = PAL_STM32_MODE_ALTERNATE | PAL_STM32_OTYPE_PUSHPULL
        | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING
        | PAL_STM32_ALTERNATE(9);
    palSetPadMode(GPIOB, GPIOB_PIN8, mode); // RX
    palSetPadMode(GPIOB, GPIOB_PIN9, mode); // TX

    CANDriver *cand = &CAND1;
    canStart(cand, &can_config);
    chThdCreateStatic(can_rx_thread_wa, sizeof(can_rx_thread_wa), NORMALPRIO+2, can_rx_thread, cand);
    chThdCreateStatic(can_tx_thread_wa, sizeof(can_tx_thread_wa), NORMALPRIO, can_tx_thread, cand);
}
