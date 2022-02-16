#include "uart.h"
#include "app.hpp"

uint8_t UART_Buff[4150];

#define  dbClient Serial2

void UART_Task(void *pvParameters)
{
  uint16_t idx, x;
  uint16_t len;
  uint16_t frameLen;
  uint32_t frameTimeoutTmr;
  uint32_t frameTimeout = 5000;
  while (1)
  {
    idx = 0;
    len = 0;

    if (Serial.available() >= 2)
    {
      idx = Serial.read(UART_Buff, 2);
      frameLen = ((uint16_t)(UART_Buff[0] & 0x0F) << 8) | (uint16_t)UART_Buff[1];

      StartTimer(frameTimeoutTmr, frameTimeout);
      while ((len < frameLen) && !IsTimerElapsed(frameTimeoutTmr))
      {
        len = Serial.available();
        len = (len > 0) ? len : 0;
        vTaskDelay(pdMS_TO_TICKS(10));
      }

      idx += Serial.read(&UART_Buff[idx], len);
      APP_ProcessData(UART_Buff, idx, APP_MSG_CHANNEL_UART);
      
      while(x = Serial.available())
      {
        while(x--)
          Serial.read();
      }
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void UART_Write(uint8_t *payLoad, uint16_t len)
{
  Serial.write(payLoad, len);
}