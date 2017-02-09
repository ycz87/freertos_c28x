//-------------------------------------------------------------------------------------------------
// Simplified SPI driver.
//
//-------------------------------------------------------------------------------------------------
#include "spi.h"
#include "semphr.h"

//-------------------------------------------------------------------------------------------------
#define SPI_BRR(x)         ((200E6 / 4) / (x)) - 1
#define SPI_CHAR_BITS      8
#define TX_FIFO_INT_LVL    2
#define RX_DUMMY_VALUE     0xBAAD

//-------------------------------------------------------------------------------------------------
typedef enum {
  SPI_TRANSMIT,
  SPI_RECEIVE,
}SpiOperation_E;

static struct {
  SpiOperation_E    operation;
  uint8_t*          Buff;
  uint16_t          BuffSize;
  uint16_t          txBuffIdx;
  uint16_t          rxBuffIdx;
  SemaphoreHandle_t completeEvent;
  StaticSemaphore_t completeEventBuffer;
}SpiState;

//-------------------------------------------------------------------------------------------------
__interrupt void spiTxFifo_ISR(void)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if(SpiState.operation == SPI_TRANSMIT)
  {
    while(   (SpiaRegs.SPIFFTX.bit.TXFFST != 16)
	      && (SpiState.txBuffIdx < SpiState.BuffSize))
    {
      SpiaRegs.SPITXBUF = SpiState.Buff[SpiState.txBuffIdx++] << (16 - SPI_CHAR_BITS);
    }

    if(SpiState.txBuffIdx == SpiState.BuffSize)
    {
      xSemaphoreGiveFromISR(SpiState.completeEvent, &xHigherPriorityTaskWoken);
    }
  }
  else
  {
    while(   (SpiaRegs.SPIFFTX.bit.TXFFST != 16)
	      && (SpiState.txBuffIdx < SpiState.BuffSize))
	{
	  SpiaRegs.SPITXBUF = RX_DUMMY_VALUE;
	  SpiState.txBuffIdx++;
	}

    while(SpiaRegs.SPIFFRX.bit.RXFFST != 0)
    {
      SpiState.Buff[SpiState.rxBuffIdx++] = SpiaRegs.SPIRXBUF;
    }

    if(SpiState.txBuffIdx == SpiState.BuffSize)
    {
      xSemaphoreGiveFromISR(SpiState.completeEvent, &xHigherPriorityTaskWoken);
    }
  }

  SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1;  // Clear Interrupt flag
  PieCtrlRegs.PIEACK.all |= 0x20;       // Issue PIE ACK

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//-------------------------------------------------------------------------------------------------
static void initPins(void)
{
  EALLOW;

  // Enable internal pull-up for the selected pins
  GpioCtrlRegs.GPBPUD.bit.GPIO58 = 0;  // Enable pull-up on GPIO58 (SPIA_MOSI)
  GpioCtrlRegs.GPBPUD.bit.GPIO59 = 0;  // Enable pull-up on GPIO59 (SPIA_MISO)
  GpioCtrlRegs.GPBPUD.bit.GPIO60 = 0;  // Enable pull-up on GPIO60 (SPIA_CLK)

  // Set qualification for selected pins to asynch only
  GpioCtrlRegs.GPBQSEL2.bit.GPIO58 = 3;
  GpioCtrlRegs.GPBQSEL2.bit.GPIO59 = 3;
  GpioCtrlRegs.GPBQSEL2.bit.GPIO60 = 3;
  GpioCtrlRegs.GPBQSEL2.bit.GPIO61 = 3;

  //Configure SPI-A pins using GPIO regs
  GpioCtrlRegs.GPBGMUX2.bit.GPIO58 = 3;
  GpioCtrlRegs.GPBMUX2.bit.GPIO58  = 3;	// Configure GPIO58 as SPIA_MOSI
  GpioCtrlRegs.GPBGMUX2.bit.GPIO59 = 3;
  GpioCtrlRegs.GPBMUX2.bit.GPIO59  = 3;	// Configure GPIO59 as SPIA_MISO
  GpioCtrlRegs.GPBGMUX2.bit.GPIO60 = 3;
  GpioCtrlRegs.GPBMUX2.bit.GPIO60  = 3;	// Configure GPIO60 as SPIA_CLK

  EDIS;

  GPIO_SetupPinMux(61, GPIO_MUX_CPU1, 0);
  GPIO_SetupPinOptions(61, GPIO_OUTPUT, GPIO_PUSHPULL);
}

