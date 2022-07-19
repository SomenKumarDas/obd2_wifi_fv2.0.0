#include "app.hpp"
#include "app_ota.hpp"

uint8_t APP_SecuityCode[] = {0x47, 0x56, 0x8A, 0xFE, 0x56, 0x21, 0x4E, 0x23, 0x80, 0x00};

const uint8_t var[3] = { 4, 0, 0};
VALUE_U value;
uint8_t COM_CHANNEL;
uint8_t PROTOCOL = 1;
uint8_t ProtocolNumber = 0;
uint8_t APP_BuffLockedBy = 0;
uint16_t APP_CAN_TxDataLen;
uint16_t APP_BuffRxIndex;
uint8_t APP_CAN_TxBuff[4096];

bool APP_CAN_TransmitTstrMsg = false;
uint32_t APP_TstrPrTmr;
uint32_t CAN_RqRspTmr;
uint32_t CAN_RqRspMaxTime = 500;
uint32_t CAN_RxId = 0;
uint8_t P1MIN = 0;

String OTA_URL;

bool GetIvnFrame = false;

static void APP_Frame0(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame1(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_COMMAND(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame3(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void ISO_CAN_FRAME(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void SecurityCheck(uint8_t *p_buff, uint16_t len, uint8_t channel);

void (*cb_APP_FrameType[])(uint8_t *, uint16_t, uint8_t) =
    {
        APP_Frame0,
        APP_Frame1,
        APP_COMMAND,
        APP_Frame3,
        ISO_CAN_FRAME,
        SecurityCheck};

void (*cb_APP_Send[])(uint8_t *, uint16_t) =
    {
        UART_Write,
        WIFI_TCP_Soc_Write,
        rtl_interface_write,
        eth_Write
};

void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel)
{
  uint8_t frameType;
  uint16_t frameLen;
  uint8_t respType;
  uint8_t respNo;
  uint16_t crc16Act;
  uint16_t crc16Calc;
  uint16_t respLen;
  uint8_t respBuff[50];

  COM_CHANNEL = (uint8_t)channel;
  FLAG.APP_COM = true;
  respLen = 0;
  respType = APP_RESP_ACK;
  respNo = APP_RESP_ACK;

  frameType = p_buff[0] >> 4;
  frameLen = ((uint16_t)(p_buff[0] & 0x0F) << 8) | (uint16_t)p_buff[1];

  if (frameLen == (len - 2))
  {
    crc16Act = ((uint16_t)p_buff[len - 2] << 8) | (uint16_t)p_buff[len - 1];
    crc16Calc = UTIL_CRC16_CCITT(0xFFFF, &p_buff[2], (frameLen - 2));

    if (crc16Act == crc16Calc)
    {
      if (FLAG.SECURITY)
      {
        cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), (uint8_t)channel);
      }
      else
      {
        if (frameType == 5)
        {
          cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), (uint8_t)channel);
        }
        else
        {
          respType = APP_RESP_NACK;
          respNo = APP_RESP_NACK_33;
        }
      }
    }
    else
    {
      respType = APP_RESP_NACK;
      respNo = APP_RESP_NACK_15;

      respBuff[respLen++] = crc16Act >> 8;
      respBuff[respLen++] = crc16Act;
      respBuff[respLen++] = crc16Calc >> 8;
      respBuff[respLen++] = crc16Calc;
    }
  }
  else
  {
    respType = APP_RESP_NACK;
    respNo = APP_RESP_NACK_13;
  }

  if (respType != APP_RESP_ACK)
  {
    APP_SendRespToFrame(respType, respNo, respBuff, respLen, (uint8_t)channel);
  }
}

