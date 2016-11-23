#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct can_frame_s {
    uint32_t timestamp;
    uint32_t id:29;
    uint32_t extended:1;
    uint32_t remote:1;
    uint8_t length;
    uint8_t data[8];
};

void slcan_frame_delete(struct can_frame_s *f);
struct can_frame_s *slcan_rx_queue_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
