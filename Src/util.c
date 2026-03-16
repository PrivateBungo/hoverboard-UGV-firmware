#include <string.h>

#include "config.h"
#include "util.h"

#define SERIAL_COMMAND_FRAME_SIZE ((uint8_t)sizeof(SerialCommand))

static uint8_t parser_buf[SERIAL_COMMAND_FRAME_SIZE];
static uint8_t parser_count;

uint16_t serial_cmd_checksum(const SerialCommand *cmd) {
  return (uint16_t)(cmd->start ^ (uint16_t)cmd->steer ^ (uint16_t)cmd->speed);
}

uint16_t serial_feedback_checksum(const SerialFeedback *feedback) {
  return (uint16_t)(feedback->start ^ (uint16_t)feedback->cmd1 ^ (uint16_t)feedback->cmd2 ^
                    (uint16_t)feedback->speedR_meas ^ (uint16_t)feedback->speedL_meas ^
                    feedback->batVoltage ^ (uint16_t)feedback->boardTemp ^ feedback->cmdLed);
}

void serial_parser_reset(void) {
  parser_count = 0;
  memset(parser_buf, 0, sizeof(parser_buf));
}

uint8_t serial_parser_process_byte(uint8_t byte, SerialCommand *out_cmd) {
  SerialCommand candidate;

  if (parser_count < SERIAL_COMMAND_FRAME_SIZE) {
    parser_buf[parser_count++] = byte;
  } else {
    memmove(&parser_buf[0], &parser_buf[1], SERIAL_COMMAND_FRAME_SIZE - 1);
    parser_buf[SERIAL_COMMAND_FRAME_SIZE - 1] = byte;
  }

  if (parser_count < SERIAL_COMMAND_FRAME_SIZE) {
    return 0;
  }

  memcpy(&candidate, parser_buf, SERIAL_COMMAND_FRAME_SIZE);
  if (candidate.start != START_FRAME) {
    return 0;
  }

  if (candidate.checksum != serial_cmd_checksum(&candidate)) {
    return 0;
  }

  if (out_cmd != NULL) {
    *out_cmd = candidate;
  }

  parser_count = 0;
  return 1;
}
