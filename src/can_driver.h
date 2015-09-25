#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <ch.h>

extern memory_pool_t can_rx_pool;
extern memory_pool_t can_tx_pool;
extern mailbox_t can_rx_queue;
extern mailbox_t can_tx_queue;

#ifdef __cplusplus
extern "C" {
#endif

void can_driver_start(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
