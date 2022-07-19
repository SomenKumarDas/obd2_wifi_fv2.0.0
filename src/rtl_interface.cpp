#include "app.hpp"

uint8_t DataBuff[4100];

void rtl_interface_Task(void *pvParameters)
{
    RTL_DATA_CH.begin(460800);
    RTL_DATA_CH.setRxBufferSize(4100);
    int avlData = 0, RxDataLen = 0;
    uint16_t idx = 0, frameLen = 0;
    uint32_t frameTimeoutTmr, frameTimeout = 5000;
    for (;;)
    {
        if (RTL_DATA_CH.available())
        {
            if (idx == 0 && RTL_DATA_CH.available() >= 2)
            {
                idx = RTL_DATA_CH.read(DataBuff, 2);
                frameLen = ((uint16_t)(DataBuff[0] & 0x0F) << 8) | (uint16_t)DataBuff[1];
                log_printf("[APP] [New frameLen: %u]\r\n", frameLen);
            }
            else if (idx)
            {
                idx += RTL_DATA_CH.read(&DataBuff[idx], (frameLen));
                log_printf("[APP] [Cur idx: %u]\r\n", idx);
                if (idx >= (frameLen + 2))
                {
                    log_printf("[APP] [FR rcvd:]");
                    for (int i = 0; i < ((idx > 10) ? 10 : (idx)); i++)
                    {
                        Serial.printf("%02x", DataBuff[i]);
                    }
                    Serial.println();
                    APP_ProcessData(DataBuff, idx, APP_MSG_CHANNEL_RTL);
                    idx = 0;
                    frameLen = 0;
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    vTaskDelete(NULL);
}

void rtl_interface_write(uint8_t *payLoad, uint16_t len)
{
    RTL_DATA_CH.write(payLoad, len);
}