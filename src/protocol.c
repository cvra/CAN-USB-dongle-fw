#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cmp/cmp.h>
#include <datagram-messages/service_call.h>
#include <datagram-messages/msg_dispatcher.h>
#include <cmp_mem_access/cmp_mem_access.h>
#include <can_driver.h>
#include <bus_power.h>
#include <device_info.h>
#include "protocol.h"

#define CAN_ID_MASK             ((1<<29) - 1)
#define CAN_ID_EXTENDED_MAX     ((1<<29) - 1)
#define CAN_ID_STANDARD_MAX     ((1<<11) - 1)
#define CAN_ID_EXTENDED         (1<<29)
#define CAN_ID_REMOTE           (1<<30)

void can_drop_msg_encode(cmp_ctx_t *cmp)
{
    cmp_write_array(cmp, 2);
    cmp_write_str(cmp, "drop", 4);
    cmp_write_nil(cmp);
}

void can_error_msg_encode(cmp_ctx_t *cmp, timestamp_t timestamp)
{
    cmp_write_array(cmp, 2);
    cmp_write_str(cmp, "err", 3);
    cmp_write_uint(cmp, timestamp);
}

void can_rx_msg_encode(cmp_ctx_t *cmp, struct can_frame_s *frame, timestamp_t timestamp)
{
    uint8_t buf[13];
    uint32_t id = frame->id;
    if (frame->extended) {
        id |= CAN_ID_EXTENDED;
    }
    if (frame->remote) {
        id |= CAN_ID_REMOTE;
    }
    buf[0] = id;
    buf[1] = id>>8;
    buf[2] = id>>16;
    buf[3] = id>>24;
    int i;
    for (i  = 0; i < frame->length; i++) {
        buf[i+4] = frame->data[i];
    }
    size_t len = 4 + frame->length;
    cmp_write_array(cmp, 2);
    cmp_write_str(cmp, "rx", 2);
    cmp_write_array(cmp, 2);
    cmp_write_bin(cmp, &buf[0], len);
    cmp_write_uint(cmp, timestamp);
}

static bool can_frame_decode_and_send(uint8_t *buf, size_t len)
{
    bool extended = false;
    bool remote = false;
    if (len < 5) {
        return false;
    }
    uint32_t id = buf[0] | buf[1]<<8 | buf[2]<<16 | buf[3]<<24;
    buf += 4;
    len -= 4;
    if (len > 8) {
        return false; // data must be <= 8 bytes
    }
    if (id & CAN_ID_EXTENDED) {
        extended = true;
    }
    if (id & CAN_ID_REMOTE) {
        extended = true;
    }
    id &= CAN_ID_MASK;
    return can_frame_send(id, extended, remote, buf, len);
}

bool tx_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    uint32_t nb_frames;
    bool ok = false;
    if (cmp_read_array(in, &nb_frames) && nb_frames > 0) {
        while (nb_frames--) {
            uint32_t size;
            if (cmp_read_bin_size(in, &size)) {
                cmp_mem_access_t *mem = (cmp_mem_access_t *)in->buf;
                size_t pos = cmp_mem_access_get_pos(mem);
                void *frame = cmp_mem_access_get_ptr_at_pos(mem, pos);
                cmp_mem_access_set_pos(mem, pos + size);
                if (!can_frame_decode_and_send(frame, size)) {
                    break;
                } else if (nb_frames == 0) {
                    ok = true; // last frame successfully transmitted
                }
            } else {
                break;
            }
        }
    }
    cmp_write_bool(out, ok);
    return true;
}

bool bit_rate_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    bool ok = false;
    uint32_t bitrate;
    if (cmp_read_uint(in, &bitrate)) {
        ok = can_set_bitrate(bitrate);
    }
    return cmp_write_bool(out, ok);
}

bool filter_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    uint32_t nb_filter;
    bool ok = false;
    if (cmp_read_array(in, &nb_filter)) {
        can_filter_start_edit();
        if (nb_filter == 0) {
            can_filter_reset();
        }
        uint32_t i;
        for (i = 0; i < nb_filter; i++) {
            uint32_t len;
            if (cmp_read_array(in, &len) && len == 2) {
                uint32_t id, mask;
                if (!cmp_read_uint(in, &id) || !cmp_read_uint(in, &mask)) {
                    break;
                }
                if (!can_filter_set(i, id, mask)) {
                    break;
                }
            } else {
                break;
            }
        }
        if (i == nb_filter) {
            ok = true; // all filters successful
        }
        can_filter_stop_edit();
    }
    uint64_t timestamp = 0; // todo
    if (cmp_write_array(out, 2)
        && cmp_write_bool(out, ok)
        && cmp_write_uint(out, timestamp)) {
        return true;
    } else {
        return false;
    }
}

bool bus_voltage_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    (void)in;
    return cmp_write_float(out, bus_voltage_get());
}

bool bus_power_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    bool ok = false;
    bool enable;
    if (cmp_read_bool(in, &enable)) {
        ok = bus_power(enable);
    }
    return cmp_write_bool(out, ok);
}

bool silent_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    bool enable;
    if (cmp_read_bool(in, &enable)) {
        can_silent_mode(enable);
    }
    return cmp_write_nil(out);
}

bool loop_back_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)arg;
    bool enable;
    if (cmp_read_bool(in, &enable)) {
        can_loopback_mode(enable);
    }
    return cmp_write_nil(out);
}

bool hw_version_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)in;
    (void)arg;
    return cmp_write_str(out, HARDWARE_VERSION_STR, strlen(HARDWARE_VERSION_STR));
}

bool sw_version_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)in;
    (void)arg;
    return cmp_write_str(out, SOFTWARE_VERSION_STR, strlen(SOFTWARE_VERSION_STR));
}

bool name_cb(cmp_ctx_t *in, cmp_ctx_t *out, void *arg)
{
    (void)in;
    (void)arg;
    return cmp_write_str(out, DEVICE_NAME_STR, strlen(DEVICE_NAME_STR));
}

struct service_entry_s service_calls[] = {
    {.id="tx", .cb=tx_cb, .arg=NULL},
    {.id="filter", .cb=filter_cb, .arg=NULL},
    {.id="bit rate", .cb=bit_rate_cb, .arg=NULL},
    {.id="bus voltage", .cb=bus_voltage_cb, .arg=NULL},
    {.id="bus power", .cb=bus_power_cb, .arg=NULL},
    {.id="silent", .cb=silent_cb, .arg=NULL},
    {.id="loop back", .cb=loop_back_cb, .arg=NULL},
    {.id="hw version", .cb=hw_version_cb, .arg=NULL},
    {.id="sw version", .cb=sw_version_cb, .arg=NULL},
    {.id="name", .cb=name_cb, .arg=NULL},
    {NULL, NULL, NULL},
};
