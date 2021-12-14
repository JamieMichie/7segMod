/*
 * 7segClock.c
 *
 * Created: 23/11/2021 19:03:24
 * Author : Jamie
 */ 
#define F_CPU 16000000UL

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define DECODE_ADDRESS 0x01// 0x07, all 8 on
#define INTENSITY_ADDRESS 0x02 // 0x07 to start, half intensity 02
#define SCANLIMIT_ADDRESS 0x03 // 0x07, all 8 on
#define CONFIG_ADDRESS 0x04
#define DISPLAYTEST_ADDRESS 0x07
#define DIGIT0_ADDRESS 0x20 // write Plane 0 20
#define DIGIT1_ADDRESS 0x21 // write Plane 0 21
#define DIGIT2_ADDRESS 0x22 // write Plane 0
#define DIGIT3_ADDRESS 0x23 // write Plane 0
#define DIGIT4_ADDRESS 0x24 // write Plane 0
#define DIGIT5_ADDRESS 0x25 // write Plane 0
#define DIGIT6_ADDRESS 0x26 // write Plane 0
#define DIGIT7_ADDRESS 0x27 // write Plane 0

#define BAUD 9600
#define PRESCALER (((F_CPU / (BAUD * 16UL))) - 1)

#define TRUE 1
#define FALSE 0
#define CHAR_NEWLINE '\n'
#define CHAR_RETURN '\r'
#define RETURN_NEWLINE "\r\n"

char cmd[3] = {0};
uint8_t cmdCount = 0;


// The inputted commands are never going to be
// more than 8 chars long.
// volatile so the ISR can alter them
volatile unsigned char data_in[8];
char command_in[8];

volatile unsigned char data_count;
volatile unsigned char command_ready;

uint16_t sensitivity = 0; 

void SPI_MasterInit(void);
void SPI_Transmit(char cData);
void Display_Init(void);
void toggleCS(void);
void toggleCS1(void);
void toggleCS2(void);
void Display_brightness(uint8_t uval);

void Uart_Init(void);
void usart_putc(char cdata);
void usart_puts(const char *send);
void usart_ok(void);
void command_copy(void);
void process_command(void);
void print_val(char *id, int value);
unsigned long parse_assignment(void);
unsigned long parse_query(void);

void SPI_MasterInit(void){
	//                       SCK - D15       MOSI - D16
	DDRB = (1 << PORTB0) | (1 << PORTB1) | (1 << PORTB2);
	//       Green LED          CS1	D2			CS2 D3
	DDRD = (1 << PORTD5) | (1 << PORTD1) | (1 << PORTD0); 
	// SPI enable   Master Mode   1MHz
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) ;
	
	PORTD = (1 << PORTD5);
}
void SPI_Transmit(char cData){
	SPDR = cData;
	while(!(SPSR & (1<< SPIF)))
	;
}
void Display_Init(void){
	//set CS pins high
	PORTD |= (1 << PORTD1) | (1 << PORTD0);
	
	toggleCS();
	
	SPI_Transmit(CONFIG_ADDRESS);
	
	SPI_Transmit(0x05);
	
	
	toggleCS();
	_delay_ms(10);
	toggleCS();
	
	SPI_Transmit(INTENSITY_ADDRESS);
	SPI_Transmit(0x03);
	
	toggleCS();
	_delay_ms(10);
	toggleCS();
	
	SPI_Transmit(SCANLIMIT_ADDRESS);
	SPI_Transmit(0x0F);
	
	toggleCS();
	_delay_ms(10);
	toggleCS();
	
	SPI_Transmit(DECODE_ADDRESS);
	SPI_Transmit(0xFF);
	
	toggleCS();
	_delay_ms(10);
	toggleCS();
	
	SPI_Transmit(DISPLAYTEST_ADDRESS);
	SPI_Transmit(0x00);
	
	toggleCS();
	
	
}
void toggleCS(void){
	//	     CS1	D2			CS2 D3
	PORTD ^= (1 << PORTD1) | (1 << PORTD0);
	
}
void toggleCS1(void){
	PORTD ^= (1 << PORTD1);
}
void toggleCS2(void){
	PORTD ^= (1 << PORTD0);
	
}
void Display_brightness(uint8_t uval){
	if(uval >= 0 && uval <= 14){
		toggleCS();
		SPI_Transmit(INTENSITY_ADDRESS);
		SPI_Transmit(uval);
		toggleCS();
	}else{
		usart_puts("Invalid value setting to 1 \n\r");
		toggleCS();
		SPI_Transmit(INTENSITY_ADDRESS);
		SPI_Transmit(0x01);
		toggleCS();
	}
	
}

