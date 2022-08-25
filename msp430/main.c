#include <msp430.h> 
#include <stdint.h>


#define RXD BIT1
#define TXD BIT2
#define BUFFER_SIZE 15

volatile uint16_t duration = 0;       // for tracking duration of "capacitor" charging
volatile uint32_t base_duration, base_duration_2;  // duration untouched configured when booted
uint8_t buffer = 0;                   // debouncing for touch sensor 1
uint8_t buffer_2 = 0;                 // debouncing for touch sensor 2
char message;                         // buffer for message sent through UART

uint8_t i;
uint16_t total;

void send_char(const char c);

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	P1DIR = BIT4 | BIT6;  // 1.6 and 1.4 as output
	P1DIR |= BIT5 | BIT7;  // used to discharge capacitor quickly
	P1OUT = 0;

	// ---------------------------------------------
	// set clock for 1MHz
	// ---------------------------------------------
    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;  // use calibration data for 1MHz
    DCOCTL = CALDCO_1MHZ;   // use calibration data for 1MHz

    // ---------------------------------------------
	// timer setup
    // ---------------------------------------------
    // capture/compare register
    TA0CCTL0 |= CAP     // capture mode
              | CM_3    // capture on both edge
              | CCIS_3  // capture input CCI0B
              | SCS;    // synchronous capture
    // timer a
    TA0CTL |= TASSEL_2  // use SMCLK
            | MC_2      // timer in continuous mode
            | TACLR;    // clear count register

    // ---------------------------------------------
    // configure UART for 9600 Baud Rate
    // ---------------------------------------------
    UCA0CTL1 = UCSWRST;     // put into reset state
    // configure pins to use for UART
    P1DIR |= TXD;           // set as output pin
    P1SEL |= RXD | TXD;     // set pins for uart mode
    P1SEL2 |= RXD | TXD;    // set pins for uart mode
    UCA0CTL1 |= UCSSEL_2;   // use SMCLK as source - 1MHz
    UCA0BR0 = 104;          // UCBRx = 104
    UCA0BR1 = 0;            // nothing about this in user guide
    UCA0MCTL = UCBRS_1;     // UCBRSx = 1 ; UCBRFx = 0
    UCA0CTL1 &= ~UCSWRST;   // take out of reset state

    // ----------------------------------------------
	// setup for comparator
    // ----------------------------------------------
	CACTL2 |= P2CA1 | P2CA2 | P2CA3  // pin 1.7 as (-)
	        | P2CA0 //| P2CA4          // pin 1.0 as (+)
	        | CAF;                   // filtering
	CACTL1 |= CAIE                   // enable interrupt
	        | CAIES                  // interrupt on falling edge
	        | CAREF_0;               // dont use internal reference generator
	CACTL1 |= CAON;                  // turn on comparator
	CACTL1 &= ~CAIFG;                // unset interrupt flag
	__enable_interrupt();

	// ----------------------------------------------
	// determine base_duration
	// ----------------------------------------------

	// ----------------------------------------------
	// touch sensor 1
	// ----------------------------------------------
	total = 0;
	for (i = 8; i > 0; --i) {
	    while(CACTL2 & CAOUT == 0); // wait until comparator read 1
        TA0CTL |= TACLR;     // reset timer and start it
        P1OUT |= BIT6;       // start charging "capacitor"
        LPM0;                // disable cpu and mclk waiting for comparator interrupt
        P1OUT &= ~BIT6;      // stop charging "capacitor"
        total += duration;
        __delay_cycles(600);
	}
	base_duration = total / 8 + 100;

	// switch to second touch sensor
    CACTL2 &= ~P2CA2;    // pin 1.5 as (-)
    CAPD |= CAPD5;       // pin 1.5 disable gpio
    CAPD &= ~CAPD2;      // pin 1.7 enable gpio to discharge cap

    // ----------------------------------------------
    // touch sensor 2
    // ----------------------------------------------
    total = 0;
    for (i = 8; i > 0; --i) {
        while(CACTL2 & CAOUT == 0); // wait until comparator read 1
        TA0CTL |= TACLR;     // reset timer and start it
        P1OUT |= BIT4;       // start charging "capacitor"
        LPM0;                // disable cpu and mclk waiting for comparator interrupt
        P1OUT &= ~BIT4;      // stop charging "capacitor"
        total += duration;
        __delay_cycles(600);
    }
    base_duration_2 = total / 8 + 100;
    // switch to first touch sensor
    CACTL2 |= P2CA2;     // pin 1.7 as input
    CAPD |= CAPD7;       // pin 1.7 disable gpio
    CAPD &= ~CAPD5;       // pin 1.5 enable gpio to discharge cap

	while(1) {
	    // ------------------------------------------
	    // touch sensor 1
	    // ------------------------------------------
	    while(CACTL2 & CAOUT == 0); // wait until comparator read 1
	    TA0CTL |= TACLR;     // reset timer and start it
	    P1OUT |= BIT6;       // start charging "capacitor"
	    LPM0;                // disable cpu and mclk waiting for comparator interrupt
	    P1OUT &= ~BIT6;      // stop charging "capacitor"

	    if (duration > base_duration) {
            buffer = BUFFER_SIZE;
        } else if (buffer > 0){
            --buffer;
        }

	    // switch to second touch sensor
	    CACTL2 &= ~P2CA2;    // pin 1.5 as (-)
	    CAPD |= CAPD5;       // pin 1.5 disable gpio
	    CAPD &= ~CAPD2;      // pin 1.7 enable gpio to discharge cap

	    // ------------------------------------------
	    // touch sensor 2
	    // ------------------------------------------
	    while(CACTL2 & CAOUT == 0); // wait until comparator read 1
	    TA0CTL |= TACLR;     // reset timer and start it
	    P1OUT |= BIT4;       // start charging "capacitor"
	    LPM0;                // disable cpu and mclk waiting for comparator interrupt
	    P1OUT &= ~BIT4;      // stop charging "capacitor"

	    // switch to first touch sensor
	    CACTL2 |= P2CA2;     // pin 1.7 as input
	    CAPD |= CAPD7;       // pin 1.7 disable gpio
	    CAPD &= ~CAPD5;       // pin 1.5 enable gpio to discharge cap


	    if (duration > base_duration_2) {
            buffer_2 = BUFFER_SIZE;
        } else if (buffer_2 > 0){
            --buffer_2;
        }

	    // -----------------------------------------
	    // Send UART message
	    // -----------------------------------------
	    // BIT0 and BIT1 represent touchpads
	    // 1 is unpressed 0 is pressed
	    message = 0xFF;
        if (buffer > 0) {
            message &= ~BIT0;
        }
        if (buffer_2 > 0) {
            message &= ~BIT1;
        }
        send_char(message);
        __delay_cycles(50);
	}  // while
}  // main


#pragma vector = COMPARATORA_VECTOR
__interrupt void COMPA_ISR(void) {
    TA0CCTL0 ^= CCIS0;   // trigger capture
    duration = TA0CCR0;  // store duration in variable
    LPM0_EXIT;           // enable cpu and mclk
}

void send_char(const char c) {
    while (!(IFG2 & UCA0TXIFG));  // wait until the transmit buffer is ready
    UCA0TXBUF = c;                // put char into the transmit buffer
}
