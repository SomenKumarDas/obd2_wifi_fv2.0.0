#ifndef __TCPSOC_H__
#define __TCPSOC_H__

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>


#ifdef __cplusplus
extern "C" {
#endif

void WIFI_Task(void *pvParameters);
void FOTA_TASK(void *args);

void WIFI_TCP_Soc_Write(uint8_t *payLoad, uint16_t len);

bool WIFI_Set_STA_SSID(uint8_t idx, char *p_str);
bool WIFI_Set_STA_Pass(uint8_t idx, char *p_str);



#ifdef __cplusplus
}
#endif

#endif