#ifndef __UART_H__
#define __UART_H__

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void UART_Task(void *pv);
void UART_Write(uint8_t *payLoad, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif