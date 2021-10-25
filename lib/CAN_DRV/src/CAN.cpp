/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017, Thomas Barth, barth-dev.de
 *               2017, Jaime Breva, jbreva@nayarsystems.com
 *               2018, Michael Wagner, mw@iot-make.de
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "CAN.h"
#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_intr.h"
#include "soc/dport_reg.h"
#include <math.h>

#include "driver/gpio.h"

#include "can_regdef.h"
#include "CAN_config.h"

// CAN Filter - no acceptance filter
static CAN_filter_t CAN_Filter = {Dual_Mode, 0, 0, 0, 0, 0Xff, 0Xff, 0Xff, 0Xff};
static CAN_device_t CAN_cfg;
static bool CAN_IsrAlloc = false;
static uint32_t CAN_FrameDelay = 0;
static SemaphoreHandle_t CAN_SemTxComplete = NULL;

// static void CAN_ReadFramePhy();
static void CAN_Drv_ISR(void *arg_p);
static int CAN_WriteFramePhy(const CAN_frame_t *p_frame);
static void CAN_Drv_ReadFramePhy(BaseType_t *higherPriorityTaskWoken);

static void CAN_Drv_ISR(void *arg_p)
{

    // Interrupt flag buffer
    __CAN_IRQ_t interrupt;

    BaseType_t higherPriorityTaskWoken = pdFALSE;

    // Read interrupt status and clear flags
    interrupt = (__CAN_IRQ_t)MODULE_CAN->IR.U;

    // Handle RX frame available interrupt
    if ((interrupt & __CAN_IRQ_RX) != 0)
    {
        CAN_Drv_ReadFramePhy(&higherPriorityTaskWoken);
    }

    // Handle TX complete interrupt
    // Handle error interrupts.
    if ((interrupt & __CAN_IRQ_TX) != 0)
    {
        xSemaphoreGiveFromISR(CAN_SemTxComplete, &higherPriorityTaskWoken);
        //Serial.println("Handle TX complete interrupt");

    }

    // Handle TX complete interrupt
    // Handle error interrupts.
    if ((interrupt & (__CAN_IRQ_ERR            // 0x04
                      | __CAN_IRQ_DATA_OVERRUN // 0x08
                      | __CAN_IRQ_WAKEUP       // 0x10
                      | __CAN_IRQ_ERR_PASSIVE  // 0x20
                      | __CAN_IRQ_ARB_LOST     // 0x40
                      | __CAN_IRQ_BUS_ERR      // 0x80
                      )) != 0)
    {
        CAN_error_t error = {
            .RXERR = MODULE_CAN->RXERR.B.RXERR,
            .TXERR = MODULE_CAN->TXERR.B.TXERR,
            .IR = interrupt};

        xQueueSendFromISR(CAN_cfg.err_queue, (void *)&error, &higherPriorityTaskWoken);
    }

    // check if any higher priority task has been woken by any handler
    if (higherPriorityTaskWoken == true)
    {
        portYIELD_FROM_ISR();
    }
}

static void CAN_Drv_ReadFramePhy(BaseType_t *higherPriorityTaskWoken)
{
    // byte iterator
    uint8_t __byte_i;

    // frame read buffer
    CAN_frame_t __frame;

    // check if we have a queue. If not, operation is aborted.
    if (CAN_cfg.rx_queue == NULL)
    {
        // Let the hardware know the frame has been read.
        MODULE_CAN->CMR.B.RRB = 1;
        return;
    }

    // get FIR
    __frame.FIR.U = MODULE_CAN->MBX_CTRL.FCTRL.FIR.U;

    // check if this is a standard or extended CAN frame
    // standard frame
    if (__frame.FIR.B.FF == CAN_frame_std)
    {

        // Get Message ID
        __frame.MsgID = _CAN_GET_STD_ID;

        // deep copy data bytes
        for (__byte_i = 0; __byte_i < __frame.FIR.B.DLC; __byte_i++)
            __frame.data.u8[__byte_i] = MODULE_CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[__byte_i];
    }
    // extended frame
    else
    {

        // Get Message ID
        __frame.MsgID = _CAN_GET_EXT_ID;

        // deep copy data bytes
        for (__byte_i = 0; __byte_i < __frame.FIR.B.DLC; __byte_i++)
            __frame.data.u8[__byte_i] = MODULE_CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[__byte_i];
    }

    // send frame to input queue
    xQueueSendToBackFromISR(CAN_cfg.rx_queue, &__frame, higherPriorityTaskWoken);

    // Let the hardware know the frame has been read.
    MODULE_CAN->CMR.B.RRB = 1;
}

