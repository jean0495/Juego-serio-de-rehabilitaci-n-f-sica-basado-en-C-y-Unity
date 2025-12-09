#include "uu.h"


void USART0_init(uint16_t ubrr_value) {
	// Configurar el baud rate
	UBRR0H = (uint8_t)(ubrr_value >> 8);
	UBRR0L = (uint8_t)ubrr_value;

	// Habilitar transmisor y receptor
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);

	// 8 bits de datos, sin paridad, 1 bit de parada
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // 8 bits
	
}

void USART0_transmit_char(char data) {
	while (!(UCSR0A & (1 << UDRE0)));  // Esperar a que el buffer esté libre
	UDR0 = data;
}

void USART0_send_string(const char *str) {
	while (*str) {
		USART0_transmit_char(*str++);
	}
}