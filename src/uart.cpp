#include "uart.h"
#include "app.hpp"

uint8_t UART_Buff[4150];

// WiFiServer dbServer(5500);
// WiFiClient dbClient;
/*
void UART_Task(void *pvParameters)
{
  uint16_t idx;
  uint16_t len;

  while (1)
  {
    idx = 0;
    len = 0;
    while (len != Serial.available())
    {
      len = Serial.available();
      vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    if (len && (len < sizeof(UART_Buff)))
    {
      while (len--)
      {
        UART_Buff[idx++] = Serial.read();
      }

      APP_ProcessData(UART_Buff, idx, APP_MSG_CHANNEL_UART);
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}
*/

void UART_Task(void *pvParameters)
{
  uint16_t idx;
  uint16_t len;
  uint16_t frameLen;
  uint32_t frameTimeoutTmr;
  uint32_t frameTimeout = 5000;

  // dbServer.begin();

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

      // dbClient.printf("APP -> [%u] ", idx);
      // for (int i = 0; i < ((idx > 10) ? 10 : (idx)); i++)
      // {
      //   dbClient.printf("%02x", UART_Buff[i]);
      // }
      // dbClient.println("");

      APP_ProcessData(UART_Buff, idx, APP_MSG_CHANNEL_UART);

      // Serial.printf("Read: %u\r\n",idx);

      // if(len == frameLen)
      // {
      //   Serial.println("VALID FRAME LEN");
      // }
      // else{
      //   Serial.printf("INVALID FRAME LEN: READ [%03x] EXP [%03x]\r\n", idx, frameLen+2);
      // }

      // // Serial.printf("APP -> [%u] ", idx);
      // // for(int i = 0; i < len +  2; i++)
      // // {
      // //   Serial.printf("%02x", UART_Buff[i]);
      // // }
      // // Serial.println("");

      Serial.flush();
    }

    // if (!dbClient.connected())
    // {
    //   dbClient = dbServer.available();
    // }

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void UART_Write(uint8_t *payLoad, uint16_t len)
{
  Serial.write(payLoad, len);
}