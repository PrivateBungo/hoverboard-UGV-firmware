#pragma once

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef struct __attribute__((packed)) {
  uint16_t start;
  int16_t steer;
  int16_t speed;
  uint16_t checksum;
} SerialCommand;

typedef struct __attribute__((packed)) {
  uint16_t start;
  int16_t cmd1;
  int16_t cmd2;
  int16_t speedR_meas;
  int16_t speedL_meas;
  uint16_t batVoltage;
  int16_t boardTemp;
  uint16_t cmdLed;
  uint16_t checksum;
} SerialFeedback;

uint16_t serial_cmd_checksum(const SerialCommand *cmd);
uint16_t serial_feedback_checksum(const SerialFeedback *feedback);

void serial_parser_reset(void);
uint8_t serial_parser_process_byte(uint8_t byte, SerialCommand *out_cmd);

void uart_comms_init(void);
void uart_control_rx_check(void);
uint8_t uart_control_get_command(int16_t *steer, int16_t *speed);
uint32_t uart_control_get_last_valid_ms(void);
void uart_feedback_periodic(int16_t cmd1, int16_t cmd2, int16_t speedR, int16_t speedL, uint16_t batVoltage, int16_t boardTemp, uint16_t cmdLed);