void Uart_Init(void){
	//Function to configure uart port 1 for asynchronous serial communication
	// PD2 = RXD1 PD3 = TXD1
	
	//enable receiver and transmitter
	UCSR1B |= (1 << RXEN1) | (1 << TXEN1);
	//Set frame format 8bit, 1 stop bits
	UCSR1C = (1 << UCSZ10) | (1 << UCSZ11);
	UCSR1B |=  (1 << RXCIE1);
	UBRR1H = (unsigned char)(PRESCALER >> 8);
	UBRR1L = (unsigned char)(PRESCALER);
	sei();
}
void usart_putc(char cdata){
	/* Wait for empty transmit buffer */
	while(!(UCSR1A & (1<<UDRE1)))
	;
	/*Put data into the buffer and send */
	UDR1 = cdata;
}
void usart_puts(const char *send){
	while(*send){
		usart_putc(*send++);
	}
}
void usart_ok(void){
	usart_puts("OK\r\n");
}
ISR(USART1_RX_vect){
	data_in[data_count] = UDR1;
	usart_putc(UDR1);
	if(data_in[data_count] == '\n' || data_in[data_count] == '\r'){
		command_ready = TRUE;
		data_count = 0;
	}else{
		data_count++;
	}
}

void command_copy(void){
	ATOMIC_BLOCK(ATOMIC_FORCEON){
		memcpy(command_in, data_in, 8);
		//Clear data_in for next console command
		memset(data_in[0], 0, 8);
	}
}
void process_command(void){
	char *pch;
	char cmdValue[16];
	
	switch(command_in[0])
	{
		case 'B':
			if (command_in[1] == '='){
				sensitivity = parse_assignment();
				Display_brightness(sensitivity);
			
			} else if (command_in[1] == '?'){
				usart_puts("Brightness Query \n\r");
				parse_query();
				print_val((char)command_in[0], sensitivity);
			}else{
				usart_puts("Invalid command\n\r");
			}
		break;
		
		case 'C':
		usart_puts("C command has not been implemented. \n\r");
		break;
		
		default:
		usart_puts("Invalid command\n\r");
		break;
	}
}
unsigned long parse_assignment(void){
	char *pch;
	char cmdValue[16];
	 // Find the position the equals sign is
	 // in the string, keep a pointer to it
	 pch = strchr(command_in, '=');
	 // Copy everything after that point into
	 // the buffer variable
	 strcpy(cmdValue, pch+1);
	 // Now turn this value into an integer and
	 // return it to the caller.
	 return atoi(cmdValue);
}
unsigned long parse_query(void){
	char *pch;
	char cmdValue[16];
	// Find the position the equals sign is
	// in the string, keep a pointer to it
	pch = strchr(command_in, '?');
	// Copy everything after that point into
	// the buffer variable
	strcpy(cmdValue, pch+1);
	// Now turn this value into an integer and
	// return it to the caller.
	return atoi(cmdValue);
}
void print_val(char *id, int value){
	char buffer[8];
	itoa(value, buffer, 10);
	usart_putc((uint8_t *)id);
	usart_putc((char *)":");
	usart_puts(buffer);
	usart_puts(RETURN_NEWLINE);
}
int main(void)
{
    /* Replace with your application code */
	SPI_MasterInit();
	Display_Init();
	Uart_Init();
	
	usart_puts("Hello, World!\n\r");
	usart_ok();
    while (1) 
    {
		if (command_ready == TRUE) {
			// Here is where we will copy
			command_copy();
			// and parse the command.
			process_command();
			command_ready = FALSE;
		}
		
    }
}