void SEND_CAN_RESPONSE(void)
{
  /* Check for the ecu response timeout*/
  if (IsTimerElapsed(CAN_RqRspTmr))
  {
    StopTimer(CAN_RqRspTmr);
    uint8_t respBuffer[10];
    uint16_t respLen = 0;
    respBuffer[respLen++] = 0x40;
    respBuffer[respLen++] = 0x02;
    respBuffer[respLen++] = 0xff;
    respBuffer[respLen++] = 0xff;
    cb_APP_Send[COM_CHANNEL](respBuffer, respLen);
  }

  /* check for any new iso can message*/
  if (CanIsoRcv.newMessage)
  {
    if (PROTOCOL == APP_CAN_PROTOCOL_ISO15765 || PROTOCOL == APP_CAN_PROTOCOL_OPEN)
    {
      FLAG.APP_COM = true;
      CanIsoRcv.newMessage = false;
      if ((CanIsoRcv.RxMsgLen == 2) && (CanIsoRcv.SF_buf[0] == 0x7E) && APP_CAN_TransmitTstrMsg)
        return;

      if (APP_CAN_TransmitTstrMsg)
      {
        if ((CanIsoRcv.RxMsgLen == 3) && CanIsoRcv.SF_buf[2] == 0x78)
        {
          StopTimer(APP_TstrPrTmr);
        }
        else
        {
          ResetTimer(APP_TstrPrTmr, 2000);
        }
      }
      uint16_t respLen = 0;
      uint16_t crc16;
      respLen = 0;
      uint16_t len = CanIsoRcv.RxMsgLen;
      uint8_t *rxMsgbuff = (CanIsoRcv.RxMsgLen > 7) ? CanIsoRcv.CF_buf : CanIsoRcv.SF_buf;
      uint8_t respBuffer[len + 10];

      crc16 = UTIL_CRC16_CCITT(0xFFFF, rxMsgbuff, len);

      for (uint16_t i = len; i > 0; i--)
      {
        respBuffer[i + 1] = rxMsgbuff[i - 1];
      }
      respBuffer[respLen++] = 0x40 | (((len + 2) >> 8) & 0x0F);
      respBuffer[respLen++] = (len + 2);
      respLen = respLen + len;
      respBuffer[respLen++] = crc16 >> 8;
      respBuffer[respLen++] = crc16;

      cb_APP_Send[COM_CHANNEL](respBuffer, respLen);
    }
    else if (PROTOCOL == APP_CAN_PROTOCOL_OE_IVN)
    {
      CanIsoRcv.newMessage = false;
      uint16_t respLen = 0;
      uint16_t crc16;
      respLen = 0;
      uint8_t IVNbuff[20];

      memcpy(IVNbuff, CanIsoRcv.IVN_buf, CanIsoRcv.RxMsgLen);
      crc16 = UTIL_CRC16_CCITT(0xFFFF, IVNbuff, 8);
      for (uint16_t i = 8; i > 0; i--)
      {
        IVNbuff[i + 1] = IVNbuff[i - 1];
      }
      IVNbuff[respLen++] = 0x40 | (((8 + 2) >> 8) & 0x0F);
      IVNbuff[respLen++] = (8 + 2);
      respLen = respLen + 8;
      IVNbuff[respLen++] = crc16 >> 8;
      IVNbuff[respLen++] = crc16;

      cb_APP_Send[COM_CHANNEL](IVNbuff, respLen);
    }
  }
}

static void APP_Frame0(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  uint8_t respType;
  uint8_t respNo;
  uint16_t respLen;
  uint8_t respBuff[50];

  respType = APP_RESP_ACK;
  respNo = APP_RESP_ACK;
  respLen = 0;

  if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_NONE)
  {
    APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME0;
    APP_CAN_TxDataLen = ((uint16_t)p_buff[0] << 8) | (uint16_t)p_buff[1];
    APP_BuffRxIndex = 0;

    if (APP_CAN_TxDataLen > 4095)
    {
      respType = APP_RESP_NACK;
      respNo = APP_RESP_NACK_13;
      APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
    }
    else
    {
      memcpy(&APP_CAN_TxBuff[APP_BuffRxIndex], &p_buff[2], (len - 2));
      APP_BuffRxIndex += (len - 2);
    }
  }
  else
  {
    respType = APP_RESP_NACK;
    respNo = APP_RESP_NACK_14;
  }

  respBuff[respLen++] = APP_BuffRxIndex >> 8;
  respBuff[respLen++] = APP_BuffRxIndex;

  APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel);
}

