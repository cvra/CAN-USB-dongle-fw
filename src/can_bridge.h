#ifndef CAN_BRIDGE_H
#define CAN_BRIDGE_H

#include <hal.h>
#include <serial-can-bridge/can_frame.h>

#ifdef __cplusplus
extern "C" {
#endif

void can_bridge_start(BaseSequentialStream *io);
void can_rx_frame_forward(struct can_frame *f);
void can_interface_send(struct can_frame *frame);

#ifdef __cplusplus
}
#endif

#endif /* CAN_BRIDGE_H */
