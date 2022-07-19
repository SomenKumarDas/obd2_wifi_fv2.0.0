#include "eth.hpp"

static EthernetClient client;
static EthernetServer server = EthernetServer(6888);
static uint8_t ip[] = {192, 168, 1, 100};
static uint8_t mac[6] = {0x9C, 0x9C, 0x1F, 0x27, 0xDE, 0xE0};
static uint8_t *rxData = NULL;
static int32_t rxLen = 0, rxIdx = 0, rxFrameLen = 0;
static uint32_t ListnerTmr;

static inline void listnerRefresh()
{
    rxIdx = 0;
    rxFrameLen = 0;
    StopTimer(ListnerTmr);
}

void eth_Task(void *pv)
{
    _ST_(rxData = (uint8_t *)new (uint8_t[4100]))
    Ethernet.begin(mac); Serial.printf("[ETH IP: %s]\r\n", Ethernet.localIP().toString().c_str());
    server.begin();

    while (1)
    {
        client = server.available();
        if (client.connected())
        {
            Serial.printf("[ETH] [CONNECTED]\r\n");
            while (client.connected())
            {
                Ethernet.maintain();
                if (client.available())
                {
                    if (rxIdx == 0 && client.available() >= 2)
                    {
                        rxIdx = client.read(&rxData[rxIdx], 2);
                        rxFrameLen = (((uint16_t)(rxData[0] & 0x0F) << 8) | (uint16_t)rxData[1]) + 2;
                        StartTimer(ListnerTmr, 3000);
                        log_printf("[APP] [New frameLen: %u]\r\n", rxFrameLen);
                    }
                    else if ((rxFrameLen - rxIdx) > 0u)
                    {
                        rxIdx += client.read(&rxData[rxIdx], (rxFrameLen - rxIdx));
                        log_printf("[APP] [Cur idx: %u]\r\n", rxIdx);
                        IF((rxFrameLen == rxIdx),
                           _ST_(DEBUG("APP", rxData, rxFrameLen))
                               _ST_(APP_ProcessData(rxData, rxFrameLen, APP_MSG_CHANNEL_ETH))
                                   _ST_(listnerRefresh()));
                    }
                    else
                    {
                        Serial.printf("[W] [ISSUE]\r\n");
                    }
                }
                else
                {
                    IF(IsTimerElapsed(ListnerTmr), _ST_(Serial.printf("[W] [TM OUT]\r\n")) _ST_(listnerRefresh()))
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
            Serial.printf("[ETH] [DISCONNECTED]\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

void eth_Write(uint8_t *payLoad, uint16_t len)
{
    IF(client.connected(),
       _ST_(client.write(payLoad, len))
           _ST_(DEBUG("DGL", payLoad, len)))
}