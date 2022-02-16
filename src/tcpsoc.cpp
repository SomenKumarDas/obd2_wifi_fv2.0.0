#include <ESPmDNS.h>
#include "util.h"
#include "tcpsoc.h"
#include "app.hpp"
#include "esp_wifi.h"

WiFiClient WIFI_Client;
WiFiServer SocketServer(6888);
SemaphoreHandle_t WIFI_SemTCP_SocComplete;

uint8_t WIFI_RxBuff[4200];
uint8_t WIFI_TxBuff[4200];
uint16_t WIFI_TxLen;
uint8_t WIFI_SeqNo;
bool userssid = false;
long req_time = 0;
char WIFI_AP_SSID[50] = "";
char WIFI_AP_Password[50] = "password1";

extern String OTA_URL;

void WIFI_Init(void);
void WIFI_SupportTask(void *pvParameters);

void WIFI_Task(void *pvParameters)
{
  WIFI_Init();

  if ((WIFI_SemTCP_SocComplete == NULL))
    vTaskDelete(NULL);

  SocketServer.begin();

  while (1)
  {
    WIFI_Client = SocketServer.available();

    if (WIFI_Client.connected() == true)
    {
      int32_t len;
      int32_t idx = 0;
      uint32_t frameLen;
      uint32_t frameTimeoutTmr;
      uint32_t frameTimeout = 5000;
      WIFI_Client.setTimeout(86400);
      ESP_LOGI("TCP", "Socket Client connected");

      while (WIFI_Client.connected())
      {
        idx = 0;
        len = 0;

        if (WIFI_Client.available() >= 3)
        {
          idx = WIFI_Client.read(WIFI_RxBuff, 3);
          frameLen = ((uint16_t)WIFI_RxBuff[1] << 8) | (uint16_t)WIFI_RxBuff[2];

          StartTimer(frameTimeoutTmr, frameTimeout);
          while ((len < frameLen))
          {
            len = WIFI_Client.available();
            len = (len > 0) ? len : 0;
            if (IsTimerElapsed(frameTimeoutTmr))
            {
              // Serial.print("<FRAME TIME OUT> ");
              // Serial.printf("Read: [%u] Exp: [%u]\r\n", len, frameLen);
              break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
          }

          // if(len != frameLen)
          // {
          //   Serial.println("INVALID FRAME LEN");
          //   Serial.printf("Read: [%u] Exp: [%u]\r\n", len, frameLen);
          // }

          idx += WIFI_Client.read(&WIFI_RxBuff[idx], len);

          // Serial.printf("APP -> [%u] ", (idx - 13));
          // for(int i = 0; i < ((idx - 13 > 10) ? 10 : (idx - 13)); i++)
          // {
          //   Serial.printf("%02x", WIFI_RxBuff[11 + i]);
          // }
          // Serial.println("");

          APP_ProcessData(&WIFI_RxBuff[11], (idx - 13), APP_MSG_CHANNEL_TCP_SOC);

          WIFI_Client.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
      }
      ESP_LOGI("TCP", "Socket Client disconnected");
      WIFI_Client.stop();
      FLAG.SECURITY = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void WIFI_TCP_Soc_Write(uint8_t *payLoad, uint16_t len)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))

  uint16_t crc16;
  uint32_t idx;
  uint32_t tick;

  if (WIFI_SemTCP_SocComplete == NULL)
  {
    return;
  }

  xSemaphoreTake(WIFI_SemTCP_SocComplete, portMAX_DELAY);

  // if (WIFI_TxLen == 0)
  {
    idx = 0;
    if ((payLoad[0] & 0xF0) == 0x20)
    {
      WIFI_TxBuff[idx++] = WIFI_SeqNo;
    }
    else
    {
      WIFI_TxBuff[idx++] = 0xFF;
    }

    WIFI_TxBuff[idx++] = byte(len, 1);
    WIFI_TxBuff[idx++] = byte(len, 0);
    tick = xTaskGetTickCount();
    WIFI_TxBuff[idx++] = byte(tick, 3);
    WIFI_TxBuff[idx++] = byte(tick, 2);
    WIFI_TxBuff[idx++] = byte(tick, 1);
    WIFI_TxBuff[idx++] = byte(tick, 0);
    memset(&WIFI_TxBuff[idx], 0, 4);
    idx = idx + 4;
    memcpy(&WIFI_TxBuff[idx], payLoad, len);
    idx = idx + len;
    crc16 = UTIL_CRC16_CCITT(0xFFFF, WIFI_TxBuff, len + 11);
    WIFI_TxBuff[idx++] = byte(crc16, 1);
    WIFI_TxBuff[idx++] = byte(crc16, 0);
    WIFI_TxLen = idx;

    if (WIFI_Client.connected() == true)
    {
      if (WIFI_Client.write(WIFI_TxBuff, WIFI_TxLen) > 0)
      {
        // Serial.printf("DGL <- [%u] ", len);
        // for (int i = 0; i < ((len > 10) ? 10 : len); i++)
        //   Serial.printf("%02x", payLoad[i]);
        // Serial.println("\n");
      }
    }
    else
    {
      Serial.println("INFO: TCP Socket Client disconnected during send");
    }

    WIFI_TxLen = 0;
  }

  xSemaphoreGive(WIFI_SemTCP_SocComplete);
}

void WIFI_Init(void)
{
  char mac[6];
  Preferences preferences;
  bool staConnected = false;

  esp_wifi_set_ps(WIFI_PS_NONE);
  WIFI_SemTCP_SocComplete = xSemaphoreCreateBinary();

  if ((WIFI_SemTCP_SocComplete == NULL))
  {
    ESP_LOGE("Failed to create Soc Tx complete semaphore");
  }
  else
  {
    xSemaphoreGive(WIFI_SemTCP_SocComplete);
  }

  preferences.begin("config", false);

  ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());
  WiFi.macAddress((uint8_t *)mac);
  sprintf(WIFI_AP_SSID, "OBD2-%02X%02X", mac[4], mac[5]);
  Serial.printf("INFO: AP SSID <%s>\r\n", WIFI_AP_SSID);

  Serial.printf("INFO: USER SSID <%s>\r\n", preferences.getString("stSSID[1]").c_str());
  Serial.printf("INFO: DEFAULT SSID <%s>\r\n", preferences.getString("stSSID[0]").c_str());

  if ((preferences.getString("stSSID[1]") != "") && (strnlen(preferences.getString("stSSID[1]").c_str(), 50) < 50))
  {
    if ((preferences.getString("stPASS[1]") != "") && (strnlen(preferences.getString("stPASS[1]").c_str(), 50) < 50))
    {
      WiFi.begin((char *)preferences.getString("stSSID[1]").c_str(), (char *)preferences.getString("stPASS[1]").c_str());
      ESP_LOGI("TCP", "CONNECTING USER SSID <%s>\r\n", preferences.getString("stSSID[1]").c_str());
      Serial.printf("INFO: CONNECTING USER SSID <%s>\r\n", preferences.getString("stSSID[1]").c_str());
      staConnected = true;
      userssid = true;
    }
  }

  if (!userssid)
  {
    if ((preferences.getString("stSSID[0]") != "") && (strnlen(preferences.getString("stSSID[0]").c_str(), 50) < 50))
    {
      if ((preferences.getString("stPASS[0]") != "") && (strnlen(preferences.getString("stPASS[0]").c_str(), 50) < 50))
      {
        WiFi.begin((char *)preferences.getString("stSSID[0]").c_str(), (char *)preferences.getString("stPASS[0]").c_str());
        ESP_LOGI("TCP", "CONNECTING DEFAULT SSID <%s>\r\n", preferences.getString("stSSID[0]").c_str());
        Serial.printf("INFO: CONNECTING DEFAULT SSID <%s>\r\n", preferences.getString("stSSID[0]").c_str());
        staConnected = true;
      }
    }
  }

  if (MDNS.begin("obd2") == true)
  {
    ESP_LOGI("TCP", "mDNS responder started");
    MDNS.addService("_http", "_tcp", 80);
  }

  if (staConnected == false)
  {
    Serial.printf("INFO: WIFI AP begin <%s>\r\n", WIFI_AP_SSID);
    if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_Password) == true)
    {
      ESP_LOGI("WIFI", "AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
      Serial.printf("IP <%s>\r\n", WiFi.softAPIP().toString().c_str());
    }
  }
  else
  {
    if (xTaskCreate(WIFI_SupportTask, "WIFI_SupportTask", 5000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
      configASSERT(0);
  }
  WiFi.setHostname("Autopeepal_OBD2");
  preferences.end();
}

void WIFI_SupportTask(void *pvParameters)
{
  wl_status_t wifiStatus = WL_NO_SHIELD;
  uint8_t userModeTimeout = 0;
  Preferences preferences;

  while (1)
  {
    wifiStatus = WiFi.status();

    switch (wifiStatus)
    {
    case WL_CONNECTED:
      if ((!FLAG.WIFI))
        Serial.printf("INFO: ST MODE CONNECTED <%s>\r\n", WiFi.localIP().toString().c_str());
      FLAG.WIFI = true;
      break;

    default:
      FLAG.WIFI = false;
      userModeTimeout++;
      WIFI_Client.stop();
      if (userModeTimeout == 2)
      {
        if (userssid)
        {
          preferences.begin("config", false);
          WiFi.begin((char *)preferences.getString("stSSID[1]").c_str(), (char *)preferences.getString("stPASS[1]").c_str());
          preferences.end();
        }
        else
        {
          
          WiFi.begin((char *)preferences.getString("stSSID[0]").c_str(), (char *)preferences.getString("stPASS[0]").c_str());
          // if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_Password) == true)
          // {
          //   ESP_LOGI("WIFI", "AP IP address: %s\r\n", WiFi.softAPIP().toString().c_str());
          //   vTaskDelete(NULL);
          // }
        }
        userModeTimeout = 0;
      }
      break;
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

bool WIFI_Set_STA_SSID(uint8_t idx, char *p_str)
{
  bool ret = false;
  char key[20];

  if (idx < 2)
  {
    Preferences preferences;
    snprintf(key, sizeof(key), "stSSID[%d]", idx);
    preferences.begin("config", false);
    preferences.putString((const char *)key, p_str);
    preferences.end();
    // Serial.printf("Updated SSID at <%d>\r\n", idx);
    ret = true;
  }
  return ret;
}

bool WIFI_Set_STA_Pass(uint8_t idx, char *p_str)
{
  bool ret = false;
  char key[20];

  if (idx < 2)
  {
    Preferences preferences;
    snprintf(key, sizeof(key), "stPASS[%d]", idx);
    preferences.begin("config", false);
    preferences.putString((const char *)key, p_str);
    preferences.end();
    ret = true;
    // Serial.printf("Updated Password at <%d>\r\n", idx);
  }

  return ret;
}
