/*****************************************************************************
 *
 * File: widePayloadRx.c
 * Andrew Kessler, 8/2010
 *
 * This file waits to recieve a packet of a user defined width,
 * then outputs it to the screen via RS232. "Width" must be a
 *  non-zero positive integer no more than 32, and match the
 * width of the transmitter. It can be adjusted using the "[" and "]" keys.
 *
 *
 ******************************************************************************/



#include "p32xxxx.h"
#include "plib.h"
#include "config.h"
#include "nrf24l01.h"

#define DESIRED_BAUDRATE       (115200)      // The desired BaudRate for RS232 communication w/ UART
#define LED1            PORTDbits.RD0
#define LED2            PORTDbits.RD1
#define LED3            PORTDbits.RD2

void Initialize(void);
void InitializeIO(void);
void SpiInitDevice(int, int, int, int);
void initUART1(int);
void Delayus(unsigned);
void MainLoop(void);
unsigned char spi_send_read_byte(unsigned char byte);

int width;
char RS232_Out_Buffer[64]; // buffer for printing data to the screen via UART

int main(void)
{
    Initialize(); //initialize IO, UART, SPI, set up nRF24L01 as RX
    sprintf(RS232_Out_Buffer, "*** Reciever Intialized (%i)*** \r\n", width);
    putsUART1(RS232_Out_Buffer);
    MainLoop();

}

void MainLoop()
{
    width = 1;
    char data[width]; //register to hold letter sent and received
    int i = 0;

    while (1)
    {
        PORTD;
        while (!(nrf24l01_irq_pin_active()));
        while (!nrf24l01_irq_rx_dr_active());

        LED2 = ~LED2;
        nrf24l01_read_rx_payload(data, width); //get the payload into data
        nrf24l01_irq_clear_all(); //clear interrupts again

        for (i = 0; i < width; i++)
        {
            sprintf(RS232_Out_Buffer, "%c", data[i]);
            putsUART1(RS232_Out_Buffer);
        }
        putsUART1("\r\n");

        Delayus(130); //wait for receiver to come from standby to RX

        LED3 = ~LED3;

    }
}
//initialize routine

void Initialize()
{
    int pbClk = SYSTEMConfigPerformance(SYS_FREQ);
    InitializeIO(); //set up IO (directions and functions)
    initUART1(pbClk);
    SpiInitDevice(1, 1, 0, 0);
    nrf24l01_initialize_debug(true, width, false); //initialize the 24L01 to the debug configuration as RX, 1 data byte, and auto-ack disabled

}

//initialize IO pins

void InitializeIO(void)
{
    //Initializing CSN and CE pins as output
    mPORTFSetPinsDigitalOut(BIT_0 | BIT_1);
    mPORTFClearBits(BIT_0 | BIT_1);
    //Initializing IRQ pin as input
    mPORTDSetPinsDigitalIn(BIT_13);
    mPORTDClearBits(BIT_13);
    //Setting CSN bit active
    LATFbits.LATF1 = 1;
    //Initializing LEDs
    mPORTDSetPinsDigitalOut(BIT_1 | BIT_2);
    mPORTDClearBits(BIT_1 | BIT_2);
    //Initializing MOSI/SDO1 and SCK as output
    mPORTDSetPinsDigitalOut(BIT_10);
    mPORTDSetPinsDigitalOut(BIT_0);
    //Initializing MISO/SDI1 as input
    mPORTCSetPinsDigitalIn(BIT_4);
}

void Delayus(unsigned t)
{
    OpenTimer1(T1_ON | T1_PS_1_256, 0xFFFF);
    while (t--)
    { // t x 1ms loop
        WriteTimer1(0);
        while (ReadTimer1() < SYS_FREQ / 256 / 1000000);
    }
    CloseTimer1();
} // Delayus

unsigned char spi_send_read_byte(unsigned char byte)
{
    unsigned short txData, rxData; // transmit, receive characters
    int chn = 1; // SPI channel to use (1 or 2)

    txData = byte; // take inputted byte and store into txData
    SpiChnPutC(chn, txData); // send data
    rxData = SpiChnGetC(chn); // retreive over channel chn the received data into rxData

    return rxData;
}

void SpiInitDevice(int chn, int isMaster, int frmEn, int frmMaster)
{
    unsigned int config = SPI_CON_MODE8 | SPI_CON_SMP | SPI_CON_ON; // SPI configuration word
    if (isMaster)
    {
        config |= SPI_CON_MSTEN;
    }
    if (frmEn)
    {
        config |= SPI_CON_FRMEN;
        if (!frmMaster)
        {
            config |= SPI_CON_FRMSYNC;
        }
    }
    SpiChnOpen(chn, config, 160); // divide fpb by 4, configure the I/O ports. Not using SS in this example
}

void initUART1(int pbClk)
{
#define config1    UART_EN | UART_IDLE_CON | UART_RX_TX | UART_DIS_WAKE | UART_DIS_LOOPBACK | UART_DIS_ABAUD | UART_NO_PAR_8BIT | UART_1STOPBIT | UART_IRDA_DIS | UART_DIS_BCLK_CTS_RTS| UART_NORMAL_RX | UART_BRGH_SIXTEEN
#define config2      UART_TX_PIN_LOW | UART_RX_ENABLE | UART_TX_ENABLE | UART_INT_TX | UART_INT_RX_CHAR | UART_ADR_DETECT_DIS | UART_RX_OVERRUN_CLEAR
    OpenUART1(config1, config2, pbClk / 16 / DESIRED_BAUDRATE - 1); // calculate actual BAUD generate value.
    ConfigIntUART1(UART_INT_PR2 | UART_RX_INT_EN);
    INTEnableSystemMultiVectoredInt();
}

void __ISR(_UART1_VECTOR, ipl2) IntUart1Handler(void)
{
    // Is this an RX interrupt?
    if (mU2RXGetIntFlag())
    {
        // Clear the RX interrupt Flag
        mU2RXClearIntFlag();

        char change = (char) ReadUART1();

        if (change == ']')
        {
            width++;
            if (width > 32)
            {
                width = width - 32;
            }
        }

        if (change == '[')
        {
            width--;
            if (width < 1)
            {
                width = width + 32;
            }
        }

        // Echo what we just received.
        sprintf(RS232_Out_Buffer, "Width: %d \r\n", width);
        putsUART1(RS232_Out_Buffer);

        //reset chip to new width
        nrf24l01_initialize_debug(true, width, false);
    }

    // We don't care about TX interrupt
    if (mU2TXGetIntFlag())
    {
        mU2TXClearIntFlag();
    }
}