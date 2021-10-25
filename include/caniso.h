#pragma once

#include <Arduino.h>
#include "driver/can.h"
#include <CAN_config.h>
#include "CAN.h"
#include <vector>


struct ReceiveIso{
  uint32_t    RxID = 0;
  bool        exID = 0;
  uint16_t    RxMsgLen = 0;
  uint8_t*    CF_buf;
  uint8_t     SF_buf[7] = {0};
  uint8_t     IVN_buf[8] = {0};
  bool        newMessage;
};


struct CanIsoFrame
{
  uint32_t    TxID;
  uint8_t     TxExid;
  bool        TxIdExD;
  uint16_t    TxFrameLen;
  uint16_t    TxFrameIdx;
  uint8_t*    TxFrameBuf;
  uint8_t     Tx_CF_Idx;
  bool        SetTx_FF;

  uint32_t    RxID;
  uint8_t     RxExid;
  bool        RxIdExD;
  uint16_t    RxFramelen;
  uint16_t    RxFrameIdx;
  uint8_t*    RxFrameBuf;
  uint8_t     Rx_CF_Idx;
  bool        SetRx_FF;

  uint8_t     FC_BS;
  uint8_t     FC_STmin;
  uint16_t    FC_STMI_TX;
  uint8_t     PadByte;
  bool        PadEn;

};

extern ReceiveIso      CanIsoRcv;
extern CanIsoFrame     frame;

void CAN_TASK(void* args);
void _TX_task(void *pvParameters);

bool SEND_ISO(uint8_t *data, uint32_t len);
void RCV_ISO(void);
void SET_TxHeader(uint32_t msgID, bool extd);
void CAN_DeInit(void);
void CAN_SetBaud(CAN_speed_t speed);
void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId);
void CAN_EnableInterframeDelay(uint32_t delay);
