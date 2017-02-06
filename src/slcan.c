#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "version.h"
#include "can_driver.h"
#include "slcan.h"

#define MAX_FRAME_LEN (sizeof("T1111222281122334455667788EA5F\r")+1)

int slcan_serial_write(void *arg, const char *buf, size_t len);
char *slcan_getline(void *arg);

static void slcan_ack(char *buf);
static void slcan_nack(char *buf);

static char hex_digit(const uint8_t b)
{
    static const char *hex_tbl = "0123456789abcdef";
    return hex_tbl[b & 0x0f];
}

static void hex_write(char **p, const uint8_t *data, uint8_t len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        *(*p)++ = hex_digit(data[i]>>4);
        *(*p)++ = hex_digit(data[i]);
    }
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

static uint32_t hex_to_u32(const char *str, uint8_t len)
{
    uint32_t val = 0;
    unsigned int i;
    for (i = 0; i < len; i++) {
        val = (val<<4) | hex_val(str[i]);
    }
    return val;
}

static uint8_t hex_to_u8(const char *str)
{
    uint8_t val;
    val = hex_val(*str++);
    val = (val<<4) | hex_val(*str);
    return val;
}

void hex_to_u8_array(const char *str, uint8_t *buf, size_t len)
{
    while (len-- > 0) {
        *buf++ = hex_to_u8(str);
        str += 2;
    }
}

size_t slcan_frame_to_ascii(char *buf, const struct can_frame_s *f, bool timestamp)
{
    char *p = buf;
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
        int i;
        for (i = 3; i >= 0; i--) {
            uint8_t b = id>>(8*i);
            hex_write(&p, &b, 1);
        }
    } else {
        *p++ = hex_digit(id>>8);
        *p++ = hex_digit(id>>4);
        *p++ = hex_digit(id);
    }

    // DLC
    *p++ = hex_digit(f->length);

    // data
    if (!f->remote) {
        hex_write(&p, f->data, f->length);
    }

    // timestamp
    if (timestamp) {
        uint16_t t = f->timestamp;
        uint8_t b[2] = {t>>8, t};
        hex_write(&p, b, 2);
    }

    *p++ = '\r';
    *p = 0;

    return (size_t)(p - buf);
}

#define SLC_STD_ID_LEN 3
#define SLC_EXT_ID_LEN 8

void slcan_send_frame(char *line)
{
    char *out = line;
    uint8_t data[8];
    uint8_t len;
    uint32_t id;
    bool remote = false;
    bool extended = false;

    switch (*line++) {
    case 'r':
        remote = true;
        /* fallthrought */
    case 't':
        id = hex_to_u32(line, SLC_STD_ID_LEN);
        line += SLC_STD_ID_LEN;
        break;
    case 'R':
        remote = true;
        /* fallthrought */
    case 'T':
        extended = true;
        id = hex_to_u32(line, SLC_EXT_ID_LEN);
        line += SLC_EXT_ID_LEN;
        break;
    default:
        slcan_nack(out);
        return;
    };

    len = hex_val(*line++);

    if (len > 8) {
        slcan_nack(out);
        return;
    }

    if (!remote) {
        hex_to_u8_array(line, data, len);
    }

    if (can_send(id, extended, remote, data, len)) {
        slcan_ack(out);
    } else {
        slcan_nack(out);
    }
}

static void set_bitrate(char* line)
{
    static const uint32_t br_tbl[10] = {10000, 20000, 50000, 100000, 125000,
                                        250000, 500000, 800000, 1000000};
    unsigned char i = line[1];
    if (i < '0' || i > '8') {
        slcan_nack(line);
        return;
    }
    i -= '0';
    if (can_set_bitrate(br_tbl[i])) {
        slcan_ack(line);
    } else {
        slcan_nack(line);
    }
}

static void slcan_open(char *line, int mode)
{
    if (can_open(mode)) {
        slcan_ack(line);
    } else {
        slcan_nack(line);
    }
}

static void slcan_close(char *line)
{
    can_close();
    slcan_ack(line);
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
        slcan_open(line, CAN_MODE_NORMAL);
        break;
    case 'l': // open in loop back mode
        slcan_open(line, CAN_MODE_LOOPBACK);
        break;
    case 'L': // open in silent mode (listen only)
        slcan_open(line, CAN_MODE_SILENT);
        break;
    case 'C': // close CAN channel
        slcan_close(line);
        break;
    case 'V': // hardware version
        line = stpcpy(line, hardware_version_str);
        slcan_ack(line);
        break;
    case 'v': // firmware version
        line = stpcpy(line, software_version_str);
        slcan_ack(line);
        break;
    // 'N': // serial number
    // 'F': // read status byte
    // 'Z': // timestamp on/off, Zx[CR]
    // 'm': // acceptance mask, mxxxxxxxx[CR]
    // 'M': // acceptance code, Mxxxxxxxx[CR]
    default:
        slcan_nack(line);
        break;
    };
}

void slcan_spin(void *arg)
{
    char *line = slcan_getline(arg);
    if (line) {
        slcan_decode_line(line);
        slcan_serial_write(arg, line, strlen(line));
    }

    struct can_frame_s *rxf;
    while ((rxf = can_receive()) != NULL) {
        static char txbuf[MAX_FRAME_LEN];
        size_t len;
        len = slcan_frame_to_ascii(txbuf, rxf, false);
        can_frame_delete(rxf);
        slcan_serial_write(arg, txbuf, len);
    }
}
