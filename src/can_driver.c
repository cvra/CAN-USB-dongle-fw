#include <ch.h>
#include <hal.h>
#include <string.h>
#include <timestamp/timestamp.h>
#include "can_driver.h"

#define CAN_RX_BUFFER_SIZE   10

semaphore_t can_config_wait;

// Baudrate prescaler (10 bits), Time segment 1 (4 bits), Time segment 2 (3 bits)
static const uint32_t can_bitrate_table[][2] = {
//  {bitrate, BRP      | TS1     | TS2
    {  10000, (199<<0) | (9<<16) | (6<<20)},
    { 100000, ( 19<<0) | (9<<16) | (6<<20)},
    { 250000, (  7<<0) | (9<<16) | (6<<20)},
    { 500000, (  3<<0) | (9<<16) | (6<<20)},
    {1000000, (  1<<0) | (9<<16) | (6<<20)} // APB = 36MHz / 2 / (1tq + 10tq + 7tq) => 1Mbit
};
#define CAN_BITRATE_TABLE_SIZE  (sizeof(can_bitrate_table)/(2*sizeof(uint32_t)))

#define CAN_BTR_BRP_MASK 0x000003FF
#define CAN_BTR_TS1_MASK 0x000F0000
#define CAN_BTR_TS2_MASK 0x00700000
#define CAN_BTR_TIMING_MASK (CAN_BTR_BRP_MASK | CAN_BTR_TS1_MASK | CAN_BTR_TS2_MASK)
#if !defined(CAN_DEFAULT_BITRATE)
#define CAN_DEFAULT_BITRATE     1000000
#endif

static CANConfig can_config = {
    .mcr = (1 << 6)  // Automatic bus-off management enabled
         | (1 << 2), // Message are prioritized by order of arrival
    .btr = CAN_BTR_LBKM | CAN_BTR_SILM // Loopback & Silent mode
         | CAN_BTR_SJW_0 // 2tq resynchronization jump width
};

// searches CAN bit rate register value
static bool can_btr_from_bitrate(uint32_t bitrate, uint32_t *btr_value)
{
    unsigned int i;
    for (i = 0; i < CAN_BITRATE_TABLE_SIZE; i++) {
        if (can_bitrate_table[i][0] == bitrate) {
            *btr_value = can_bitrate_table[i][1];
            return true;
        }
    }
    return false;
}

static void can_wait_if_stop_requested(void)
{
    cnt_t count;
    chSysLock();
    count = chSemGetCounterI(&can_config_wait);
    chSysUnlock();
    if (count == -1) {
        chSemSignal(&can_config_wait);
        chSemWait(&can_config_wait);
    }
}

static void can_stop(void)
{
    chSemWait(&can_config_wait);
    canStop(&CAND1);
}

static void can_start(void)
{
    canStart(&CAND1, &can_config);
    chSemSignal(&can_config_wait);
}

memory_pool_t can_rx_pool;
mailbox_t can_rx_queue;
msg_t rx_mbox_buf[CAN_RX_BUFFER_SIZE];
binary_semaphore_t can_rx_queue_is_flushed;
bool can_rx_queue_flush;
struct can_rx_frame_s rx_pool_buf[CAN_RX_BUFFER_SIZE+1];

bool can_frame_send(uint32_t id, bool extended, bool remote, void *data, size_t length)
{
    led_set(CAN1_STATUS_LED);
    CANTxFrame txf;
    if (extended) {
        txf.EID = id;
        txf.IDE = 1;
    } else {
        txf.SID = id;
        txf.IDE = 0;
    }
    txf.DLC = length;
    if (remote) {
        txf.RTR = 1;
    } else {
        txf.RTR = 0;
        memcpy(&txf.data8[0], data, length);
    }
    msg_t m = canTransmit(&CAND1, CAN_ANY_MAILBOX, &txf, MS2ST(100));
    led_clear(CAN1_STATUS_LED);
    if (m != MSG_OK) {
        return false;
    }
    return true;
}

