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
    uint32_t id : 29;
    uint32_t extended : 1;
    uint32_t remote : 1;
    uint8_t length;
    uint8_t data[8];
};

enum {
    CAN_MODE_NORMAL,
    CAN_MODE_LOOPBACK,
    CAN_MODE_SILENT
};

/* non-blocking CAN frame receive, NULL if nothing received */
struct can_frame_s* can_receive(void);
void can_frame_delete(struct can_frame_s* f);

/* blocking CAN frame send */
bool can_send(uint32_t id, bool extended, bool remote, uint8_t* data, size_t length);

/* returns true on success, must be called before can_open */
bool can_set_bitrate(uint32_t bitrate);

/* returns true on success */
bool can_open(int mode);
void can_close(void);
void can_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
