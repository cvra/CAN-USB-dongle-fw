#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct can_frame_s {
    uint32_t id:29;
    uint32_t extended:1;
    uint32_t remote:1;
    uint8_t length;
    uint8_t data[8];
};

struct can_rx_frame_s {
    uint64_t timestamp;
    struct can_frame_s frame;
    bool error;
};

bool can_frame_send(uint32_t id, bool extended, bool remote, void *data, size_t length);
struct can_rx_frame_s *can_frame_receive(bool *dropped);
void can_rx_frame_delete(struct can_rx_frame_s *f);
void can_driver_start(void);
bool can_set_bitrate(uint32_t bitrate);
void can_filter_start_edit(void);
void can_filter_stop_edit(void);
void can_filter_reset(void);
bool can_filter_set(unsigned int filter_nb, uint32_t id, uint32_t mask);
void can_silent_mode(bool enable);
void can_loopback_mode(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
