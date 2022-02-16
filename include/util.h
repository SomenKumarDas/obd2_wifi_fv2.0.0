#ifndef __UTIL_H__
#define __UTIL_H__

#include <Arduino.h>


#define DEBUG_PRINT 0

#if DEBUG_PRINT
#define Serial_println(...)     Serial.println(__VA_ARGS__)   
#define Serial_print(...)       Serial.print(__VA_ARGS__)   
#define Serial_printf(...)      Serial.printf(__VA_ARGS__) 
#else
#define Serial_println(...)
#define Serial_print(...)      
#define Serial_printf(...)      
#endif

#define xstr(s) str(s)
#define str(s) #s

struct FLAG
{
  bool SECURITY;
  bool WIFI;
  bool APP_COM;
  bool CAN_COM;
};

union VALUE_U {
  uint32_t U32;
  uint16_t U16;
  struct little_endian {
      uint8_t B_0;
      uint8_t B_1;
      uint8_t B_2;
      uint8_t B_3;
  } BYTE;
};


extern struct FLAG FLAG;



#ifdef __cplusplus
extern "C" {
#endif

#define LED_1 18
#define LED_2 19
#define LED_3 27
#define LED_4 26
#define LED_5 25
#define LED_6 33
#define LED_7 32
#define SW_1 35

void app_partition_init();
void IO_OPERATION_TASK(void *pv);
void OTA_LED_TASK(void *args);
extern uint32_t cmd;



#ifdef __cplusplus
}
#endif

#endif