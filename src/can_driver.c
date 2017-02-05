#include <ch.h>
#include <hal.h>
#include <string.h>
#include <timestamp/timestamp.h>
#include "can_driver.h"

#define CAN_RX_BUFFER_SIZE   100

// size+2 for the 2 threads handling CAN frames from can_rx_pool
#define CAN_RX_POOL_SIZE CAN_RX_BUFFER_SIZE + 2

#define CAN_BTR_BRP_MASK 0x000003FF
#define CAN_BTR_TS1_MASK 0x000F0000
#define CAN_BTR_TS2_MASK 0x00700000
#define CAN_BTR_TIMING_MASK (CAN_BTR_BRP_MASK | CAN_BTR_TS1_MASK | CAN_BTR_TS2_MASK)
#if !defined(CAN_DEFAULT_BITRATE)
#define CAN_DEFAULT_BITRATE     1000000
#endif

#define CAN_BASE_CLOCK STM32_PCLK1 // APB1 clock = 36MHz
#if CAN_BASE_CLOCK != 36000000
#error "CAN peripheral clock"
#endif
#define BTR_TS1 6
#define BTR_TS2 5
#define CAN_BASE_BIT_RATE CAN_BASE_CLOCK / (1 + BTR_TS1 + BTR_TS2)

static bool can_btr_from_bitrate(uint32_t bitrate, uint32_t *btr_value);
static void can_wait_if_stop_requested(void);
static void can_stop(void);
static void can_start(void);
static void can_rx_queue_post(struct can_frame_s *fp);
static void can_rx_queue_flush(void);

static CANConfig can_config = {
    .mcr = (1 << 6)  // Automatic bus-off management enabled
         | (1 << 2), // Message are prioritized by order of arrival
    .btr = CAN_BTR_SILM // Silent mode
         | CAN_BTR_SJW_0 // 2tq resynchronization jump width
};

// searches CAN bit rate register value
static bool can_btr_from_bitrate(uint32_t bitrate, uint32_t *btr_value)
{
    if (bitrate > 1000000 ||
        CAN_BASE_BIT_RATE % bitrate != 0 ||
        CAN_BASE_BIT_RATE / bitrate > 1024) {
        // bad range or no exact bitrate possible
        return false;
    }
    uint32_t prescaler = CAN_BASE_BIT_RATE / bitrate;
    *btr_value = ((prescaler - 1)<<0) | ((BTR_TS1 - 1)<<16) | ((BTR_TS2 - 1)<<20);
    return true;
}

// synchronize between recevier and sender thread
semaphore_t can_config_wait;

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
struct can_frame_s rx_pool_buf[CAN_RX_POOL_SIZE];

bool can_send(uint32_t id, bool extended, bool remote, void *data, size_t length)
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
            continue;
        }
        led_set(CAN1_STATUS_LED);
        struct can_frame_s *fp = (struct can_frame_s *)chPoolAlloc(&can_rx_pool);
        if (fp == NULL) {
            chSysHalt("CAN driver out of memory");
        }
        fp->timestamp = timestamp_get()/1000;
        if (rxf.IDE) {
            fp->id = rxf.EID;
            fp->extended = 1;
        } else {
            fp->id = rxf.SID;
            fp->extended = 0;
        }
        if (rxf.RTR) {
            fp->remote = 1;
        } else {
            fp->remote = 0;
        }
        fp->length = rxf.DLC;
        memcpy(&fp->data[0], &rxf.data8[0], rxf.DLC);
        can_rx_queue_post(fp);
    }
}

static void can_rx_queue_post(struct can_frame_s *fp)
{
    msg_t m = chMBPost(&can_rx_queue, (msg_t)fp, TIME_IMMEDIATE);
    if (m != MSG_OK) {
        chPoolFree(&can_rx_pool, fp);
        can_rx_queue_flush();
    }
}

static void can_rx_queue_flush(void)
{
    struct can_frame_s *fp;
    while (1) {
        msg_t m = chMBFetch(&can_rx_queue, (msg_t *)&fp, TIME_IMMEDIATE);
        if (m == MSG_OK) {
            chPoolFree(&can_rx_pool, fp);
        } else {
            break;
        }
    }
}

void can_frame_delete(struct can_frame_s *f)
{
    chPoolFree(&can_rx_pool, f);
}

struct can_frame_s *can_receive(void)
{
    struct can_frame_s *fp;
    msg_t m = chMBFetch(&can_rx_queue, (msg_t *)&fp, TIME_IMMEDIATE);
    if (m == MSG_OK) {
        return fp;
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

bool can_open(void)
{
    // TODO
    return true;
}

void can_close(void)
{
    // TODO
}

void can_init(void)
{
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
