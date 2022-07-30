#ifndef __UART_H__
#define __UART_H__

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_COM Serial
#define MAX_DATA_LEN 4100

void UART_Task(void *pv);
void UART_Write(uint8_t *payLoad, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif