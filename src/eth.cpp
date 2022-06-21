#include "eth.hpp"

uint8_t mac[6] = {0xFF, 0x01, 0xFF, 0x03, 0x04, 0x05};
uint8_t myIP[4] = {192, 168, 0, 120};
uint8_t Eth_Buff[4100];

EthernetClient client;
EthernetServer server = EthernetServer(2020);

void eth_Task(void *pv)
{

    Ethernet.begin(mac);
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println(Ethernet.localIP());
    server.begin();

    uint16_t idx, x;
    uint16_t len;
    uint16_t frameLen;
    uint32_t frameTimeoutTmr;
    uint32_t frameTimeout = 5000;
    while (1)
    {
        client = server.available();
        idx = 0;
        len = 0;

        if (client.connected())
        {
            Serial.println("New Client Connected");
            while (client.connected())
            {
                if (client.available() >= 2)
                {
                    idx = client.read(Eth_Buff, 2);
                    frameLen = ((uint16_t)(Eth_Buff[0] & 0x0F) << 8) | (uint16_t)Eth_Buff[1];

                    StartTimer(frameTimeoutTmr, frameTimeout);
                    while ((len < frameLen) && !IsTimerElapsed(frameTimeoutTmr))
                    {
                        len = client.available();
                        len = (len > 0) ? len : 0;
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }

                    idx += client.read(&Eth_Buff[idx], len);
                    APP_ProcessData(Eth_Buff, idx, APP_MSG_CHANNEL_ETH);

                    while (x = client.available())
                    {
                        while (x--)
                            client.read();
                    }
                }
                vTaskDelay(5 / portTICK_PERIOD_MS);
            }
            Serial.println("Client DisConnected");
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void eth_Write(uint8_t *payLoad, uint16_t len)
{
    if (client.connected())
    {
        client.write(payLoad, len);
    }
}