static THD_WORKING_AREA(can_rx_thread_wa, 256);
static THD_FUNCTION(can_rx_thread, arg) {
    (void)arg;
    chRegSetThreadName("CAN rx");
    while (1) {
        can_wait_if_stop_requested();
        CANRxFrame rxf;
        msg_t m = canReceive(&CAND1, CAN_ANY_MAILBOX, &rxf, MS2ST(10));
        if (m != MSG_OK) {
            led_clear(CAN1_STATUS_LED);
            continue;
        }
        led_toggle(CAN1_STATUS_LED);
        struct can_rx_frame_s *fp = (struct can_rx_frame_s *)chPoolAlloc(&can_rx_pool);
        if (fp == NULL) {
            chSysHalt("CAN driver out of memory");
        }
        fp->timestamp = timestamp_get();
        fp->error = false;
        if (rxf.IDE) {
            fp->frame.id = rxf.EID;
            fp->frame.extended = 1;
        } else {
            fp->frame.id = rxf.SID;
            fp->frame.extended = 0;
        }
        if (rxf.RTR) {
            fp->frame.remote = 1;
        } else {
            fp->frame.remote = 0;
        }
        fp->frame.length = rxf.DLC;
        memcpy(&fp->frame.data[0], &rxf.data8[0], rxf.DLC);

        m = chMBPost(&can_rx_queue, (msg_t)fp, TIME_IMMEDIATE);
        if (m != MSG_OK) {
            chPoolFree(&can_rx_pool, fp);
            can_rx_queue_flush = true;
            chBSemWait(&can_rx_queue_is_flushed);
        }
    }
}

void can_rx_frame_delete(struct can_rx_frame_s *f)
{
    chPoolFree(&can_rx_pool, f);
}

struct can_rx_frame_s *can_frame_receive(bool *drop)
{
    struct can_rx_frame_s *fp;
    *drop = false;
    systime_t timeout = MS2ST(100);
    if (can_rx_queue_flush) {
        timeout = TIME_IMMEDIATE;
    }
    msg_t m = chMBFetch(&can_rx_queue, (msg_t *)&fp, timeout);
    if (m == MSG_OK) {
        return fp;
    } else if (can_rx_queue_flush) {
        can_rx_queue_flush = false;
        chBSemSignal(&can_rx_queue_is_flushed);
        *drop = true;
    }
    return NULL;
}

bool can_set_bitrate(uint32_t bitrate)
{
    bool ok;
    can_stop();
    uint32_t btr;
    ok = can_btr_from_bitrate(bitrate, &btr);
    if (ok) {
        can_config.btr = (can_config.btr & ~CAN_BTR_TIMING_MASK) | btr;
    }
    can_start();
    return ok;
}

void can_filter_start_edit(void)
{
    can_stop();
}

void can_filter_stop_edit(void)
{
    can_start();
}

void can_filter_reset(void)
{
    // todo
}

bool can_filter_set(unsigned int filter_nb, uint32_t id, uint32_t mask)
{
    (void)filter_nb;
    (void)id;
    (void)mask;
    // todo
    return true;
}


void can_silent_mode(bool enable)
{
    can_stop();
    if (enable) {
        palSetPad(GPIOA, GPIOA_CAN_SILENT);
        can_config.btr |= CAN_BTR_SILM;
    } else {
        palClearPad(GPIOA, GPIOA_CAN_SILENT);
        can_config.btr &= ~CAN_BTR_SILM;
    }
    can_start();
}

void can_loopback_mode(bool enable)
{
    can_stop();
    if (enable) {
        can_config.btr |= CAN_BTR_LBKM;
    } else {
        can_config.btr &= ~CAN_BTR_LBKM;
    }
    can_start();
}

void can_driver_start(void)
{
    can_rx_queue_flush = false;
    chBSemObjectInit(&can_rx_queue_is_flushed, true);
    chMBObjectInit(&can_rx_queue, rx_mbox_buf, sizeof(rx_mbox_buf)/sizeof(rx_mbox_buf[0]));
    chPoolObjectInit(&can_rx_pool, sizeof(rx_pool_buf[0]), NULL);
    chPoolLoadArray(&can_rx_pool, rx_pool_buf, sizeof(rx_pool_buf)/sizeof(rx_pool_buf[0]));

    chSemObjectInit(&can_config_wait, 0);

    uint32_t btr;
    if (!can_btr_from_bitrate(CAN_DEFAULT_BITRATE, &btr)) {
        chSysHalt("CAN default bitrate");
    }
    can_config.btr |= btr;
    canStart(&CAND1, &can_config);
    chThdCreateStatic(can_rx_thread_wa, sizeof(can_rx_thread_wa), NORMALPRIO+2, can_rx_thread, NULL);
}
