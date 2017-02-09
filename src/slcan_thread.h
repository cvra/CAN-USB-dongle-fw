#ifndef SLCAN_THREAD_H
#define SLCAN_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ch.h>

void slcan_start(BaseChannel *ch);

#ifdef __cplusplus
}
#endif

#endif /* SLCAN_THREAD_H */
