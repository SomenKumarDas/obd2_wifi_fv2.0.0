#include "caniso.h"
#include "app.hpp"
#include "util.h"

CAN_device_t CAN_cfg = {
    .speed = CAN_SPEED_500KBPS,
    .tx_pin_id = GPIO_NUM_5,
    .rx_pin_id = GPIO_NUM_4,
    .rx_queue = NULL,
    .tx_queue = NULL,
};

ReceiveIso CanIsoRcv;
CAN_frame_t rx_frame;
CAN_frame_t tx_frame;
CanIsoFrame frame;

extern uint8_t PROTOCOL;
extern uint32_t APP_TstrPrTmr;
extern uint32_t CAN_RqRspTmr;
extern uint32_t CAN_RqRspMaxTime;
extern bool APP_CAN_TransmitTstrMsg;

// CAN open Implementation
bool SDO_DataTR;
bool SDO_BLOCK_ECU_OK = false;

// New IVN Implementation
extern bool GetIvnFrame;

static void canInit(void);
static void HandleRX_SF();
static void HandleRX_CF();
static void HandleRX_FF();
static void HandleRX_FC();
static void Handle_SDO_MOSI();
static void SendSDO_block();
void Send_TTP(void);
static esp_err_t CAN_WriteFrame(const CAN_frame_t *pframe, TickType_t ticks_to_wait);

void CAN_TASK(void *args)
{
  canInit();
  frame.RxFrameBuf = new (uint8_t[4096]);
  frame.TxFrameBuf = new (uint8_t[4096]);

  SET_TxHeader(0x7e0, 0);
  CAN_SetBaud(CAN_SPEED_500KBPS);
  CAN_ConfigFilterterMask(0x7e8, 0);
  frame.PadEn = true;
  frame.PadByte = 0x55;

  // CAN_EnableInterframeDelay(6000);

  for (;;)
  {
    RCV_ISO();
    SEND_CAN_RESPONSE();
    Send_TTP();
  }

  ESP_LOGE("CAN", "CAN TASK STOPED");
  vTaskDelete(NULL);
}

void Send_TTP(void)
{
  if ((IsTimerElapsed(APP_TstrPrTmr)) && (APP_CAN_TransmitTstrMsg))
  {
    ResetTimer(APP_TstrPrTmr, CAN_RqRspMaxTime);
    if (frame.PadEn)
    {
      memset(&tx_frame.data.u8[3], frame.PadByte, 5);
      tx_frame.FIR.B.DLC = 8;
    }
    else
    {
      tx_frame.FIR.B.DLC = 3;
    }
    tx_frame.data.u8[0] = 0x02;
    tx_frame.data.u8[1] = 0x3E;
    tx_frame.data.u8[2] = 0x00;
    CAN_WriteFrame(&tx_frame, portMAX_DELAY);
  }
}

bool SEND_ISO(uint8_t *data, uint32_t len)
{
  StartTimer(CAN_RqRspTmr, CAN_RqRspMaxTime);
  if (PROTOCOL == APP_CAN_PROTOCOL_ISO15765)
  {
    if (len <= 7)
    {
      if (frame.PadEn)
      {
        memset(tx_frame.data.u8, frame.PadByte, 8);
        tx_frame.FIR.B.DLC = 8;
      }
      else
      {
        tx_frame.FIR.B.DLC = len + 1;
      }

      tx_frame.data.u8[0] = 0x0F & len;
      memcpy((uint8_t *)&tx_frame.data.u8[1], data, len);
      CAN_WriteFrame(&tx_frame, portMAX_DELAY);
      return true;
    }
    else if (len <= 4096)
    {
      tx_frame.FIR.B.DLC = 8;
      tx_frame.data.u8[0] = (0x0F & (uint8_t)(len >> 8)) | 0x10;
      tx_frame.data.u8[1] = (uint8_t)len;
      memcpy(&tx_frame.data.u8[2], data, 6);
      frame.TxFrameIdx = 0;
      frame.TxFrameLen = len - 6;
      memcpy(frame.TxFrameBuf, &data[6], len - 6);
      frame.SetTx_FF = true;
      frame.Tx_CF_Idx = 1;
      CAN_WriteFrame(&tx_frame, portMAX_DELAY);
      return true;
    }
  }
  else if (PROTOCOL == APP_CAN_PROTOCOL_OPEN)
  {
    if (len == 8)
    {
      tx_frame.FIR.B.DLC = 8;
      memcpy((uint8_t *)tx_frame.data.u8, data, len);
      CAN_WriteFrame(&tx_frame, portMAX_DELAY);
    }
    else if (len > 8)
    {
      memcpy(frame.TxFrameBuf, data, len);
      frame.TxFrameLen = len;
      frame.TxFrameIdx = 0;
      frame.Tx_CF_Idx = 1;
      if (SDO_BLOCK_ECU_OK)
        SendSDO_block();
    }
  }

  return false;
}

