#ifndef USART0_H
#define USART0_H



#include <avr/io.h>

void USART0_init(uint16_t ubrr_value);
void USART0_transmit_char(char data);
void USART0_send_string(const char *str);

#endif