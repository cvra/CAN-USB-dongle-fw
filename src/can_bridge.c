#include <ch.h>
#include <hal.h>
#include <cmp_mem_access/cmp_mem_access.h>
#include <datagram-messages/msg_dispatcher.h>
#include <datagram-messages/service_call.h>
#include <serial-datagram/serial_datagram.h>
#include "protocol.h"

void send_cb(void *arg, const void *p, size_t len)
{
    if (len > 0) {
        BaseChannel *ch = (BaseChannel *)arg;
        chnWrite(ch, p, len);
    }
}

struct io_dev_s {
    BaseChannel *channel;
    binary_semaphore_t lock;
};

void io_dev_send(const void *dtgrm, size_t len, void *arg)
{
    struct io_dev_s *dev = (struct io_dev_s *)arg;
    chBSemWait(&dev->lock);
    serial_datagram_send(dtgrm, len, send_cb, dev->channel);
    chBSemSignal(&dev->lock);
}

struct service_call_handler_s service_call_handler;
struct msg_dispatcher_entry_s messages[] = {
    {.id="req", .cb=service_call_msg_cb, .arg=&service_call_handler},
    {.id=NULL, .cb=NULL, .arg=NULL},
};

THD_WORKING_AREA(receiver_therad_wa, 1000);
void receiver_therad(void *arg)
{
    chRegSetThreadName("USB receiver");
    struct io_dev_s *dev = (struct io_dev_s *)arg;

    static char response_buffer[100];
    service_call_handler.service_table = service_calls;
    service_call_handler.response_buffer = response_buffer;
    service_call_handler.response_buffer_sz = sizeof(response_buffer);
    service_call_handler.send_cb = io_dev_send;
    service_call_handler.send_cb_arg = dev;

    serial_datagram_rcv_handler_t rcv;
    static char datagram_buf[64];
    serial_datagram_rcv_handler_init(
        &rcv,
        datagram_buf,
        sizeof(datagram_buf),
        (serial_datagram_cb_t)msg_dispatcher,
        messages);

    while (1) {
        static uint8_t buf[32];
        int len = chnReadTimeout(dev->channel, buf, sizeof(buf), MS2ST(1));
        if (len == 0) {
            continue;
        }
        serial_datagram_receive(&rcv, buf, len);
    }
}

THD_WORKING_AREA(sender_thread_wa, 1000);
void sender_thread(void *arg)
{
    chRegSetThreadName("USB sender");
    struct io_dev_s *dev = (struct io_dev_s *)arg;
    char message_buffer[32];
    cmp_ctx_t cmp;
    cmp_mem_access_t mem;
    cmp_mem_access_init(&cmp, &mem, message_buffer, sizeof(message_buffer));
    while (1) {
        struct can_rx_frame_s *fp;
        bool drop;
        fp = can_frame_receive(&drop);
        if (drop) {
            can_drop_msg_encode(&cmp);
        } else {
            if (fp == NULL) {
                continue;
            } else if (fp->error) {
                can_error_msg_encode(&cmp, fp->timestamp);
            } else {
                can_rx_msg_encode(&cmp, &fp->frame, fp->timestamp);
            }
            can_rx_frame_delete(fp);
        }
        size_t len = cmp_mem_access_get_pos(&mem);
        io_dev_send(message_buffer, len, dev);
        cmp_mem_access_set_pos(&mem, 0);
    }
}

void can_bridge_start(BaseChannel *ch)
{
    static struct io_dev_s io_dev;
    io_dev.channel = ch;
    chBSemObjectInit(&io_dev.lock, false);
    can_driver_start();
    chThdCreateStatic(receiver_therad_wa, sizeof(receiver_therad_wa),
        NORMALPRIO, receiver_therad, &io_dev);
    chThdCreateStatic(sender_thread_wa, sizeof(sender_thread_wa),
        NORMALPRIO, sender_thread, &io_dev);
}
