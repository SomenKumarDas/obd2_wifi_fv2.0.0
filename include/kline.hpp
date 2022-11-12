#pragma once
#include "app.hpp"



struct KLine_api_msg_t
{
    bool NewMsg;
    uint8_t* data;
    uint16_t len;
};


extern KLine_api_msg_t RespMsg;


void KLIN_Task(void *args);
void KLIN_FastInit();
void KLIN_Send(uint8_t *data, uint16_t len);
void KLIN_SetTgt(uint8_t data);
void KLIN_SetSrc(uint8_t data);
