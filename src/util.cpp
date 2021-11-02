#include "util.h"
#include "app.hpp"
#include <Preferences.h>

#define MAJOR_VERSION 2
#define MINOR_VERSION 0
#define SUB_VERSION 1

bool secWifiLedTask_b = false;

struct FLAG FLAG;

void SecWifiLedTask(void *pv);
void ComLedTask(void *pv);
void SW_OPERATION_TASK(void *pv);
void GPIO_CONFIG(void);

TaskHandle_t comLedTask_h;
TaskHandle_t secWifiLedTask_h;

// int tim1, tim2, tim3;
// bool l1,l2,l3;

// uint16_t LedTmr1, LedTmr2, LedTm3;

void IO_OPERATION_TASK(void *pv)
{
  GPIO_CONFIG();

  if (xTaskCreate(SecWifiLedTask, "SecWifiLedTask", 1024 * 2, NULL, tskIDLE_PRIORITY, &secWifiLedTask_h) != pdTRUE)
  {
    Serial.println("Failed to Create SecWifiLedTask");
  }

  if (xTaskCreate(ComLedTask, "APP_TASK", 1024 * 2, NULL, tskIDLE_PRIORITY, &comLedTask_h) != pdTRUE)
  {
    Serial.println("Failed to Create ComLedTask");
  }

  if (xTaskCreate(SW_OPERATION_TASK, "SW_OPERATION_TASK", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
  {
    Serial.println("Failed to Create SW_OPERATION_TASK");
  }

  vTaskDelete(NULL);
}

void OTA_LED_TASK(void *args)
{

  if(!secWifiLedTask_b)
    vTaskDelete(secWifiLedTask_h);
  
  vTaskDelete(comLedTask_h);

  for (;;)
  {
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, HIGH);
    digitalWrite(LED_3, HIGH);
    digitalWrite(LED_4, HIGH);
    digitalWrite(LED_5, HIGH);
    vTaskDelay(pdMS_TO_TICKS(250));
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
    digitalWrite(LED_5, LOW);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void GPIO_CONFIG(void)
{
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);
  pinMode(LED_6, OUTPUT);
  pinMode(LED_7, OUTPUT);
  pinMode(SW_1, INPUT);

  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, HIGH);
  digitalWrite(LED_3, HIGH);
  digitalWrite(LED_4, HIGH);
  digitalWrite(LED_5, HIGH);
  digitalWrite(LED_6, HIGH);
  digitalWrite(LED_7, HIGH);
}

void SW_OPERATION_TASK(void *pv)
{
  int btn_cnt = 0;
  while (1)
  {
    if (!digitalRead(SW_1))
    {
      btn_cnt++;
      vTaskDelay(300 / portTICK_PERIOD_MS);
      digitalWrite(LED_1, HIGH);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(LED_1, LOW);
    }
    else
    {
      if (btn_cnt == 2)
      {
        ESP.restart();
      }
      else if (btn_cnt == 4)
      {
        Serial.println("Deleted config USER");
        Preferences preferences;
        preferences.begin("config", false);
        preferences.remove("stSSID[1]");
        preferences.remove("stPASS[1]");
        preferences.end();
        ESP.restart();
      }
      else if (btn_cnt == 6)
      {
        Serial.println("Deleted config USER & DEFAULT");
        Preferences preferences;
        preferences.begin("config", false);
        preferences.remove("stSSID[0]");
        preferences.remove("stPASS[0]");
        preferences.remove("stSSID[1]");
        preferences.remove("stPASS[1]");
        preferences.end();
        ESP.restart();
      }
      btn_cnt = 0;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void SecWifiLedTask(void *pv) 
{
  while (1)
  {
    if (!FLAG.SECURITY)
      digitalWrite(LED_3, HIGH);
    if (!FLAG.WIFI)
      digitalWrite(LED_2, HIGH);
    vTaskDelay(499 / portTICK_PERIOD_MS);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_2, LOW);
    vTaskDelay(499 / portTICK_PERIOD_MS);
  }
  secWifiLedTask_b = true;
  vTaskDelete(NULL);
}

void ComLedTask(void *pv)
{
  for (;;)
  {
    if (FLAG.APP_COM)
      digitalWrite(LED_4, LOW);
    if (FLAG.CAN_COM)
      digitalWrite(LED_5, LOW);

    vTaskDelay(50 / portTICK_PERIOD_MS);

    digitalWrite(LED_4, HIGH);
    digitalWrite(LED_5, HIGH);
    FLAG.APP_COM = false;
    FLAG.CAN_COM = false;

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
