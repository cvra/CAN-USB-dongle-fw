#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_bridge_start(SerialUSBDriver *sdu);

#ifdef __cplusplus
}
#endif

#endif /* UART_BRIDGE_H */