void RCV_ISO(void)
{
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, pdMS_TO_TICKS(5)) == pdTRUE)
  {
    if (PROTOCOL == APP_CAN_PROTOCOL_ISO15765)
    {
      switch (rx_frame.data.u8[0] >> 4)
      {
      case 0: HandleRX_SF(); break;

      case 1: HandleRX_FF(); break;

      case 2: HandleRX_CF(); break;

      case 3: HandleRX_FC(); break;
      }
    }
    else if (PROTOCOL == APP_CAN_PROTOCOL_OE_IVN)
    {
      if (GetIvnFrame)
      {
        CanIsoRcv.RxMsgLen = rx_frame.FIR.B.DLC;
        memcpy(CanIsoRcv.IVN_buf, rx_frame.data.u8, CanIsoRcv.RxMsgLen);
        CanIsoRcv.newMessage = true;
        GetIvnFrame = false;
      }
      else
      {
        CAN_ConfigFilterterMask(0x7FF, 0);
      }
    }
    else if (PROTOCOL == APP_CAN_PROTOCOL_OPEN)
    {
      Handle_SDO_MOSI();
    }
  
    FLAG.CAN_COM = true;
  }
}

static void HandleRX_SF()
{
  StopTimer(CAN_RqRspTmr);
  CanIsoRcv.RxID = rx_frame.MsgID;
  CanIsoRcv.exID = (bool)rx_frame.FIR.B.FF;
  CanIsoRcv.RxMsgLen = (rx_frame.data.u8[0] & 0x0F);
  memcpy(CanIsoRcv.SF_buf, &rx_frame.data.u8[1], CanIsoRcv.RxMsgLen);
  CanIsoRcv.newMessage = true;
}

static void HandleRX_FF()
{
  StopTimer(CAN_RqRspTmr);
  frame.RxFramelen = ((uint16_t)(0x0F & rx_frame.data.u8[0]) << 8) | (uint16_t)rx_frame.data.u8[1];
  memcpy(frame.RxFrameBuf, &rx_frame.data.u8[2], rx_frame.FIR.B.DLC - 2);
  frame.RxFrameIdx = rx_frame.FIR.B.DLC - 2;

  tx_frame.data.u8[0] = 0x30;
  tx_frame.data.u8[1] = 0;
  tx_frame.data.u8[2] = 0xF9;
  if (frame.PadEn)
  {
    tx_frame.FIR.B.DLC = 8;
    memset(&tx_frame.data.u8[3], frame.PadByte, 5);
  }
  else
  {
    tx_frame.FIR.B.DLC = 3;
  }

  if (CAN_WriteFrame(&tx_frame, portMAX_DELAY) == ESP_OK)
    ;
  frame.SetRx_FF = true;
  return;
}

static void HandleRX_CF()
{
  if (frame.RxFramelen - frame.RxFrameIdx > 7)
  {
    memcpy(&frame.RxFrameBuf[frame.RxFrameIdx], &rx_frame.data.u8[1], rx_frame.FIR.B.DLC - 1);
    frame.RxFrameIdx += rx_frame.FIR.B.DLC - 1;
  }
  else
  {
    uint8_t remlen = frame.RxFramelen - frame.RxFrameIdx;
    memcpy(&frame.RxFrameBuf[frame.RxFrameIdx], &rx_frame.data.u8[1], remlen);
    frame.RxFrameIdx = 0;
    frame.SetRx_FF = false;

    CanIsoRcv.RxID = rx_frame.MsgID;
    CanIsoRcv.exID = (bool)rx_frame.FIR.B.FF;
    CanIsoRcv.RxMsgLen = frame.RxFramelen;
    CanIsoRcv.CF_buf = frame.RxFrameBuf;
    CanIsoRcv.newMessage = true;
  }
}

