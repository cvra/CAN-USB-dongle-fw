
#include <ch.h>
#include <hal.h>
#include "usbcfg.h"
#include <chprintf.h>

volatile bool uart1_needs_reconfig = false;
binary_semaphore_t uart1_config_lock;

static THD_WORKING_AREA(uart_rx_wa, 300);
static THD_FUNCTION(uart_rx, arg)
{
    chRegSetThreadName("uart_rx");
    BaseChannel* io = (BaseChannel*)arg;
    size_t len;
    static uint8_t buf[32];
    while (true) {
        len = sdReadTimeout(&SD1, &buf[0], sizeof(buf), MS2ST(1));
        if (len > 0) {
            led_set(STATUS_LED);
            chnWrite(io, &buf[0], len);
        }
        if (uart1_needs_reconfig) {
            // wait until uart is reconfigured
            chBSemWait(&uart1_config_lock);
        }
    }
}

static THD_WORKING_AREA(uart_tx_wa, 300);
static THD_FUNCTION(uart_tx, arg)
{
    chRegSetThreadName("uart_tx");
    BaseChannel* io = (BaseChannel*)arg;
    static SerialConfig uart1_config = {
        SERIAL_DEFAULT_BITRATE,
        0,
        USART_CR2_STOP1_BITS | USART_CR2_LINEN,
        0};
    uart1_config.speed = serial_usb_get_baudrate();
    uart1_config.speed *= 2;
    sdStart(&SD1, &uart1_config);
    // start uart receive thread
    chBSemObjectInit(&uart1_config_lock, true);
    chThdCreateStatic(uart_rx_wa, sizeof(uart_rx_wa), NORMALPRIO, uart_rx, io);
    while (true) {
        size_t len;
        static uint8_t buf[32];
        len = chnReadTimeout(io, &buf[0], sizeof(buf), MS2ST(1));
        if (len > 0) {
            led_set(STATUS_LED);
            sdWrite(&SD1, &buf[0], len);
        } else {
            // check if uart baudrate has changed
            uint32_t speed = serial_usb_get_baudrate();
            speed *= 2; // workaround: uart1 clock is off by a factor of 2 for some reason
            if (speed != uart1_config.speed) {
                // wait until receiver thread is halted
                uart1_needs_reconfig = true;
                while (!chBSemGetStateI(&uart1_config_lock)) {
                    chThdSleepMilliseconds(1);
                }
                // reconfigure uart
                sdStop(&SD1);
                uart1_config.speed = speed;
                sdStart(&SD1, &uart1_config);
                uart1_needs_reconfig = false;
                chBSemSignal(&uart1_config_lock);
            }
        }
    }
}

void uart_bridge_start(BaseChannel* ch)
{
    chThdCreateStatic(uart_tx_wa, sizeof(uart_tx_wa), NORMALPRIO, uart_tx, ch);
}
