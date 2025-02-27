/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief UART example for DA14585, DA14586 and DA14531 devices.
 *
 * Copyright (C) 2012-2023 Renesas Electronics Corporation and/or its
 *affiliates. All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation
 *and/or its affiliates ("Renesas"). Renesas grants you a personal,
 *non-exclusive, non-transferable, revocable, non-sub-licensable right and
 *license to use the Software, solely if used in or together with Renesas
 *products. You may make copies of this Software, provided this copyright notice
 *and disclaimer ("Notice") is included in all such copies. Renesas reserves the
 *right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY
 *KIND, WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE
 *WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *NON-INFRINGEMENT. TO THE MAXIMUM EXTENT PERMITTED UNDER LAW, IN NO EVENT SHALL
 *RENESAS BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR
 *CONSEQUENTIAL DAMAGES ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE,
 *EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. USE OF
 *THIS SOFTWARE MAY BE SUBJECT TO TERMS AND CONDITIONS CONTAINED IN AN
 *ADDITIONAL AGREEMENT BETWEEN YOU AND RENESAS. IN CASE OF CONFLICT BETWEEN THE
 *TERMS OF THIS NOTICE AND ANY SUCH ADDITIONAL LICENSE AGREEMENT, THE TERMS OF
 *THE AGREEMENT SHALL TAKE PRECEDENCE. BY CONTINUING TO USE THIS SOFTWARE, YOU
 *AGREE TO THE TERMS OF THIS NOTICE.IF YOU DO NOT AGREE TO THESE TERMS, YOU ARE
 *NOT PERMITTED TO USE THIS SOFTWARE.
 *
 ****************************************************************************************
 */
#include "arch_system.h"
#include "crc.h"
#include "gpio.h"
#include "hw_otpc.h"
#include "uart_common.h"
#include "uart_utils.h"
#include "user_periph_setup.h"
#include <stdio.h>

/**
 ****************************************************************************************
 * @brief UART print example
 * @param[in] uart_id           Identifies which UART to use
 ****************************************************************************************
 */
void uart_print_example(uart_t *uart);

/**
 ****************************************************************************************
 * @brief Main routine of the UART example
 ****************************************************************************************
 */

#define CS_OFFSET 0x7ED0
#define HDR_OFFSET 0x7FC0

#define CS_WORDS 60
#define HDR_WORDS 16

#define EMPTY_ADDRESS 0x7f4c

// offset - offset from 0x07f80000

int otp_read(uint32_t offset, uint32_t *data, uint32_t count) {
  int ret = count;

  uint32_t addr = MEMORY_OTP_BASE + offset;
  hw_otpc_init();
  hw_otpc_enter_mode(HW_OTPC_MODE_READ);

  while (count--) {
    *data++ = GetWord32(addr);
    addr += sizeof(uint32_t);
  }
  hw_otpc_close();
  return ret;
}

// offset in bytes from 0x07f80000
int otp_write(uint32_t offset, uint32_t *data, uint32_t count) {
  offset = offset / sizeof(uint32_t);
  hw_otpc_init();
  hw_otpc_prog(data, offset, count);
  hw_otpc_close();
}

int otp_read_hdr() {
  uint32_t buf[HDR_WORDS];
  otp_read(HDR_OFFSET, &buf[0], HDR_WORDS);

  printf_string(UART1, "OTP HDR\n\r");
  for (int i = 0; i < sizeof(buf) / sizeof(uint32_t); i++) {
    printf_string(UART1, "0x");
    print_word(UART1, MEMORY_OTP_BASE + HDR_OFFSET + i * sizeof(uint32_t));
    printf_string(UART1, ": 0x");
    print_word(UART1, buf[i]);
    printf_string(UART1, "\n\r");
  }
  printf_string(UART1, "END\n\r");
}
int otp_read_cs() {
  uint32_t buf[CS_WORDS];
  otp_read(CS_OFFSET, &buf[0], CS_WORDS);

  printf_string(UART1, "OTP CS\n\r");
  for (int i = 0; i < sizeof(buf) / sizeof(uint32_t); i++) {
    printf_string(UART1, "0x");
    print_word(UART1, MEMORY_OTP_BASE + CS_OFFSET + i * sizeof(uint32_t));
    printf_string(UART1, ": 0x");
    print_word(UART1, buf[i]);
    printf_string(UART1, "\n\r");
  }
  printf_string(UART1, "END\n\r");
}

#define SL_SOF 0x7E
#define SL_EOF 0x7E
#define SL_ESCAPE 0x7D
#define SL_XOR 0x20

void serial_link_send_byte_escaped(uint8_t data) {
  switch (data) {
  case SL_SOF:
  case SL_ESCAPE:
    uart_write_byte(UART1, SL_ESCAPE);
    uart_write_byte(UART1, data ^ SL_XOR);
    break;
  default:
    uart_write_byte(UART1, data);
    break;
  }
}