static void APP_Frame1(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  uint8_t respType;
  uint8_t respNo;
  uint16_t respLen;
  uint8_t respBuff[50];

  respType = APP_RESP_ACK;
  respNo = APP_RESP_ACK;
  respLen = 0;

  if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_FRAME0)
  {
    if (APP_CAN_TxDataLen && ((APP_BuffRxIndex + len) <= 4095))
    {
      memcpy(&APP_CAN_TxBuff[APP_BuffRxIndex], p_buff, len);
      APP_BuffRxIndex += len;

      if (APP_BuffRxIndex >= APP_CAN_TxDataLen)
      {
        APP_BuffRxIndex = 0;
        APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;

        SEND_ISO(APP_CAN_TxBuff, APP_CAN_TxDataLen);
      }
    }
    else
    {
      respType = APP_RESP_NACK;
      respNo = APP_RESP_NACK_13;
      APP_BuffRxIndex = 0;
      APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
    }
  }
  else
  {
    respType = APP_RESP_NACK;
    respNo = APP_RESP_NACK_14;
    APP_BuffRxIndex = 0;
    APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
  }

  respBuff[respLen++] = APP_BuffRxIndex >> 8;
  respBuff[respLen++] = APP_BuffRxIndex;

  APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel);
}

static void APP_COMMAND(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  uint8_t offset;
  uint8_t respType;
  uint8_t respNo;
  uint16_t respLen;
  uint8_t respBuff[50];

  respType = APP_RESP_ACK;
  respNo = APP_RESP_ACK;
  respLen = 0;
  offset = 0;

  if (len > 0)
  {
    switch (p_buff[0])
    {
    case APP_REQ_CMD_RESET:
    {
      ESP.restart();
    }
    break;
    case APP_REQ_CMD_SET_PROTOCOL:
    {
      if (len != 2)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      ProtocolNumber = p_buff[1];
      if (p_buff[1] < 6)
      {
        offset = 0;
        PROTOCOL = APP_CAN_PROTOCOL_ISO15765;
      }
      else if (p_buff[1] < 0x0C)
      {
        offset = 6;
        PROTOCOL = APP_CAN_PROTOCOL_NORMAL;
      }
      else if (p_buff[1] < 0x12)
      {
        offset = 0x0C;
        PROTOCOL = APP_CAN_PROTOCOL_OE_IVN;
      }
      else if (p_buff[1] < 0x18)
      {
        offset = 0x12;
        PROTOCOL = APP_CAN_PROTOCOL_OPEN;
      }
      else
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_12;
      }

      if ((p_buff[1] - offset) < 2)
      {
        CAN_SetBaud(CAN_SPEED_250KBPS);
      }
      else if ((p_buff[1] - offset) < 4)
      {
        CAN_SetBaud(CAN_SPEED_500KBPS);
      }
      else if ((p_buff[1] - offset) < 6)
      {
        CAN_SetBaud(CAN_SPEED_1000KBPS);
      }
      else
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_12;
      }
    }
    break;
    case APP_REQ_CMD_GET_PROTOCOL:
    {
      respBuff[respLen++] = ProtocolNumber;
    }
    break;
    case APP_REQ_CMD_SET_TX_CAN_ID:
    {
      if (len == 3)
      {
        uint32_t txID = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
        SET_TxHeader(txID, false);
      }
      else if (len == 5)
      {
        uint32_t txID = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
        SET_TxHeader(txID, true);
      }
      else
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
      }
    }
    break;
    case APP_REQ_CMD_SET_RX_CAN_ID:
    {
      if (len == 3)
      {
        CAN_RxId = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
        CAN_ConfigFilterterMask(CAN_RxId, (bool)false);
      }
      else if (len == 5)
      {
        CAN_RxId = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
        CAN_ConfigFilterterMask(CAN_RxId, (bool)true);
      }
      else
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
    }
    break;
    case APP_REQ_CMD_GET_RX_CAN_ID:
    {
      value.U32 = CAN_RxId;
      respBuff[respLen++] = value.BYTE.B_3;
      respBuff[respLen++] = value.BYTE.B_2;
      respBuff[respLen++] = value.BYTE.B_1;
      respBuff[respLen++] = value.BYTE.B_0;
    }
    break;
    case APP_REQ_CMD_SETP1MIN:
    {
      if (len < 2)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      P1MIN = p_buff[1];
      if (p_buff[1] == 0)
      {
        CAN_EnableInterframeDelay(0);
      }
      else if (p_buff[1] == 1)
      {
        CAN_EnableInterframeDelay(800);
      }
      else
      {
        CAN_EnableInterframeDelay(((uint32_t)p_buff[1] * 100));
      }
    }
    break;
    case APP_REQ_CMD_GETP1MIN:
    {
      respBuff[respLen++] = P1MIN;
    }
    case APP_REQ_CMD_SETP2MAX:
    {
      if (len != 3)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      CAN_RqRspMaxTime = ((uint16_t)p_buff[1] << 8) | (uint16_t)p_buff[2];
    }
    break;
    case APP_REQ_CMD_TXTP:
    {
      if (len != 1)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      APP_CAN_TransmitTstrMsg = true;
      StartTimer(APP_TstrPrTmr, CAN_RqRspMaxTime);
    }
    break;
    case APP_REQ_CMD_STPTXTP:
    {
      if (len != 1)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }

      APP_CAN_TransmitTstrMsg = false;
      StopTimer(APP_TstrPrTmr);
    }
    break;
    case APP_REQ_CMD_TXPAD:
    {
      if (len != 2)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      frame.PadByte = p_buff[1];
      frame.PadEn = true;
    }
    break;
    case APP_REQ_CMD_STPTXPAD:
    {
      if (len != 1)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      frame.PadEn = false;
    }
    break;
    case APP_REQ_CMD_GET_FIRMWARE_VER:
    {
      respBuff[0] = var[0];
      respBuff[1] = var[1];
      respBuff[2] = var[2];
      respLen = 3;
    }
    break;
    case APP_REQ_CMD_SET_STA_SSID:
    {
      if (len < 5)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
      p_buff[len - 1] = 0;

      if (WIFI_Set_STA_SSID(p_buff[1], (char *)&p_buff[2]) == false)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
      }
    }
    break;
    case APP_REQ_CMD_SET_STA_PASSWORD:
    {
      if (len < 5)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }

      p_buff[len - 1] = 0;
      if (WIFI_Set_STA_Pass(p_buff[1], (char *)&p_buff[2]) == false)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
      }
    }
    break;
    case APP_REQ_CMD_ENABLE_CAN_TX_DELAY:
    {
      if (len < 2)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }

      if (p_buff[1] == 0)
      {
        CAN_EnableInterframeDelay(0);
      }
      else if (p_buff[1] == 1)
      {
        CAN_EnableInterframeDelay(800);
      }
      else
      {
        CAN_EnableInterframeDelay(((uint32_t)p_buff[1] * 100));
      }
    }
    break;
    case APP_REQ_CMD_START_OTA:
    {
      OTA_URL = "";
      for (int i = 1; i < len; i++)
        OTA_URL += (char)p_buff[i];

      // Serial.println(OTA_URL);

      if (xTaskCreate(ota_task, "FOTA_TASK", 1024 * 10, NULL, tskIDLE_PRIORITY + 8, NULL) != pdTRUE)
      {
        Serial.println("Failed to Create FOTA_TASK");
      }

      if (xTaskCreate(OTA_LED_TASK, "OTA_LED_TASK", 1024 * 1, NULL, tskIDLE_PRIORITY + 2, NULL) != pdTRUE)
      {
        Serial.println("Failed to Create OTA_LED_TASK");
      }
    }
    break;
    case APP_REQ_CMD_GET_FRAME:
    {
      if (PROTOCOL != APP_CAN_PROTOCOL_OE_IVN)
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_7E;
        break;
      }
      if (len == 3)
      {
        uint32_t rxID = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
        CAN_ConfigFilterterMask(rxID, (bool)false);
        GetIvnFrame = true;
        StartTimer(CAN_RqRspTmr, CAN_RqRspMaxTime);
      }
      else if (len == 5)
      {
        uint32_t rxID = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
        CAN_ConfigFilterterMask(rxID, (bool)true);
        GetIvnFrame = true;
        StartTimer(CAN_RqRspTmr, CAN_RqRspMaxTime);
      }
      else
      {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
        break;
      }
    }
    break;
    case APP_REQ_CMD_GET_MAC:
    {
      char mac[6];
      respLen = 0;
      WiFi.macAddress((uint8_t *)mac);
      memcpy(respBuff, mac, 6);
      respLen = 6;
    }
    break;
    case APP_REQ_CMD_GET_STA_SSID:
    {
      char key[20];
      respLen = 0;

      Preferences preferences;
      snprintf(key, sizeof(key), "stSSID[%d]", p_buff[1]);
      preferences.begin("config", false);
      String ret = preferences.getString(key, "NULL");
      preferences.end();

      respBuff[respLen++] = p_buff[1];
      memcpy(&respBuff[respLen], ret.c_str(), ret.length());
      respLen += ret.length();
    }
    break;
    case APP_REQ_CMD_GET_STA_PASSWORD:
    {
      char key[20];
      respLen = 0;
      
      Preferences preferences;
      snprintf(key, sizeof(key), "stPASS[%d]", p_buff[1]);
      preferences.begin("config", false);
      String ret = preferences.getString(key, "NULL");
      preferences.end();

      respBuff[respLen++] = p_buff[1];
      memcpy(&respBuff[respLen], ret.c_str(), ret.length());
      respLen += ret.length();
    }
    break;
    default:
      respType = APP_RESP_NACK;
      respNo = APP_RESP_NACK_10;
      break;
    }
  }
  else
  {
    respType = APP_RESP_NACK;
    respNo = APP_RESP_NACK_13;
  }

  APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel);
}