//-------------------------------------------------------------------------------------------------
static void initSpia(void)
{
  // Set reset low before configuration changes
  // Clock polarity (0 == rising, 1 == falling)
  // 16-bit character
  // Enable HS mode
  SpiaRegs.SPICCR.bit.SPISWRESET   = 0;
  SpiaRegs.SPICCR.bit.CLKPOLARITY  = 1;
  SpiaRegs.SPICCR.bit.HS_MODE      = 1;
  SpiaRegs.SPICCR.bit.SPICHAR      = (SPI_CHAR_BITS-1);

  // Enable master (0 == slave, 1 == master)
  // Enable transmission (Talk)
  // Clock phase (0 == normal, 1 == delayed)
  // SPI interrupts are disabled
  SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1;
  SpiaRegs.SPICTL.bit.TALK         = 1;
  SpiaRegs.SPICTL.bit.CLK_PHASE    = 0;
  SpiaRegs.SPICTL.bit.SPIINTENA    = 0;

  // Initialize SPI TX_FIFO register
  SpiaRegs.SPIFFTX.bit.TXFFIL      = TX_FIFO_INT_LVL;
  SpiaRegs.SPIFFTX.bit.TXFFIENA    = 0;
  SpiaRegs.SPIFFTX.bit.TXFFINTCLR  = 1;
  SpiaRegs.SPIFFTX.bit.TXFIFO      = 1;
  SpiaRegs.SPIFFTX.bit.SPIFFENA    = 1;
  SpiaRegs.SPIFFTX.bit.SPIRST      = 1;

  // Initialize SPI RX_FIFO register
  SpiaRegs.SPIFFRX.bit.RXFFIL      = 0x10;
  SpiaRegs.SPIFFRX.bit.RXFFIENA    = 0;
  SpiaRegs.SPIFFRX.bit.RXFFINTCLR  = 1;
  SpiaRegs.SPIFFRX.bit.RXFIFORESET = 1;

  // Initialize SPI CT_FIFO register
  SpiaRegs.SPIFFCT.all = 0;

  SpiaRegs.SPIFFRX.all = 0x2044;
  SpiaRegs.SPIFFCT.all = 0x0;

  // Set the baud rate
  SpiaRegs.SPIBRR.bit.SPI_BIT_RATE = SPI_BRR(400E3);

  // Set FREE bit
  // Halting on a breakpoint will not halt the SPI
  SpiaRegs.SPIPRI.bit.FREE = 1;

  // Configure interrupts
  EALLOW;
  PieVectTable.SPIA_TX_INT = &spiTxFifo_ISR;
  EDIS;
  PieCtrlRegs.PIECTRL.bit.ENPIE = 1;     // Enable the PIE block
  PieCtrlRegs.PIEIER6.bit.INTx2 = 1;     // Enable PIE Group 6, INT 2
  IER=M_INT6;                            // Enable CPU INT6

  // Release the SPI from reset
  SpiaRegs.SPICCR.bit.SPISWRESET = 1;
}

//-------------------------------------------------------------------------------------------------
void SPI_open(void)
{
  // Init OS primitives.
  SpiState.completeEvent = xSemaphoreCreateBinaryStatic(&SpiState.completeEventBuffer);

  initPins();
  initSpia();
}

//-------------------------------------------------------------------------------------------------
void SPI_close(void)
{

}

