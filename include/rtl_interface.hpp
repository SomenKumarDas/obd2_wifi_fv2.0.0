#pragma once
#include <Arduino.h>

#define RTL_DATA_CH Serial2

void rtl_interface_Task(void *pv);
void rtl_interface_write(uint8_t *payLoad, uint16_t len);