static void APP_Frame3(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  APP_SendRespToFrame(0, 0, NULL, 0, channel);
}

static void ISO_CAN_FRAME(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  if (len <= 4096)
  {
    if (APP_CAN_TransmitTstrMsg)
    {
      if (!IsTimerEnabled(APP_TstrPrTmr))
      {
        StartTimer(APP_TstrPrTmr, 2000);
      }
      else
      {
        ResetTimer(APP_TstrPrTmr, 2000);
      }
    }
    SEND_ISO(p_buff, len);
    APP_SendRespToFrame(APP_RESP_ACK, APP_RESP_ACK, NULL, 0, channel);
  }
}

static void SecurityCheck(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
  uint8_t respType;
  uint8_t respNo;

  respType = APP_RESP_ACK;
  respNo = APP_RESP_ACK;

  if (len != (uint16_t)sizeof(APP_SecuityCode))
  {
    FLAG.SECURITY = false;
  }
  else
  {
    if (memcmp(p_buff, APP_SecuityCode, sizeof(APP_SecuityCode)) == 0)
    {
      FLAG.SECURITY = true;
    }
    else
    {
      FLAG.SECURITY = false;
    }
  }

  if (FLAG.SECURITY == false)
  {
    respType = APP_RESP_NACK;
    respNo = APP_RESP_NACK_35;
  }
  APP_SendRespToFrame(respType, respNo, NULL, 0, channel);
}

