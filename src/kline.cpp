#include "kline.hpp"
#include <SoftwareSerial.h>

#define txPin 32
#define rxPin 33

SoftwareSerial swSer1;
KLine_api_msg_t RespMsg;

extern uint8_t PROTOCOL;
extern uint32_t KLIN_RqRspTmr;
extern uint32_t KLIN_RqRspMaxTime;

static uint8_t TxDataPtr[305];
static uint8_t RxDataPtr[305];

static uint8_t Tgt = 0;
static uint8_t Src = 0;
volatile bool ListenRx = false;

static uint8_t checksum(void *buff, uint8_t len);

void KLIN_Task(void *args)
{
    pinMode(txPin, OUTPUT);
    swSer1.begin(10400, SWSERIAL_8N1, rxPin, txPin);
    digitalWrite(txPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));

    uint16_t rxLen = 0;

    while (true)
    {
        if (ListenRx && swSer1.available())
        {
            if (PROTOCOL == APP_KLIN_ISO14230_4KWP_FASTINIT)
            {
                do
                {
                    rxLen = swSer1.available();
                    vTaskDelay(pdMS_TO_TICKS(10));
                } while (rxLen != swSer1.available());

                if (swSer1.readBytes(RxDataPtr, rxLen) > 0)
                {
                    uint16_t idx = (RxDataPtr[0] & 0x3F) ? 4 : 3;
                    RespMsg.data = &RxDataPtr[idx];
                    RespMsg.len = rxLen - idx - 1;
                    RespMsg.NewMsg = true;
                    ListenRx = false;
                    DEBUG("ECU", RxDataPtr, RespMsg.len);
                }
            }
        }

        SEND_KLIN_RESPONSE();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

void KLIN_FastInit()
{
    uint8_t data[] = {0xc1, 0x11, 0xf1, 0x81, 0x44};
    uint8_t len = sizeof(data);

    digitalWrite(txPin, LOW);
    delay(25);
    digitalWrite(txPin, HIGH);
    delay(25);
    swSer1.write(data, len);
    swSer1.readBytes(RxDataPtr, len);
    ListenRx = true;
}

void KLIN_Send(uint8_t *data, uint16_t len)
{

    uint16_t idx = 0;

    TxDataPtr[idx++] = ((len > 64) ? 0x00 : len) | 0xC0; // A1 A0 : 1;
    TxDataPtr[idx++] = Tgt;
    TxDataPtr[idx++] = Src;

    IF((idx > 64), TxDataPtr[idx++] = len;)

    memcpy(&TxDataPtr[idx], data, len);
    idx += len;
    TxDataPtr[idx++] = checksum((void *)TxDataPtr, idx);

    if (swSer1.write(TxDataPtr, idx) > 0)
    {
        DEBUG("DGL", TxDataPtr, idx);
    }

    if (swSer1.readBytes(RxDataPtr, idx) > 0)
    {
        DEBUG("ECU", RxDataPtr, idx);
    }
    ListenRx = true;
    StartTimer(KLIN_RqRspTmr, KLIN_RqRspMaxTime);
}

void KLIN_SetTgt(uint8_t data)
{
    Tgt = data;
}

void KLIN_SetSrc(uint8_t data)
{
    Src = data;
}

static uint8_t checksum(void *buff, uint8_t len)
{
    uint8_t CS = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        CS += reinterpret_cast<uint8_t *>(buff)[i];
    }
    return CS;
}
