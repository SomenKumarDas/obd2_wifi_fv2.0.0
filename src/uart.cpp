#include "uart.h"
#include "app.hpp"

static SemaphoreHandle_t Txcomp_mx;
static portMUX_TYPE muxCritical = portMUX_INITIALIZER_UNLOCKED;
static uint8_t *rxData = NULL;
static int32_t rxLen = 0, rxIdx = 0, rxFrameLen = 0;
static uint32_t ListnerTmr;

static inline void listnerRefresh()
{
    rxIdx = 0;
    rxFrameLen = 0;
    StopTimer(ListnerTmr);
}

void UART_Task(void *pvParameters)
{
    IF((rxData = (uint8_t *)new (uint8_t[MAX_DATA_LEN])) == NULL, goto _EXIT_;)
    IF((Txcomp_mx = xSemaphoreCreateMutex()) == NULL, goto _EXIT_;)
    while (1)
    {
        if (Serial.available())
        {
            taskENTER_CRITICAL(&muxCritical);
            if (rxIdx == 0 && Serial.available() >= 2)
            {
                rxIdx = Serial.read(&rxData[rxIdx], 2);
                rxFrameLen = (((uint16_t)(rxData[0] & 0x0F) << 8) | (uint16_t)rxData[1]) + 2;
                StartTimer(ListnerTmr, 5000);
            }
            else if ((rxFrameLen - rxIdx) > 0u)
            {
                rxIdx += Serial.read(&rxData[rxIdx], (rxFrameLen - rxIdx));
            }
            taskEXIT_CRITICAL(&muxCritical);
            IF((rxFrameLen == rxIdx),
            //    _ST_(DEBUG("USB", rxData, rxFrameLen))
               _ST_(APP_ProcessData(rxData, rxFrameLen, APP_MSG_CHANNEL_UART))
                   _ST_(listnerRefresh()));
        }
        else
        {
            IF(IsTimerElapsed(ListnerTmr), _ST_(listnerRefresh()))
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
_EXIT_:
    Serial.printf("[E] [USB] [UART_Task Exit]\r\n");
    vTaskDelete(NULL);
}

void UART_Write(uint8_t *payLoad, uint16_t len)
{
    xSemaphoreTake(Txcomp_mx, portMAX_DELAY);

    Serial.write(payLoad, len);
    // _ST_(DEBUG("DGL", payLoad, len))

    xSemaphoreGive(Txcomp_mx);
}