void APP_SendRespToFrame(uint8_t respType, uint8_t APP_RESP_nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel)
{
  uint8_t len;
  uint8_t buff[30];
  uint16_t crc16;

  len = 0;
  buff[len++] = 0x20;

  if (respType > 0)
  {
    buff[len++] = 2 + 2;
    buff[len++] = respType;
    buff[len++] = APP_RESP_nackNo;
  }
  else
  {
    buff[len++] = 1 + 2;
    buff[len++] = respType;
  }

  if (dataLen && (p_buff != NULL))
  {
    memcpy(&buff[len], p_buff, dataLen);
    buff[1] += dataLen;
    len += dataLen;
  }

  // Exclude header 2Bytes and CRC 2 Bytes
  crc16 = UTIL_CRC16_CCITT(0xFFFF, &buff[2], (len - 2));
  buff[len++] = crc16 >> 8;
  buff[len++] = crc16;

  cb_APP_Send[channel](buff, len);
}

const uint16_t Crc16_CCITT_Table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

uint16_t UTIL_CRC16_CCITT(uint16_t initVal, uint8_t *p_inBuff, uint16_t len)
{
  uint32_t cnt;
  uint16_t crc = initVal;

  for (cnt = 0; cnt < len; cnt++)
  {
    crc = (crc << 8) ^ Crc16_CCITT_Table[((crc >> 8) ^ p_inBuff[cnt]) & 0x00FF];
  }

  return crc;
}