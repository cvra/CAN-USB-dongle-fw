#ifndef CAN_BRIDGE_H
#define CAN_BRIDGE_H

#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif

void can_bridge_start(BaseChannel *ch);

#ifdef __cplusplus
}
#endif

#endif /* CAN_BRIDGE_H */
