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
#define LED_4 0
#define LED_5 0
#define LED_6 33
#define LED_7 32
#define SW_1 35
#define DAC1 25
#define DAC2 26
#define ADC1 34

#define IF(x, y) \
  if (x)         \
  {              \
    y            \
  }
#define _ST_(std_exp) std_exp;
#define DebugCom Serial

inline void DEBUG(const char* tag, uint8_t *buff, int idx)
{
  DebugCom.printf("[%s] [%04u] [", tag, idx);
  for (int i = 0; i < ((idx > 10) ? 10 : idx); i++)
  {
    DebugCom.printf("%02x", buff[i]);
  }
  IF(idx > 10, DebugCom.printf("...");)
  DebugCom.println("]");
}

void app_partition_init();
void IO_OPERATION_TASK(void *pv);
void OTA_LED_TASK(void *args);
extern uint32_t cmd;



#ifdef __cplusplus
}
#endif

#endif