/*
 Enc28J60NetworkClass.h
 UIPEthernet network driver for Microchip ENC28J60 Ethernet Interface.

 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 inspired by enc28j60.c file from the AVRlib library by Pascal Stang.
 For AVRlib See http://www.procyonengineering.com/

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef Enc28J60Network_H_
#define Enc28J60Network_H_

#include "mempool.h"

#define ENC28J60_CONTROL_CS   4 //SS
#define SPI_MOSI              13 //MOSI
#define SPI_MISO              12 //MISO
#define SPI_SCK               14 //SCK

extern uint8_t ENC28J60ControlCS;

#if defined(__MBED__) || defined(ARDUINO_ARCH_SAM) || defined(ARDUINO_ARCH_SAMD) || defined(__ARDUINO_ARC__) || defined(__STM32F1__) || defined(__STM32F3__) || defined(STM32F3) || defined(__STM32F4__) || defined(STM32F2) || defined(ESP8266) || defined(ARDUINO_ARCH_AMEBA) || defined(__MK20DX128__) || defined(__MKL26Z64__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__) || defined(__RFduino__) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_MEGAAVR)
   #if defined(ARDUINO) && (!defined(ARDUINO_ARCH_STM32) && defined(STM32F3))
      #include "HardwareSPI.h"
   #else
      #include <SPI.h>
   #endif
   #define ENC28J60_USE_SPILIB 1
#endif

#define UIP_RECEIVEBUFFERHANDLE 0xff

#define UIP_SENDBUFFER_PADDING 7
#define UIP_SENDBUFFER_OFFSET 1

/*
 * Empfangen von ip-header, arp etc...
 * wenn tcp/udp -> tcp/udp-callback -> assign new packet to connection
 */

#define TX_COLLISION_RETRY_COUNT 10

class Enc28J60Network : public MemoryPool
{

private:
  static uint16_t nextPacketPtr;
  static uint8_t bank;
  static uint8_t erevid;

  static struct memblock receivePkt;

  static bool broadcast_enabled; //!< True if broadcasts enabled (used to allow temporary disable of broadcast for DHCP or other internal functions)

  static uint8_t readOp(uint8_t op, uint8_t address);
  static void writeOp(uint8_t op, uint8_t address, uint8_t data);
  static uint16_t setReadPtr(memhandle handle, memaddress position, uint16_t len);
  static void setERXRDPT(void);
  static void readBuffer(uint16_t len, uint8_t* data);
  static void writeBuffer(uint16_t len, uint8_t* data);
  static uint8_t readByte(uint16_t addr);
  static void writeByte(uint16_t addr, uint8_t data);
  static void setBank(uint8_t address);
  static uint8_t readReg(uint8_t address);
  static void writeReg(uint8_t address, uint8_t data);
  static void writeRegPair(uint8_t address, uint16_t data);
  static void phyWrite(uint8_t address, uint16_t data);
  static uint16_t phyRead(uint8_t address);
  static void clkout(uint8_t clk);

  static void enableBroadcast (bool temporary);
  static void disableBroadcast (bool temporary);
  static void enableMulticast (void);
  static void disableMulticast (void);

  static uint8_t readRegByte (uint8_t address);
  static void writeRegByte (uint8_t address, uint8_t data);

  friend void enc28J60_mempool_block_move_callback(memaddress,memaddress,memaddress);

public:

  void powerOn(void);
  void powerOff(void);
  static uint8_t geterevid(void);
  uint16_t PhyStatus(void);
  static bool linkStatus(void);

  static void initSPI();
  static void init(uint8_t* macaddr);
  static memhandle receivePacket(void);
  static void freePacket(void);
  static memaddress blockSize(memhandle handle);
  static bool sendPacket(memhandle handle);
  static uint16_t readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  static uint16_t writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  static void copyPacket(memhandle dest, memaddress dest_pos, memhandle src, memaddress src_pos, uint16_t len);
  static uint16_t chksum(uint16_t sum, memhandle handle, memaddress pos, uint16_t len);
};

#endif /* Enc28J60NetworkClass_H_ */
