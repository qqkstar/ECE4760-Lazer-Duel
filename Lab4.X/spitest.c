
// i/o names 
#define _SUPPRESS_PLIB_WARNING 1
#include <plib.h>

// frequency we're running at
#define	SYS_FREQ 64000000
// serial stuff
#include <stdio.h>
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
#include <stdlib.h>
// protoThreads environment
#include "pt_cornell_TFT.h"
#include "nrf24l01.h"

// PIN Setup
// SCK -> SCK2 (pin 26)
// SDI -> MISO (RPA4) (pin 12)
// SDO -> MOSI (RPB2) (pin 9)
// IRQ -> extern interrupt 1 (RPB10) (pin 21)
// CSN -> RPB7 (I/O) (pin 16)
// CE -> RPB8 (I/O) (pin 17)

static char send; // 5 byte address for testing
static char receive; // data read from the address


int main(void) {
    TX = 0;
    send = 0xBB;
    INTEnableSystemMultiVectoredInt();
    // Set outputs to CE and CSN
    TRIS_csn = 0;
    TRIS_ce = 0;

    init_SPI();
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    //240x320 vertical display
    tft_setRotation(0); // Use tft_setRotation(1) for 320x240

    tft_setCursor(0, 60);
    tft_setTextColor(ILI9340_MAGENTA);
    tft_setTextSize(2);
    tft_writeString("Start");
    // write the 5 byte address to pipe 1
    nrf_pwrup(); //Go to standby

    // set the payload width to 1 bytes
    payload_size = 1;
    nrf_write_reg(nrf24l01_RX_PW_P0, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P1, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P2, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P3, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P4, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P5, &payload_size, 1);

    // Disable auto ack
    //char disable_ack = nrf24l01_EN_AA_ENAA_NONE;
    //nrf_write_reg(nrf24l01_EN_AA, &disable_ack, 1);

    //_TRIS_LEDRED = 0;
    //_TRIS_LEDYELLOW = 0;
    //_LEDRED = 0;
    //_LEDYELLOW = 0;
    nrf_flush_rx();
   
    while (1) {
        // if transmitter
        if (TX) {
            nrf_send_payload(&send, 1);
            send = send + 1;
            delay_ms(1000); // wait a bit before sending it again
            //_LEDYELLOW = 0;
            //_LEDRED = 0;
            delay_ms(1000);
        } else {
            nrf_rx_mode();
            while (1) {
           
                receive = RX_payload[0];
                //nrf_flush_rx();
                if (received == 1) {
                    tft_fillScreen(ILI9340_BLACK);
                    //_LEDRED = 0;

                    delay_ms(1000);
                    tft_setCursor(0, 60);
                    tft_setTextColor(ILI9340_MAGENTA);
                    tft_setTextSize(2);
                    tft_writeString("Sent");
                    nrf_read_reg(nrf24l01_STATUS, &status, 1);
                    tft_setCursor(0, 300);
                    tft_setTextColor(ILI9340_YELLOW);
                    tft_setTextSize(2);
                    sprintf(buffer, "%X", receive);
                    tft_writeString(buffer);
                    received = 0;
                    receive = 0;
                    nrf_flush_rx();
                }
            }
        }
    }
} // main
