/*
 * lpc1700_spi.c
 *
 *  Created on: Nov 10, 2018
 *      Author: jan
 */

/*
 * SPI interface functions for LPC1700, following the TI_CC_SPI* api
 */



#include "TI_CC_spi.h"
#include "TI_CC_CC1100-CC2500.h"
#include "board.h"
#include "arch/lpc_arch.h" /* msDelay() */
#include "my_debug.h"
#include "ssp_17xx_40xx.h"

/* Assert SSEL pin */
static void chip_enable(void)
{
	Chip_GPIO_WritePortBit(LPC_GPIO, 0, 6, false);
	msDelay(1);
}


/* De-Assert SSEL pin */
static void chip_disable(void)
{
	Chip_GPIO_WritePortBit(LPC_GPIO, 0, 6, true);
}

static uint32_t spi_write_blocking(char* cmd, size_t cmd_len) {
	uint32_t ret = Chip_SSP_WriteFrames_Blocking(LPC_SSP1, (uint8_t*) cmd, cmd_len);

	while (Chip_SSP_GetStatus(LPC_SSP1, SSP_STAT_BSY));

	return ret;
}

static uint32_t spi_read_blocking(char *buffer, size_t buffer_len) {
	return Chip_SSP_ReadFrames_Blocking(LPC_SSP1, (uint8_t*) buffer, buffer_len);

}




//------------------------------------------------------------------------------
//  void TI_CC_SPISetup(void)
//
//  DESCRIPTION:
//  Configures the assigned interface to function as a SPI port and
//  initializes it.
void TI_CC_SPISetup(void) {

	// Init SPI peripheral

	/* Set up clock and muxing for SSP1 (SPI) interface */
	/*
	 * Initialize SSP1 pins connect
	 * P0.7: SCK1
	 * P0.6: SSEL (chip enable, CE, as normal GPIO)
	 * P0.8: MISO1
	 * P0.9: MOSI1
	 *
	 */
	Chip_IOCON_PinMux(LPC_IOCON, 0, 7, IOCON_MODE_INACT, IOCON_FUNC2);
	Chip_IOCON_PinMux(LPC_IOCON, 0, 8, IOCON_MODE_INACT, IOCON_FUNC2);
	Chip_IOCON_PinMux(LPC_IOCON, 0, 9, IOCON_MODE_INACT, IOCON_FUNC2);

	/* Setup chipsel pin, as output with pull-up */
	Chip_IOCON_PinMux(LPC_IOCON, 0, 6, IOCON_MODE_PULLUP, IOCON_FUNC0);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 6, true);

	/* SPI initialization */
	Chip_SSP_Init(LPC_SSP1);
	Chip_SSP_SetBitRate(LPC_SSP1, 1000000); // 1000000 => 1 MHz
	//Chip_SSP_SetFormat(LPC_SSP1, SSP_BITS_8, CHIP_SSP_FRAME_FORMAT_TI, SSP_CLOCK_CPHA0_CPOL0);

	Chip_SSP_Enable(LPC_SSP1);

	/* Set chipsel to high to disable chip */
	chip_disable();

}

void TI_CC_PowerupResetCCxxxx(void) {
	chip_disable();
	TI_CC_Wait(30);
	chip_enable();
	TI_CC_Wait(30);
	chip_disable();
	TI_CC_Wait(45);
	TI_CC_SPIStrobe(TI_CCxxx0_SRES);
}

//------------------------------------------------------------------------------
//  void TI_CC_SPIWriteReg(char addr, char value)
//
//  DESCRIPTION:
//  Writes "value" to a single configuration register at address "addr".

void TI_CC_SPIWriteReg(char addr, char value) {
	char data[2];
	data[0] = addr;
	data[1] = value;
	chip_enable();
	spi_write_blocking(data, 2);
	chip_disable();
}

//------------------------------------------------------------------------------
//  void TI_CC_SPIWriteBurstReg(char addr, char *buffer, char count)
//
//  DESCRIPTION:
//  Writes values to multiple configuration registers, the first register being
//  at address "addr".  First data byte is at "buffer", and both addr and
//  buffer are incremented sequentially (within the CCxxxx and MSP430,
//  respectively) until "count" writes have been performed.

void TI_CC_SPIWriteBurstReg(char addr, char* buffer, char count) {
	chip_enable();
	char burst_addr = addr | TI_CCxxx0_WRITE_BURST;
	spi_write_blocking(&burst_addr, 1);
	spi_write_blocking(buffer, count);
	chip_disable();
}

//------------------------------------------------------------------------------
//  char TI_CC_SPIReadReg(char addr)
//
//  DESCRIPTION:
//  Reads a single configuration register at address "addr" and returns the
//  value read.

char TI_CC_SPIReadReg(char addr) {
	chip_enable();
	char read_addr = addr | TI_CCxxx0_READ_SINGLE;
	spi_write_blocking(&read_addr, 1);

	char data = 0;
	int ret = spi_read_blocking(&data, 1);
	if (ret != 1) {
		MY_DEBUG_VALUE("SPI unexpected read: ", ret);
	}
	chip_disable();
	return data;
}

//------------------------------------------------------------------------------
//  void TI_CC_SPIReadBurstReg(char addr, char *buffer, char count)
//
//  DESCRIPTION:
//  Reads multiple configuration registers, the first register being at address
//  "addr".  Values read are deposited sequentially starting at address
//  "buffer", until "count" registers have been read.


void TI_CC_SPIReadBurstReg(char addr, char *buffer, char count) {
	chip_enable();

	char burst_addr = addr | TI_CCxxx0_READ_BURST;
	spi_write_blocking(&burst_addr, 1);
	int ret = spi_read_blocking(buffer, count);
	if (ret != count) {
		MY_DEBUG_VALUE("SPI unexpected ret!=count when read burst, ret=", ret);
	}
	chip_disable();
}

//------------------------------------------------------------------------------
//  char TI_CC_SPIReadStatus(char addr)
//
//  DESCRIPTION:
//  Special read function for reading status registers.  Reads status register
//  at register "addr" and returns the value read.

char TI_CC_SPIReadStatus(char addr) {
	chip_enable();
	char status_addr = addr | TI_CCxxx0_READ_BURST;
	spi_write_blocking(&status_addr, 1);
	char value = 0;
	int ret = spi_read_blocking(&value, 1);
	if (ret != 1) {
		MY_DEBUG_STR("SPI unexpected ret!=1 when reading status");
	}
	chip_disable();
	return value;
}

//------------------------------------------------------------------------------
//  void TI_CC_SPIStrobe(char strobe)
//
//  DESCRIPTION:
//  Special write function for writing to command strobe registers.  Writes
//  to the strobe at address "addr".
//------------------------------------------------------------------------------

void TI_CC_SPIStrobe(char addr) {
	char cmd = addr;
	chip_enable();
	spi_write_blocking(&cmd, 1);
	chip_disable();
}

void TI_CC_Wait(unsigned int cycles) {
	if (cycles < 1000) {
		msDelay(1);
	}
	else {
		msDelay(cycles / 1000);
	}
}
