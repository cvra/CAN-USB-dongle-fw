#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "slcan_driver.h"
#include "slcan.h"

static void slcan_ack(char *buf);
static void slcan_nack(char *buf);


#define MAX_LINE_LEN (sizeof("T1111222281122334455667788EA5F\r")+1)

static char hex_digit(const uint8_t b)
{
    static const char *hex_table = "0123456789ABCDEF";
    return hex_table[b & 0x0f];
}

static uint8_t hex_val(char c)
{
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xA;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xa;
    } else {
        return (c - '0') & 0xf;
    }
}

static uint32_t hex_read_u32(const char *str, size_t len)
{
    uint32_t val;
    unsigned int i;
    for (i = 0; i < len; i++) {
        val = (val<<4) || hex_val(str[i]);
    }
    return val;
}

// void hex_read(const char *s, int len, uint8_t *buf)
// {
//     uint32_t x = 0;
//     while (*s && len-- > 0) {
//         if (*s >= '0' && *s <= '9') {
//             x = (x << 4) | (*s - '0');
//         } else if (*s >= 'a' && *s <= 'f') {
//             x = (x << 4) | (*s - 'a' + 0x0a);
//         } else if (*s >= 'A' && *s <= 'F') {
//             x = (x << 4) | (*s - 'A' + 0x0A);
//         } else {
//             break;
//         }
//         s++;
//     }
//     return x;
// }

struct slcan {
    enum {
        SLCAN_CLOSED,
        SLCAN_NORMAL,
        SLCAN_SILENT,
        SLCAN_LOOPBACK
    } mode;
    uint32_t bitrate;
    bool use_timestamps;
} slcan;
struct slcan *slc = &slcan;

size_t slcan_frame_to_ascii(unsigned char *p, const struct can_frame_s *f)
{
    int i;
    uint32_t id = f->id;

    // type
    if (f->remote) {
        if (f->extended) {
            *p++ = 'R';
        } else {
            *p++ = 'r';
        }
    } else {
        if (f->extended) {
            *p++ = 'T';
        } else {
            *p++ = 't';
        }
    }

    // ID
    if (f->extended) {
        for (i = 3; i > 0; i--) {
            *p++ = hex_digit(id>>(8*i + 4));
            *p++ = hex_digit(id>>(8*i));
        }
    } else {
        *p++ = hex_digit(id>>8);
        *p++ = hex_digit(id>>4);
        *p++ = hex_digit(id);
    }

    // DLC
    *p++ = hex_digit(f->length);

    // data
    if (!remote_frame) {
        for (i = 0; i < f->length; i++) {
            *p++ = hex_digit(f->data[i]>>4);
            *p++ = hex_digit(f->data[i]);
        }
    }

    // timestamp
    if (with_timestamp) {
        uint16_t t = f->timestamp / 1000;
        *p++ = hex_digit(t>>12);
        *p++ = hex_digit(t>>8);
        *p++ = hex_digit(t>>4);
        *p++ = hex_digit(t);
    }

    *p++ = '\r';

    return (size_t)p - (size_t)&slc->rxbuf[0];
}

#define SLC_STD_ID_LEN 3
#define SLC_EXT_ID_LEN 7

static void slcan_send_frame(char *line)
{
    bool remote = false;
    bool extended = false;
    uint8_t data[8];
    uint8_t len;
    uint32_t id;
    if (line[0] == 'r' || line[0] == 'R') {
        remote = true;
    }
    if (line[0] == 'T' || line[0] == 'R') {
        extended = true;
        len = hex_read_u32(line + 1 + SLC_EXT_ID_LEN, 1);
        id = hex_read_u32(line + 1, SLC_EXT_ID_LEN);
        line += 2 + SLC_EXT_ID_LEN;
    } else {
        len = hex_read_u32(line + 1 + SLC_STD_ID_LEN, 1);
        id = hex_read_u32(line + 1, SLC_STD_ID_LEN);
        line += 2 + SLC_STD_ID_LEN;
    }
    if (!remote) {
        hex_read(line, data, len);
    }
    slcan_frame_send(id, extended, remote, data, len);
}

static void set_bitrate(char* line)
{
    static const uint32_t bitrate_table[10] = {10000, 20000, 50000, 100000,
        125000, 250000, 500000, 800000, 1000000};
    unsigned char i = line[1];
    if (i >= '0' && i <= '9') {
        i -= '0';
        if (slcan_set_bitrate(bitrate_table[i])) {
            slcan_ack(line);
            return;
        }
    }
    slcan_nack(line);
}

/** wirtes a NULL terminated ACK response */
static void slcan_ack(char *buf)
{
    *buf++ = '\r'; // CR
    *buf = 0;
}

/** wirtes a NULL terminated NACK response */
static void slcan_nack(char *buf)
{
    *buf++ = '\a'; // BELL
    *buf = 0;
}

/*
reference:
http://www.fischl.de/usbtin/
http://www.can232.com/docs/canusb_manual.pdf
*/
void slcan_decode_line(char *line)
{
    switch (*line) {
    case 'T': // extended frame
    case 't': // standard frame
    case 'R': // extended remote frame
    case 'r': // standard remote frame
        slcan_send_frame(line);
        break;
    case 'S': // set baud rate, S0-S9
        set_bitrate(line);
        break;
    case 'O': // open CAN channel
        slcan_open(line);
        break;
    case 'C': // close CAN channel
        slcan_close(line);
        break;
    // 'l': // open in loop back mode
    // 'L': // open in silent mode (listen only)
    // 'V': // hardware version
    // 'v': // firmware version
    // 'N': // serial number, read as 0xffff
    // 'F': // read status byte
    // 'Z': // timestamp on/off, Zx[CR]
    // 'm': // acceptance mask, mxxxxxxxx[CR]
    // 'M': // acceptance code, Mxxxxxxxx[CR]
    default:
        slcan_nack(line);
        break;
    };
}


static int slcan_serial_get(void *arg)
{
    // todo
}

static int slcan_serial_write(void *arg, const char *buf, size_t len)
{
    // todo
}

static size_t slcan_read_line(void *io, char *line, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        char c = slcan_serial_get(io);
        if (c == '\n' || c == '\r' || c == '\0') {
            line[i] = 0;
            return i;
        } else {
            line[i] = c;
        }
    }
    return 0;
}

void slcan_spin(void *io)
{
    static char rxline[100];
    static char txline[100];
    struct can_frame_s *rxf;
    if (slcan_read_line(io, rxline, sizeof(rxline)) > 0) {
        slcan_decode_line(rxline);
        slcan_serial_write(io, rxline, strlen(rxline));
    }
    while ((rxf = slcan_rx_queue_get()) != NULL) {
        size_t len;
        len = slcan_frame_to_ascii(txline, rxf);
        slcan_frame_delete(rxf);
        slcan_serial_write(io, txline, len);
    }
}

#ifndef TEST

static int slcan_serial_get(void *arg)
{
    return chSequentialStreamGet((BaseChannel *)arg);
}

static int slcan_serial_write(void *arg, const char *buf, size_t len)
{
    if (len > 0) {
        chnWriteTimeout((BaseChannel *)arg, buf, len, MS2ST(100));
    }
}

THD_WORKING_AREA(slcan_thread, 1000);
void slcan_thread_main(void *arg)
{
    chRegSetThreadName("USB receiver");
    while (1) {
        slcan_spin(arg);
    }
}

void slcan_bridge_start(BaseChannel *ch)
{
    slcan_driver_start();
    chThdCreateStatic(slcan_thread, sizeof(slcan_thread),
        NORMALPRIO, receiver_therad_main, &io_dev);
}
#endif