static void HandleRX_FC()
{
  /* Get the timing and block params*/
  if ((rx_frame.data.u8[0] & 0x0F) == 1)
    return;
  else if ((rx_frame.data.u8[0] & 0x0F) == 2)
    frame.SetTx_FF = false;

  if (rx_frame.data.u8[2] <= 127 && rx_frame.data.u8[2] > 0)
  {
    frame.FC_STmin = rx_frame.data.u8[2] + 2;
  }
  else if (rx_frame.data.u8[2] >= 0xF3 && rx_frame.data.u8[2] <= 0xF9)
  {
    CAN_EnableInterframeDelay((0x0F & rx_frame.data.u8[2]) * 100);
    frame.FC_STmin = 0;
  }
  else
  {
    frame.FC_STmin = 0;
    CAN_EnableInterframeDelay(0);
  }

  frame.FC_BS = rx_frame.data.u8[1];
  frame.Tx_CF_Idx = 1;

  /* Send the frames*/
  while (frame.SetTx_FF)
  {
    // Serial.println(frame.TxFrameLen);
    if (frame.TxFrameLen >= 7)
    {
      tx_frame.FIR.B.DLC = 8;
    }
    else
    {
      tx_frame.FIR.B.DLC = frame.TxFrameLen + 1;
    }
    tx_frame.data.u8[0] = (0x0F & frame.Tx_CF_Idx) | 0x20;
    memcpy(&tx_frame.data.u8[1], &frame.TxFrameBuf[frame.TxFrameIdx], (tx_frame.FIR.B.DLC - 1));
    frame.TxFrameIdx += (tx_frame.FIR.B.DLC - 1);
    frame.TxFrameLen -= (tx_frame.FIR.B.DLC - 1);
    if ((tx_frame.FIR.B.DLC < 8) && frame.PadEn)
    {
      memset((&tx_frame.data.u8[0] + tx_frame.FIR.B.DLC),
             (uint8_t)frame.PadByte,
             (8 - tx_frame.FIR.B.DLC));
      tx_frame.FIR.B.DLC = 8;
    }
    CAN_WriteFrame(&tx_frame, portMAX_DELAY);

    if ((frame.FC_BS) && (frame.Tx_CF_Idx == frame.FC_BS))
    {
      // Serial.printf("%u : %u ", frame.Tx_CF_Idx, frame.FC_BS);
      // Serial.println("Break");
      break;
    }
      
    frame.Tx_CF_Idx++;
    if (frame.TxFrameLen == 0)
    {
      // Serial.println("Break 0");
      frame.SetTx_FF = false;
    }
  }
}

static void Handle_SDO_MOSI()
{
  if ((rx_frame.data.u8[0] >> 4) == 0x0A)
    SDO_BLOCK_ECU_OK = true;
  else
    SDO_BLOCK_ECU_OK = false;

  CanIsoRcv.RxMsgLen = rx_frame.FIR.B.DLC;
  memcpy(frame.RxFrameBuf, rx_frame.data.u8, CanIsoRcv.RxMsgLen);
  CanIsoRcv.CF_buf = frame.RxFrameBuf;
  CanIsoRcv.newMessage = true;
}

static void SendSDO_block()
{
  while (frame.TxFrameLen && SDO_BLOCK_ECU_OK)
  {
    if (frame.TxFrameLen > 7)
    {
      tx_frame.FIR.B.DLC = 8;
      tx_frame.data.u8[0] = frame.Tx_CF_Idx;
    }
    else
    {
      tx_frame.FIR.B.DLC = frame.TxFrameLen + 1;
      tx_frame.data.u8[0] = (frame.Tx_CF_Idx != 0x7F) ? (frame.Tx_CF_Idx | 0x80) : frame.Tx_CF_Idx;
    }

    memcpy(&tx_frame.data.u8[1], &frame.TxFrameBuf[frame.TxFrameIdx], (tx_frame.FIR.B.DLC - 1));
    frame.TxFrameIdx += (tx_frame.FIR.B.DLC - 1);
    frame.TxFrameLen -= (tx_frame.FIR.B.DLC - 1);
    CAN_WriteFrame(&tx_frame, portMAX_DELAY);

    if (frame.Tx_CF_Idx == 0x7F)
    {
      frame.Tx_CF_Idx = 1;
      break;
    }
    frame.Tx_CF_Idx++;
    if (frame.TxFrameLen == 0)
    {
      SDO_DataTR = false;
      frame.TxFrameIdx = 0;
      break;
    }
  }
}

