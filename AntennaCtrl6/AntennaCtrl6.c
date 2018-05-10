/*
 * main.c
 *
 * Created: 3/23/2017 12:32:50 PM
 * Author : Lou, Yuyi Shen, Bonny Chen
 */ 

#include "AntCtrl.h"

#define bit_set(p,m) ((p) |= (1 << m))
#define bit_clear(p,m) ((p) &= ~(1 << m))

volatile int usb_connected = 0;
static FILE USBSerialStream;
/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
	.Config =
	{
		.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
		.DataINEndpoint           =
		{
			.Address          = CDC_TX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.DataOUTEndpoint =
		{
			.Address          = CDC_RX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.NotificationEndpoint =
		{
			.Address          = CDC_NOTIFICATION_EPADDR,
			.Size             = CDC_NOTIFICATION_EPSIZE,
			.Banks            = 1,
		},
	},
};

void usb_rx(void)
{
	char buf[10];
	char numbuf[10];
	char returnbuf[10];
	int i, flag;
	char *errvar;
	char *testparse;
	long epmNumber;
	int pulseerror = 0;
	errvar = fgets(buf, 10, &USBSerialStream);
	if (errvar != NULL) {
		if ((buf[0] != '+') && (buf[0] != '-')) {
			fputs("Commands must start with + or -.", &USBSerialStream);
			return;
		}
		i = 1;
		flag = 0;
		while (i < 10) {
			if (buf[i] == '\\') {
				flag = 1;
			}
			if (!flag) {
				numbuf[i-1] = buf[i];
			}
			i++;
		}
		errno = 0;
		long epmNumber = strtol(numbuf, NULL, 10);
		if ((errno != 0) || epmNumber == 0) {
			fputs("Bad command", &USBSerialStream);
			return;
		}
		if (buf[0] == '+') {
			pulseerror = SendPulse(1, (int)(epmNumber-1));
			if (pulseerror < 0) {
				fputs("Pulse failed", &USBSerialStream);
			} else {
				sprintf(returnbuf, "%d", (int)epmNumber);
				fputs("Positive Pulse sent to ", &USBSerialStream);
				fputs(returnbuf, &USBSerialStream);
			}
		} else if (buf[0] == '-') {
			pulseerror = SendPulse(0, (int)(epmNumber-1));
			if (pulseerror < 0) {
				fputs("Pulse failed", &USBSerialStream);
			} else {
				sprintf(returnbuf, "%d", (int)epmNumber);
				fputs("Negative Pulse sent to ", &USBSerialStream);
				fputs(returnbuf, &USBSerialStream);
			}
		}
		
	}
}

int main(void)
{

	setupHardware();
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
	sei(); /*  Enable interrupts */
	GlobalInterruptEnable();
	while(1){
		//pot values are inverted b/c P0W and P0B are shorted
		if (usb_connected) {
			// Do stuff
			usb_rx();
			CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
			USB_USBTask();
		}
		/*
		SendPulse(0, 0);
		_delay_ms(500);
		SendPulse(1, 0);
		_delay_ms(500);
		*/
	}
	
	return -1;
}

int SendPulse(uint8_t polarity, int epmNum)
//EPMS are indexed from 0! 
{
	// Sets up MUX and delivers pulse to H-bridge controller
	if (epmNum < 0 || epmNum > 69)
		return -1;
	if (polarity) {
		/*
		Send a positive pulse to EPMx:
		1. Set LIx to 1 and HIx to 0.
		2. Set HC to 1 and LC to 0.
		3. Wait t usecs.
		4. Set LIx to 0 and HC to 0.
		*/
		Blink();
		set_mux(epmNum, 0, 0);
		set_COM(0, 0);
		_delay_ms(1); // Zero out the FETs
		set_COM(1, 0);
		set_mux(0, 0, 1);
		_delay_us(100);
		set_COM(0, 0); // Cut off pulse
		set_mux(epmNum, 0, 0);
		_delay_ms(1);
		return 1;
	} else {
		/*
		Send a negative pulse to EPMx:
		1. Set LC to 1 and HC to 0.
		2. Set LIx to 0 and HIx to 1.
		3. Wait t usecs.
		4. Set LC to 0 and HIx to 0.
		*/
		set_mux(epmNum, 0, 0);
		set_COM(0, 0); //Zero out FETs
		_delay_ms(1);
		set_mux(epmNum, 1, 0);
		set_COM(0, 1);
		_delay_us(100);
		//Turn off
		set_mux(epmNum, 0, 0);
		set_COM(0, 0);
		_delay_ms(1);
		return 1;
	}


}

/*
set_mux(x, HI, LI) sets EPMx's LI and HI values to
the values given in LI and HI (0 or 1)
*/
void set_mux(int x, uint8_t HI, uint8_t LI)
//0 <= x <= 69
//LI and HI are either 0 or 1. Does not affect
//HC and LC!!!
{
	//0 <= CD_mux_num <= 4
	uint8_t CD_mux_num = (uint8_t)(x/16);
	//0 <= mux_io_num <= 15	
	uint8_t mux_io_num = (uint8_t)(x%16);
	//Set ISL COM on L and H to 0 initially, but after writing the address
	// to PORTD.
	PORTC &= 0b00111111;
	//Write to PORTD and connect LIx and HIx to
	//ISL_COM_L and ISL_COM_H
	uint8_t ISL_addr_mask = 0b00000111;
	uint8_t CD_addr_mask = 0b00001111;
	uint8_t portd_val = (mux_io_num & CD_addr_mask) | ((CD_mux_num & ISL_addr_mask) << 4);
	PORTD = (PORTD & 0b10000000) | (portd_val & 0b01111111);

	//Write LI and HI to PORTC to set LIx and HIx
	/*
	The Hi side MOSFET is controlled by PORTC6
	and the Lo side is controlled by PORTC7.
	*/
	if (HI && LI) {
		return; //BAD!!!
	} else if ((!HI) && LI) {
		// HI is 0 and LI is 1.
		// Turn off the upper MOSFET
		// and turn on the lower MOSFET.
		// Use twice as much dead time
		// b/c the gate voltage of the HI
		// MOSFET is greater.
		bit_clear(PORTC, PORTC6);
		_delay_us(2);
		bit_set(PORTC, PORTC7);
	} else if (HI && (!LI)) {
		// HI is 1 and LI is 0
		/*
		Turn off the lower MOSFET
		while turning on the higher MOSFET
		*/
		bit_clear(PORTC, PORTC7);
		_delay_us(2);
		bit_set(PORTC, PORTC6);
	} else {
		// HI and LI are both 0
		bit_clear(PORTC, PORTC7);
		bit_clear(PORTC, PORTC6);
	}
	return;	
}

//set_COM(HC, LC) sets HC and LC to either 1 or 0 depending
//on the values of the arguments
void set_COM(uint8_t HC, uint8_t LC) 
{
	if (HC && LC) {
		// HC and LC are both 1
		return; //This is bad!
	} else if ((!HC) && LC) {
		// LC is 1, HC is 0
		//First turn off HC.
		bit_clear(PORTC, PORTC4);
		_delay_us(2);
		bit_set(PORTC, PORTC5);
		return;
	} else if ((!LC) && HC) {
		// LC is 0, HC is 1
		//First turn off LC.
		bit_clear(PORTC, PORTC5);
		_delay_us(2);
		bit_set(PORTC, PORTC4);
		return;
	} else {
		// LC and HC are both 0
		//First turn off LC.
		bit_clear(PORTC, PORTC5);
		bit_clear(PORTC, PORTC4);
		return;
	}
	return;
}

uint16_t pot_value_calc(float voltage) 
{
	float R2 = 67000.0;
	float R1 = R2/(((voltage + 2.4)/1.5) - 1.0);
	uint16_t dig_pot = (uint16_t)(255.0*R1/(5000.0));
	if (dig_pot > 255) {
		dig_pot = 255;
	} else if (dig_pot < 0) {
		dig_pot = 0;
	}
	return (255 - dig_pot);
	
}

int pot_set (float voltage) 
{
	//val is btwn 0 and 256
	uint16_t val = pot_value_calc(voltage);
	uint8_t cmd_error_mask = 0b00000010;
	uint16_t data_mask = 0b0000000111111111;
	uint16_t hi_mask = 0b1111111100000000;
	uint16_t lo_mask = ~hi_mask;
	uint16_t cmd = (((uint16_t)cmd_error_mask) << 8) | (val & data_mask);
	uint8_t cmd_hi = (uint8_t)((hi_mask & cmd) >> 8);
	uint8_t cmd_lo = (uint8_t)(lo_mask & cmd);
	PORTB &= 0b11111110;
	_delay_ms(1);
	SPI_MasterTransmit(cmd_hi);
	SPI_MasterTransmit(cmd_lo);
	PORTB |= 0b00000001;
	return 0;
}

void error(void) 
{
	//Turns on blue LED to indicate error
	PORTB = 0b01000000;
	return;
}

void Blink(void) 
{
	// Blinks blue LED with specified period
	PORTB |= 0b01000000;
	_delay_ms(300);
	PORTB &= (~0b01000000);
	return;
	//CDC_Device_SendString(&VirtualSerial_CDC_Interface, "Blink");
	
}

void setupHardware(void) 
{
	/* Disable watchdog */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	/* Disable prescaler */
	clock_prescale_set(clock_div_1);
	
	/* Hardware Initialization */
	USB_Init();	
	DDRD = 0b01111111;
	DDRC = 0b11110000;
	SPI_MasterInit();
	return;
}

void SPI_MasterInit(void) 
{
	DDRB = 0b01001111;
	SPCR = 0b01010011;
	PORTB |= 0b00000001;
}

void SPI_MasterTransmit(char cData) 
{
	SPDR = cData;
	while(!(SPSR & (1<<SPIF)))
	;
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
    usb_connected = 1;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
    usb_connected = 0;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}
