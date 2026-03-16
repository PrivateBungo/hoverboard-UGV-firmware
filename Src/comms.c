#include "stm32f1xx_hal.h"
#include "defines.h"
#include "setup.h"
#include "config.h"
#include "util.h"
#include "stdio.h"
#include "string.h"

extern UART_HandleTypeDef huart2;

#ifdef DEBUG_SERIAL_USART3
#define UART_DMA_CHANNEL DMA1_Channel2
#endif

#ifdef DEBUG_SERIAL_USART2
#define UART_DMA_CHANNEL DMA1_Channel7
#endif

#ifdef CONTROL_SERIAL_USART2
#define UART2_RX_DMA_BUF_LEN 64
static uint8_t uart2_rx_dma_buf[UART2_RX_DMA_BUF_LEN];
static uint16_t uart2_rx_old_pos;
static int16_t latest_cmd_steer;
static int16_t latest_cmd_speed;
static uint8_t latest_cmd_valid;
static uint32_t last_valid_command_ms;
#endif

volatile uint8_t uart_buf[100];
volatile int16_t ch_buf[8];

void setScopeChannel(uint8_t ch, int16_t val) {
  ch_buf[ch] = val;
}

void uart_comms_init(void) {
#ifdef CONTROL_SERIAL_USART2
  serial_parser_reset();
  uart2_rx_old_pos = 0;
  latest_cmd_steer = 0;
  latest_cmd_speed = 0;
  latest_cmd_valid = 0;
  last_valid_command_ms = 0;
  HAL_UART_Receive_DMA(&huart2, uart2_rx_dma_buf, UART2_RX_DMA_BUF_LEN);
#endif
}

void uart_control_rx_check(void) {
#ifdef CONTROL_SERIAL_USART2
  uint16_t pos;

  if (huart2.hdmarx == NULL) {
    return;
  }

  pos = UART2_RX_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(huart2.hdmarx);
  if (pos != uart2_rx_old_pos) {
    SerialCommand command;

    if (pos > uart2_rx_old_pos) {
      for (uint16_t i = uart2_rx_old_pos; i < pos; i++) {
        if (serial_parser_process_byte(uart2_rx_dma_buf[i], &command)) {
          latest_cmd_steer = CLAMP(command.steer, -1000, 1000);
          latest_cmd_speed = CLAMP(command.speed, -1000, 1000);
          latest_cmd_valid = 1;
          last_valid_command_ms = HAL_GetTick();
        }
      }
    } else {
      for (uint16_t i = uart2_rx_old_pos; i < UART2_RX_DMA_BUF_LEN; i++) {
        if (serial_parser_process_byte(uart2_rx_dma_buf[i], &command)) {
          latest_cmd_steer = CLAMP(command.steer, -1000, 1000);
          latest_cmd_speed = CLAMP(command.speed, -1000, 1000);
          latest_cmd_valid = 1;
          last_valid_command_ms = HAL_GetTick();
        }
      }
      for (uint16_t i = 0; i < pos; i++) {
        if (serial_parser_process_byte(uart2_rx_dma_buf[i], &command)) {
          latest_cmd_steer = CLAMP(command.steer, -1000, 1000);
          latest_cmd_speed = CLAMP(command.speed, -1000, 1000);
          latest_cmd_valid = 1;
          last_valid_command_ms = HAL_GetTick();
        }
      }
    }

    uart2_rx_old_pos = pos;
  }
#endif
}

uint8_t uart_control_get_command(int16_t *steer, int16_t *speed) {
#ifdef CONTROL_SERIAL_USART2
  if (!latest_cmd_valid) {
    return 0;
  }
  if (steer != NULL) {
    *steer = latest_cmd_steer;
  }
  if (speed != NULL) {
    *speed = latest_cmd_speed;
  }
  return 1;
#else
  (void)steer;
  (void)speed;
  return 0;
#endif
}

