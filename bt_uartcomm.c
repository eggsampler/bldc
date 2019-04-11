/*
	Copyright 2017 Benjamin Vedder	benjamin@vedder.se

	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bt_uartcomm.h"
#include "ch.h"
#include "hal.h"
#include "hw.h"
#include "packet.h"
#include "commands.h"

#include <string.h>

// Settings
#ifdef USE_ALT_UART_PORT
#define BAUDRATE                    115200
#else
#define BAUDRATE                    250000
#endif
#define PACKET_HANDLER              1

// Threads
static THD_FUNCTION(packet_process_thread2, arg);
static THD_WORKING_AREA(packet_process_thread2_wa, 4096);

// Variables
static volatile bool thread_is_running = false;
static volatile bool uart_is_running = false;

// Private functions
static void process_packet(unsigned char *data, unsigned int len);
static void send_packet(unsigned char *data, unsigned int len);

static SerialConfig uart_cfg = {
                                BAUDRATE,
                                0,
                                USART_CR2_LINEN,
                                0
};

static void process_packet(unsigned char *data, unsigned int len) {
  commands_process_packet(data, len, bt_uartcomm_send_packet);
}

static void send_packet(unsigned char *data, unsigned int len) {
  if (uart_is_running) {
    sdWrite(&BT_UART_DEV, data, len);
  }
}


void bt_uartcomm_start(void) {
  packet_init(send_packet, process_packet, PACKET_HANDLER);

  if (!thread_is_running) {
    chThdCreateStatic(packet_process_thread2_wa, sizeof(packet_process_thread2_wa),
                      NORMALPRIO, packet_process_thread2, NULL);
    thread_is_running = true;
  }

  sdStart(&BT_UART_DEV, &uart_cfg);
  palSetPadMode(BT_UART_TX_PORT, BT_UART_TX_PIN, PAL_MODE_ALTERNATE(BT_UART_GPIO_AF) |
                PAL_STM32_OSPEED_HIGHEST |
                PAL_STM32_PUDR_FLOATING);
  palSetPadMode(BT_UART_RX_PORT, BT_UART_RX_PIN, PAL_MODE_ALTERNATE(BT_UART_GPIO_AF) |
                PAL_STM32_OSPEED_HIGHEST |
                PAL_STM32_PUDR_FLOATING);
  uart_is_running = true;
}


void bt_uartcomm_stop(void) {
  sdStop(&BT_UART_DEV);
  palSetPadMode(BT_UART_TX_PORT, BT_UART_TX_PIN, PAL_MODE_INPUT_PULLUP);
  palSetPadMode(BT_UART_RX_PORT, BT_UART_RX_PIN, PAL_MODE_INPUT_PULLUP);
  uart_is_running = false;

  // Notice that the processing thread is kept running in case this call is made from it.
}

void bt_uartcomm_send_packet(unsigned char *data, unsigned int len) {

  packet_send_packet(data, len, PACKET_HANDLER);
}

void bt_uartcomm_configure(uint32_t baudrate) {
  uart_cfg.speed = baudrate;

  if (thread_is_running) {
    sdStart(&BT_UART_DEV, &uart_cfg);
    uart_is_running = true;
  }
}

static THD_FUNCTION(packet_process_thread2, arg) {
  (void)arg;

      chRegSetThreadName("uartcomm process");

      event_listener_t el;
      chEvtRegisterMaskWithFlags(&BT_UART_DEV.event, &el, EVENT_MASK(0), CHN_INPUT_AVAILABLE);

      for(;;) {
          chEvtWaitAny(ALL_EVENTS);

          bool rx = true;
          while (rx) {
              rx = false;

              msg_t res = sdGetTimeout(&BT_UART_DEV, TIME_IMMEDIATE);
              if (res != MSG_TIMEOUT) {
                  packet_process_byte(res, PACKET_HANDLER);
                  rx = true;
              }
          }
      }
}