static int CAN_WriteFramePhy(const CAN_frame_t *p_frame)
{
    // byte iterator
    uint8_t __byte_i;
    {
        // wait until previous transmission is completed
        while ((MODULE_CAN->SR.B.TBS == 0) || (MODULE_CAN->SR.B.TCS == 0))
            ;
        MODULE_CAN->MBX_CTRL.FCTRL.FIR.U = 0;
        _CAN_SET_EXT_ID(0);
        // Copy the frame data to the hardware
        for (__byte_i = 0; __byte_i < p_frame->FIR.B.DLC; __byte_i++)
            MODULE_CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[__byte_i] = 0;

        // wait until previous transmission is completed
        while ((MODULE_CAN->SR.B.TBS == 0) || (MODULE_CAN->SR.B.TCS == 0))
            ;
    }
    // copy frame information record
    MODULE_CAN->MBX_CTRL.FCTRL.FIR.U = p_frame->FIR.U;

    // standard frame
    if (p_frame->FIR.B.FF == CAN_frame_std)
    {

        // Write message ID
        _CAN_SET_STD_ID(p_frame->MsgID);

        // Copy the frame data to the hardware
        for (__byte_i = 0; __byte_i < p_frame->FIR.B.DLC; __byte_i++)
            MODULE_CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[__byte_i] = p_frame->data.u8[__byte_i];
    }
    // extended frame
    else
    {

        // Write message ID
        _CAN_SET_EXT_ID(p_frame->MsgID);

        // Copy the frame data to the hardware
        for (__byte_i = 0; __byte_i < p_frame->FIR.B.DLC; __byte_i++)
            MODULE_CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[__byte_i] = p_frame->data.u8[__byte_i];
    }

    ets_delay_us(CAN_FrameDelay);

    // Transmit frame
    MODULE_CAN->CMR.B.TR = 1;

    return 0;
}

void CAN_Drv_EnableInterframeDelay(uint32_t delay)
{
    CAN_FrameDelay = delay;
}