uint32_t uart_control_get_last_valid_ms(void) {
#ifdef CONTROL_SERIAL_USART2
  return last_valid_command_ms;
#else
  return 0;
#endif
}

void uart_feedback_periodic(int16_t cmd1, int16_t cmd2, int16_t speedR, int16_t speedL, uint16_t batVoltage, int16_t boardTemp, uint16_t cmdLed) {
#if defined(CONTROL_SERIAL_USART2) && defined(FEEDBACK_SERIAL_USART2)
  static uint32_t last_feedback_tick;
  static SerialFeedback feedback;

  if ((HAL_GetTick() - last_feedback_tick) < SERIAL_FEEDBACK_INTERVAL_MS) {
    return;
  }

  if (HAL_UART_GetState(&huart2) != HAL_UART_STATE_READY) {
    return;
  }

  feedback.start = START_FRAME;
  feedback.cmd1 = cmd1;
  feedback.cmd2 = cmd2;
  feedback.speedR_meas = speedR;
  feedback.speedL_meas = speedL;
  feedback.batVoltage = batVoltage;
  feedback.boardTemp = boardTemp;
  feedback.cmdLed = cmdLed;
  feedback.checksum = serial_feedback_checksum(&feedback);

  if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&feedback, sizeof(feedback)) == HAL_OK) {
    last_feedback_tick = HAL_GetTick();
  }
#else
  (void)cmd1;
  (void)cmd2;
  (void)speedR;
  (void)speedL;
  (void)batVoltage;
  (void)boardTemp;
  (void)cmdLed;
#endif
}

void consoleScope() {
  #if defined DEBUG_SERIAL_SERVOTERM && (defined DEBUG_SERIAL_USART2 || defined DEBUG_SERIAL_USART3)
    uart_buf[0] = 0xff;
    uart_buf[1] = CLAMP(ch_buf[0]+127, 0, 255);
    uart_buf[2] = CLAMP(ch_buf[1]+127, 0, 255);
    uart_buf[3] = CLAMP(ch_buf[2]+127, 0, 255);
    uart_buf[4] = CLAMP(ch_buf[3]+127, 0, 255);
    uart_buf[5] = CLAMP(ch_buf[4]+127, 0, 255);
    uart_buf[6] = CLAMP(ch_buf[5]+127, 0, 255);
    uart_buf[7] = CLAMP(ch_buf[6]+127, 0, 255);
    uart_buf[8] = CLAMP(ch_buf[7]+127, 0, 255);
    uart_buf[9] = '\n';

    if(UART_DMA_CHANNEL->CNDTR == 0) {
      UART_DMA_CHANNEL->CCR &= ~DMA_CCR_EN;
      UART_DMA_CHANNEL->CNDTR = 10;
      UART_DMA_CHANNEL->CMAR  = (uint32_t)uart_buf;
      UART_DMA_CHANNEL->CCR |= DMA_CCR_EN;
    }
  #endif

  #if defined DEBUG_SERIAL_ASCII && (defined DEBUG_SERIAL_USART2 || defined DEBUG_SERIAL_USART3)
    memset((void *)uart_buf, 0, sizeof(uart_buf));
    sprintf((char *)uart_buf, "1:%i 2:%i 3:%i 4:%i 5:%i 6:%i 7:%i 8:%i\r\n", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5], ch_buf[6], ch_buf[7]);

    if(UART_DMA_CHANNEL->CNDTR == 0) {
      UART_DMA_CHANNEL->CCR &= ~DMA_CCR_EN;
      UART_DMA_CHANNEL->CNDTR = strlen((char *)uart_buf);
      UART_DMA_CHANNEL->CMAR  = (uint32_t)uart_buf;
      UART_DMA_CHANNEL->CCR |= DMA_CCR_EN;
    }
  #endif
}

void consoleLog(char *message)
{
    HAL_UART_Transmit_DMA(&huart2, (uint8_t *)message, strlen(message));
}
