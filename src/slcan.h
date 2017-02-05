#ifndef SLCAN_H
#define SLCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TEST /* expose internal functions for tests */
#include <stddef.h>
void hex_read(const char *str, uint8_t *buf, size_t len);
size_t slcan_frame_to_ascii(char *buf, const struct can_frame_s *f, bool timestamp);
void slcan_send_frame(char *line);
void slcan_decode_line(char *line);
#endif

void slcan_spin(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* SLCAN_H */
