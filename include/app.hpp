#ifndef __APP_HPP__
#define __APP_HPP__

#include <Arduino.h>
#include "caniso.h"
#include "uart.h"
#include "util.h"
#include "tcpsoc.h"
// #include "ble.h"
#include "rtl_interface.hpp"

#define IsTimerElapsed(x) ((x > 0) && (x <= (millis())))
#define IsTimerRunning(x) ((x > 0) && (x > (millis())))
#define IsTimerEnabled(x) ((x) > 0)
#define StartTimer(x, y)    \
    {                       \
        x = (millis()) + y; \
    }
#define ResetTimer(x, y)    \
    {                       \
        x = (millis()) + y; \
    }
#define StopTimer(x) \
    {                \
        x = 0;       \
    }

typedef enum
{
    APP_MSG_CHANNEL_NONE = -1,
    APP_MSG_CHANNEL_UART = 0,
    APP_MSG_CHANNEL_TCP_SOC,
    APP_MSG_CHANNEL_RTL,
    APP_MSG_CHANNEL_MAX,
} APP_CHANNEL_t;

typedef enum
{
    APP_CAN_PROTOCOL_NONE = 0,
    APP_CAN_PROTOCOL_ISO15765,
    APP_CAN_PROTOCOL_NORMAL,
    APP_CAN_PROTOCOL_OE_IVN,
    APP_CAN_PROTOCOL_OPEN
} CAN_PROTOCOL_t;

typedef enum
{
    APP_REQ_CMD_RESET                   = 0x01,
    APP_REQ_CMD_SET_PROTOCOL            = 0x02,
    APP_REQ_CMD_GET_PROTOCOL            = 0x03,
    APP_REQ_CMD_SET_TX_CAN_ID           = 0x04,
    // APP_REQ_CMD_GET_TX_CAN_ID           = 0x05,
    APP_REQ_CMD_SET_RX_CAN_ID           = 0x06,
    APP_REQ_CMD_GET_RX_CAN_ID           = 0x07,
    // APP_REQ_CMD_SFCBLKL                 = 0x08,
    // APP_REQ_CMD_GFCBLKL                 = 0x09,
    // APP_REQ_CMD_SFCST                   = 0x0A,
    // APP_REQ_CMD_GFCST                   = 0x0B,
    APP_REQ_CMD_SETP1MIN                = 0x0C,
    APP_REQ_CMD_GETP1MIN                = 0x0D,
    APP_REQ_CMD_SETP2MAX                = 0x0E,
    // APP_REQ_CMD_GETP2MAX                = 0x0F,
    APP_REQ_CMD_TXTP                    = 0x10,
    APP_REQ_CMD_STPTXTP                 = 0x11,
    APP_REQ_CMD_TXPAD                   = 0x12,
    APP_REQ_CMD_STPTXPAD                = 0x13,
    APP_REQ_CMD_GET_FIRMWARE_VER        = 0x14,
    // APP_REQ_CMD_GETSFR                  = 0x15,
    APP_REQ_CMD_SET_STA_SSID            = 0x16,
    APP_REQ_CMD_SET_STA_PASSWORD        = 0x17,
    APP_REQ_CMD_ENABLE_CAN_TX_DELAY     = 0x18,
    APP_REQ_CMD_START_OTA               = 0x19,
    APP_REQ_CMD_GET_FRAME               = 0x20,
    APP_REQ_CMD_GET_MAC                 = 0x21,
    APP_REQ_CMD_GET_STA_SSID            = 0x22,
    APP_REQ_CMD_GET_STA_PASSWORD        = 0x23,
} APP_REQ_CMD_t;

typedef enum
{
    APP_RESP_ACK = 0x00,     // Positive Response
    APP_RESP_NACK = 0x01,    // Negative Response
    APP_RESP_NACK_10 = 0x10, // Command Not Supported
    APP_RESP_NACK_12 = 0x12, // Input Not supported
    APP_RESP_NACK_13 = 0x13, // Invalid format or incorrect message length of input
    APP_RESP_NACK_14 = 0x14, // Invalid operation
    APP_RESP_NACK_15 = 0x15, // CRC failure
    APP_RESP_NACK_16 = 0x16, // Protocol not set
    APP_RESP_NACK_22 = 0x22, // Conditions not correct
    APP_RESP_NACK_31 = 0x31, // Request out of range
    APP_RESP_NACK_33 = 0x33, // security access denied
    APP_RESP_NACK_78 = 0x78, // response pending
    APP_RESP_NACK_24 = 0x24, // request sequence error
    APP_RESP_NACK_35 = 0x35, // Invalid Key
    APP_RESP_NACK_36 = 0x36, // exceeded number of attempts
    APP_RESP_NACK_37 = 0x37, // required time delay not expired
    APP_RESP_NACK_72 = 0x72, // General programming failure
    APP_RESP_NACK_7E = 0x7E, // sub fn not supported in this diag session
} APP_RESP_t;

typedef enum
{
    APP_CAN_ISO_TYPE_SINGLE = 0,
    APP_CAN_ISO_TYPE_FIRST,
    APP_CAN_ISO_TYPE_CONSECUTIVE,
    APP_CAN_ISO_TYPE_FLOWCONTROL,
} APP_CAN_ISO_TYPE_t;

typedef enum
{
    APP_BUFF_LOCKED_BY_NONE = 0,
    APP_BUFF_LOCKED_BY_FRAME0,
    APP_BUFF_LOCKED_BY_FRAME1,
    APP_BUFF_LOCKED_BY_CAN_TX,
    APP_BUFF_LOCKED_BY_ISO_TP_TX_SF,
    APP_BUFF_LOCKED_BY_ISO_TP_TX_FF,
    APP_BUFF_LOCKED_BY_ISO_TP_TX_CF,
} APP_BUFF_LOCKED_BY_t;

extern uint8_t PROTOCOL;

#ifdef __cplusplus
extern "C"
{
#endif
    
    extern uint8_t COM_CHANNEL;
    
    void APP_TASK(void *pv);
    void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel);
    uint16_t UTIL_CRC16_CCITT(uint16_t initVal, uint8_t *p_inBuff, uint16_t len);
    void APP_SendRespToFrame(uint8_t respType, uint8_t APP_RESP_nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel);

    void SendEcuResponseTimeoutErr(void *pv);
    void SEND_CAN_RESPONSE(void);

#ifdef __cplusplus
}
#endif

#endif