void serial_link_send(uint8_t cmd, const uint8_t *payload,
                      int16_t payload_len) {
  crc_t crc = crc_init();
  uart_write_byte(UART1, SL_SOF);

  uint8_t val = payload_len & 0xff;
  serial_link_send_byte_escaped(val);
  crc = crc_update(crc, &val, 1);

  val = (payload_len << 8) & 0xff;
  serial_link_send_byte_escaped(val);
  crc = crc_update(crc, &val, 1);

  serial_link_send_byte_escaped(cmd);
  crc = crc_update(crc, &cmd, 1);

  for (int i = 0; i < payload_len; i++) {
    serial_link_send_byte_escaped(payload[i]);
  }
  crc = crc_update(crc, &payload[0], payload_len);

  crc = crc_finalize(crc);

  for (int i = 0; i < sizeof(crc_t); i++) {
    serial_link_send_byte_escaped(crc & 0xff);
    crc >>= 8;
  }
  uart_write_byte(UART1, SL_EOF);
}

#define READ_CHAR_COUNT 8

static char buffer[READ_CHAR_COUNT + 1];

volatile static bool uart_receive_finished = false;
volatile static uint16_t data_received_cnt = 0;

static void uart_receive_cb(uint16_t length) {
  data_received_cnt = length;
  uart_receive_finished = true;
}

int main(void) {
  system_init();
  uart_periph_init(UART1);

  // Wait 1 second
  // arch_asm_delay_us(1000 * 1000);

  uart_receive_finished = false;
  data_received_cnt = 0;
  uart_register_rx_cb(UART1, uart_receive_cb);
  uart_receive(UART1, (uint8_t *)buffer, READ_CHAR_COUNT, UART_OP_INTR);

  uint8_t i = 0;
  while (1) {
    uint8_t buf[] = {i, i, i, i};
    serial_link_send(0xee, &buf[0], 4);
    arch_asm_delay_us(1000 * 1000);
    i++;
    if (uart_receive_finished) {
      uart_receive_finished = false;
      uart_receive(UART1, (uint8_t *)buffer, READ_CHAR_COUNT, UART_OP_INTR);
    }
  }

  otp_read_cs();

  // otp_read_hdr();

  // uint32_t buf[CS_WORDS];
  // otp_read(CS_OFFSET, &buf[0], CS_WORDS);

  // printf_string(UART1, "OTP\n\r");
  // for (int i = 0; i < sizeof(buf) / sizeof(uint32_t); i++) {
  //   printf_string(UART1, "0x");
  //   print_word(UART1, MEMORY_OTP_BASE + CS_OFFSET + i * sizeof(uint32_t));
  //   printf_string(UART1, ": 0x");
  //   print_word(UART1, buf[i]);
  //   printf_string(UART1, "\n\r");
  // }
  // printf_string(UART1, "END\n\r");

  uint32_t val = 0;
  otp_read(EMPTY_ADDRESS, &val, 1);
  if (val == -1) {
    // Multiple of 100 us, 10 = 1ms, 1000 = 100ms, 50000 = 2s
    val = 0x80000000 + 20000;
    printf_string(UART1, "address was empty, writing\n\r");
    print_word(UART1, val);
    printf_string(UART1, "\n\r");
    otp_write(EMPTY_ADDRESS, &val, 1);
  } else {
    printf_string(UART1, "val was\n\r");
    print_word(UART1, val);
    printf_string(UART1, "\n\r");
  }

  //  // Setup UART1 pins and configuration
  //  uart_periph_init(UART1);
  //
  //  // Run UART1 print example
  //  uart_print_example(UART1);
  //
  //  // Run UART1 send blocking example
  //  uart_send_blocking_example(UART1);
  //
  //  // Run UART1 send interrupt example
  //  uart_send_interrupt_example(UART1);
  //
  // #if defined(CFG_UART_DMA_SUPPORT)
  //  // Run UART1 send dma example
  //  uart_send_dma_example(UART1);
  // #endif
  //  // Run UART1 receive blocking example
  //  uart_receive_blocking_example(UART1);
  //
  //  // Run UART1 receive interrupt example
  //  uart_receive_interrupt_example(UART1);
  //
  // #if defined(CFG_UART_DMA_SUPPORT)
  //  // Run UART1 receive dma example
  //  uart_receive_dma_example(UART1);
  // #endif
  //
  //  // Run UART1 loopback example
  //  uart_loopback_interrupt_example(UART1);

  while (1)
    ;
}

void uart_print_example(uart_t *uart) {
  printf_string(uart, "\n\r\n\r****************************************\n\r");
  if (uart == UART1) {
    printf_string(uart, "* UART1 Print example *\n\r");
  } else {
    printf_string(uart, "* UART2 Print example *\n\r");
  }
  printf_string(uart, "****************************************\n\r\n\r");
  printf_string(uart, " printf_string() = Hello World! \n\r");
  printf_string(uart, "\n\r print_hword()   = 0x");
  print_hword(uart, 0xAABB);
  printf_string(uart, "\n\r");
  printf_string(uart, "\n\r print_word()    = 0x");
  print_word(uart, 0x11223344);
  printf_string(uart, "\n\r");
  printf_string(uart, "****************************************\n\r\n\r");
}