//-------------------------------------------------------------------------------------------------
uint16_t spi_sendByte(uint16_t byte)
{
  SpiaRegs.SPITXBUF = byte;         			//Transmit Byte
  while(SpiaRegs.SPIFFRX.bit.RXFFST == 0); 	    //Wait until the RX FIFO has received one byte
  return (SpiaRegs.SPIRXBUF << 8);			    //Read Byte from RXBUF and return
}

//-------------------------------------------------------------------------------------------------
uint16_t SPI_send(uint8_t* buff, uint16_t buffSize, TickType_t timeout)
{
  // Init transmitter state.
  SpiState.operation  = SPI_TRANSMIT;
  SpiState.Buff       = buff;
  SpiState.BuffSize   = buffSize;
  SpiState.txBuffIdx  = 0;

  // Clear TX interrupt flag to avoid just to be sure its not set.
  SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1;

  // Fill in TX FIFO with data.
  while(   (SpiaRegs.SPIFFTX.bit.TXFFST != 16)
  		&& (SpiState.txBuffIdx < SpiState.BuffSize))
  {
	SpiaRegs.SPITXBUF = SpiState.Buff[SpiState.txBuffIdx++] << (16 - SPI_CHAR_BITS);
  }

  // If there are still data in the buffer wait
  // until interrupt driven transmit completed.
  if(SpiState.txBuffIdx < SpiState.BuffSize)
  {
	SpiaRegs.SPIFFTX.bit.TXFFIENA   = 1;
    xSemaphoreTake(SpiState.completeEvent, timeout);
    SpiaRegs.SPIFFTX.bit.TXFFIENA   = 0;
  }

  return SpiState.txBuffIdx;
}

//-------------------------------------------------------------------------------------------------
uint16_t SPI_receive(uint8_t* buff, uint16_t buffSize, TickType_t timeout)
{
  // Init transmitter state.
  SpiState.operation = SPI_RECEIVE;
  SpiState.Buff      = buff;
  SpiState.BuffSize  = buffSize;
  SpiState.rxBuffIdx = 0;
  SpiState.txBuffIdx = 0;

  // Clear TX interrupt flag to avoid just to be sure its not set.
  SpiaRegs.SPIFFTX.bit.TXFFINTCLR = 1;

  // Fill in TX FIFO with dummy values.
  while(   (SpiaRegs.SPIFFTX.bit.TXFFST != 16)
   		&& (SpiState.txBuffIdx < SpiState.BuffSize))
  {
    SpiaRegs.SPITXBUF = RX_DUMMY_VALUE;
    SpiState.txBuffIdx++;
  }
  
  // If buffer is larger than FIFO level wait
  // until interrupt driven receive completed.
  if(SpiState.txBuffIdx < SpiState.BuffSize)
  {
  	SpiaRegs.SPIFFTX.bit.TXFFIENA   = 1;
    xSemaphoreTake(SpiState.completeEvent, timeout);
    SpiaRegs.SPIFFTX.bit.TXFFIENA   = 0;
  }

  // Wait untill the rest of dummy values in TX FIFO is processed
  // and read the result from RX FIFO.
  while(SpiaRegs.SPIFFTX.bit.TXFFST != 0);
  while(SpiaRegs.SPIFFRX.bit.RXFFST != 0)
  {
    SpiState.Buff[SpiState.rxBuffIdx++] = SpiaRegs.SPIRXBUF;
  }

  return SpiState.rxBuffIdx;
}

//-------------------------------------------------------------------------------------------------
void SPI_setCsHigh(void)
{
  GPIO_WritePin(61, 1);
}

//-------------------------------------------------------------------------------------------------
void SPI_setCsLow(void)
{
  GPIO_WritePin(61, 0);
}

//-------------------------------------------------------------------------------------------------
void SPI_setClockFreq(uint32_t freqHz)
{
  SpiaRegs.SPIBRR.bit.SPI_BIT_RATE = SPI_BRR(freqHz);
}
