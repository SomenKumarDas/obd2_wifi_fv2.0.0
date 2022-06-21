#pragma once

#include "app.hpp"
#include <UIPEthernet.h>



void eth_Task(void *pv);
void eth_Write(uint8_t *payLoad, uint16_t len);