int CAN_Drv_Init(const CAN_device_t *p_devCfg)
{
    // Time quantum
    double __tq;

    if ((p_devCfg == NULL) || (p_devCfg->err_queue == NULL) || (p_devCfg->tx_queue == NULL) || (p_devCfg->rx_queue == NULL))
    {
        return -1;
    }

    memcpy((uint8_t *)&CAN_cfg, p_devCfg, sizeof(CAN_device_t));

    // enable module
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_CAN_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_CAN_RST);

    // configure TX pin
    gpio_set_level(CAN_cfg.tx_pin_id, 1);
    gpio_set_direction(CAN_cfg.tx_pin_id, GPIO_MODE_OUTPUT);
    gpio_matrix_out(CAN_cfg.tx_pin_id, CAN_TX_IDX, 0, 0);
    gpio_pad_select_gpio(CAN_cfg.tx_pin_id);

    // configure RX pin
    gpio_set_direction(CAN_cfg.rx_pin_id, GPIO_MODE_INPUT);
    gpio_matrix_in(CAN_cfg.rx_pin_id, CAN_RX_IDX, 0);
    gpio_pad_select_gpio(CAN_cfg.rx_pin_id);

    // set to PELICAN mode
    MODULE_CAN->CDR.B.CAN_M = 0x1;

    // synchronization jump width is the same for all baud rates
    MODULE_CAN->BTR0.B.SJW = 0x1;

    // TSEG2 is the same for all baud rates
    MODULE_CAN->BTR1.B.TSEG2 = 0x1;

    // select time quantum and set TSEG1
    switch (CAN_cfg.speed)
    {
    case CAN_SPEED_1000KBPS:
        MODULE_CAN->BTR1.B.TSEG1 = 0x4;
        __tq = 0.125;
        break;

    case CAN_SPEED_800KBPS:
        MODULE_CAN->BTR1.B.TSEG1 = 0x6;
        __tq = 0.125;
        break;

    case CAN_SPEED_200KBPS:
        MODULE_CAN->BTR1.B.TSEG1 = 0xc;
        MODULE_CAN->BTR1.B.TSEG2 = 0x5;
        __tq = 0.25;
        break;

    default:
        MODULE_CAN->BTR1.B.TSEG1 = 0xc;
        __tq = ((float)1000 / CAN_cfg.speed) / 16;
    }

    // set baud rate prescaler
    MODULE_CAN->BTR0.B.BRP = (uint8_t)round((((APB_CLK_FREQ * __tq) / 2) - 1) / 1000000) - 1;

    /* Set sampling
     * 1 -> triple; the bus is sampled three times; recommended for low/medium speed buses     (class A and B) where
     * filtering spikes on the bus line is beneficial 0 -> single; the bus is sampled once; recommended for high speed
     * buses (SAE class C)*/
    MODULE_CAN->BTR1.B.SAM = 0x1;

    // enable all interrupts
    esp_chip_info_t chip;
    esp_chip_info (& chip);

    if(chip.revision >= 2)
    {
        MODULE_CAN->IER.U = 0xff;
        MODULE_CAN->IER.B.WUIE = 0;
    }
    else{
        MODULE_CAN->IER.U = 0xff;
    }

    // Set acceptance filter
    MODULE_CAN->MOD.B.AFM = CAN_Filter.FM;
    MODULE_CAN->MBX_CTRL.ACC.CODE[0] = CAN_Filter.ACR0;
    MODULE_CAN->MBX_CTRL.ACC.CODE[1] = CAN_Filter.ACR1;
    MODULE_CAN->MBX_CTRL.ACC.CODE[2] = CAN_Filter.ACR2;
    MODULE_CAN->MBX_CTRL.ACC.CODE[3] = CAN_Filter.ACR3;
    MODULE_CAN->MBX_CTRL.ACC.MASK[0] = CAN_Filter.AMR0;
    MODULE_CAN->MBX_CTRL.ACC.MASK[1] = CAN_Filter.AMR1;
    MODULE_CAN->MBX_CTRL.ACC.MASK[2] = CAN_Filter.AMR2;
    MODULE_CAN->MBX_CTRL.ACC.MASK[3] = CAN_Filter.AMR3;

    // set to normal mode
    MODULE_CAN->OCR.B.OCMODE = __CAN_OC_NOM;

    // clear error counters
    MODULE_CAN->TXERR.U = 0;
    MODULE_CAN->RXERR.U = 0;
    (void)MODULE_CAN->ECC;

    // clear interrupt flags
    (void)MODULE_CAN->IR.U;

    if (CAN_IsrAlloc == false)
    {
        int esp_err;
        // install CAN ISR
        // esp_err = esp_intr_alloc(ETS_CAN_INTR_SOURCE, ESP_INTR_FLAG_IRAM, CAN_Drv_ISR, NULL, NULL);
        esp_err = esp_intr_alloc(ETS_CAN_INTR_SOURCE, 0, CAN_Drv_ISR, NULL, NULL);
        if (esp_err != ESP_OK)
        {
            printf("INFO: CAN ISR failed to alloc <%d>", esp_err);
        }
        else
        {
            CAN_IsrAlloc = true;
        }
    }

    if (CAN_SemTxComplete == NULL)
    {
        // allocate the tx complete semaphore
        CAN_SemTxComplete = xSemaphoreCreateBinary();
    }

    // Showtime. Release Reset Mode.
    MODULE_CAN->MOD.B.RM = 0;

    return 0;
}

int CAN_Drv_WriteFrame(const CAN_frame_t *p_frame)
{
    if (CAN_SemTxComplete == NULL)
    {
        return -1;
    }

    // Write the frame to the controller
    CAN_WriteFramePhy(p_frame);

    // wait for the frame tx to complete
    xSemaphoreTake(CAN_SemTxComplete, portMAX_DELAY);
    

    return 0;
}

int CAN_Drv_Stop(void)
{
    // enter reset mode
    MODULE_CAN->MOD.B.RM = 1;

    return 0;
}

int CAN_Drv_ConfigFilter(const CAN_filter_t *p_filter)
{
    CAN_Filter.FM = p_filter->FM;
    CAN_Filter.ACR0 = p_filter->ACR0;
    CAN_Filter.ACR1 = p_filter->ACR1;
    CAN_Filter.ACR2 = p_filter->ACR2;
    CAN_Filter.ACR3 = p_filter->ACR3;
    CAN_Filter.AMR0 = p_filter->AMR0;
    CAN_Filter.AMR1 = p_filter->AMR1;
    CAN_Filter.AMR2 = p_filter->AMR2;
    CAN_Filter.AMR3 = p_filter->AMR3;

    return 0;
}