void _TX_task(void *pvParameters)
{
  CAN_frame_t Txframe;

  for (;;)
  {
    if (xQueueReceive(CAN_cfg.tx_queue, (void *)&Txframe, portMAX_DELAY) == pdPASS)
    {
      if (CAN_Drv_WriteFrame(&Txframe) == ESP_OK)
      {
        if (frame.FC_STmin)
        {
          vTaskDelay(pdMS_TO_TICKS(frame.FC_STmin));
        }
      }
      FLAG.CAN_COM = true;
    }
  }
}

static void canInit(void)
{
  CAN_filter_t filter;
  if (CAN_cfg.err_queue == NULL)
  {
    CAN_cfg.err_queue = xQueueCreate(3, sizeof(CAN_error_t));
  }

  if (CAN_cfg.rx_queue == NULL)
  {
    CAN_cfg.rx_queue = xQueueCreate(100, sizeof(CAN_frame_t));
  }

  if (CAN_cfg.tx_queue == NULL)
  {
    CAN_cfg.tx_queue = xQueueCreate(100, sizeof(CAN_frame_t));
  }

  filter = {
      .FM = Single_Mode,
      .ACR0 = 0xFF,
      .ACR1 = 0xFF,
      .ACR2 = 0xFF,
      .ACR3 = 0xFF,
      .AMR0 = 0,
      .AMR1 = 0,
      .AMR2 = 0,
      .AMR3 = 0,
  };
  CAN_Drv_ConfigFilter(&filter);

  if ((CAN_cfg.rx_queue == NULL) || (CAN_cfg.tx_queue == NULL) || (CAN_cfg.err_queue == NULL))
  {
    ESP_LOGE("CAN", "CAN Failed to create queue message for either Rx / Tx / err");
    vTaskDelete(NULL);
  }
  // CAN_Drv_Init(&CAN_cfg);

  if (xTaskCreate(_TX_task, "_TX_task", 1024 * 5, NULL, tskIDLE_PRIORITY + 7, NULL) != pdTRUE)
    vTaskDelete(NULL);
}

void SET_TxHeader(uint32_t msgID, bool extd)
{
  tx_frame.MsgID = msgID;
  tx_frame.FIR.B.FF = (CAN_frame_format_t)extd;
  tx_frame.FIR.B.DLC = 8;
}

void CAN_DeInit(void)
{
  CAN_Drv_Stop();
}

void CAN_SetBaud(CAN_speed_t speed)
{
  CAN_Drv_Stop();
  CAN_cfg.speed = speed;
  CAN_Drv_Init(&CAN_cfg);
}

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))
  uint32_t acceptance_mask;
  CAN_filter_t filter;

  CAN_Drv_Stop();

  if (acceptance_code == 0xFFFFFFFF)
  {
    // No filter
    acceptance_mask = 0xFFFFFFFF;
    acceptance_code = 0;
  }
  else
  {
    if (extId == true)
    {
      acceptance_code = (acceptance_code << (32 - 29));
      acceptance_mask = ~(CAN_EXTD_ID_MASK << (32 - 29));
    }
    else
    {
      acceptance_code = (acceptance_code << (32 - 11));
      acceptance_mask = ~(CAN_STD_ID_MASK << (32 - 11));
    }
  }

  filter = {
      .FM = Single_Mode,
      .ACR0 = byte(acceptance_code, 3),
      .ACR1 = byte(acceptance_code, 2),
      .ACR2 = byte(acceptance_code, 1),
      .ACR3 = byte(acceptance_code, 0),
      .AMR0 = byte(acceptance_mask, 3),
      .AMR1 = byte(acceptance_mask, 2),
      .AMR2 = byte(acceptance_mask, 1),
      .AMR3 = byte(acceptance_mask, 0),
  };

  CAN_Drv_ConfigFilter(&filter);
  CAN_Drv_Init(&CAN_cfg);
}

void CAN_EnableInterframeDelay(uint32_t delay)
{
  CAN_Drv_EnableInterframeDelay(delay);
}

esp_err_t CAN_WriteFrame(const CAN_frame_t *pframe, TickType_t ticks_to_wait)
{
  esp_err_t status = ESP_OK;

  if (CAN_cfg.tx_queue == NULL)
  {
    status = ESP_FAIL;
  }
  else
  {
    if (xQueueSendToBack(CAN_cfg.tx_queue, (void *)pframe, ticks_to_wait) != pdPASS)
    {
      Serial.println("ERROR: CAN Failed to queue Tx message");
      status = ESP_FAIL;
    }
  }
  return status;
}
