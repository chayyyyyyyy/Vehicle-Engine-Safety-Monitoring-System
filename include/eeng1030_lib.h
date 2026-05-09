#ifndef EENG1030_LIB_H
#define EENG1030_LIB_H

#include <stdint.h>
#include <stm32l432xx.h>

void initClocks(void);
void delay(volatile uint32_t dly);
void delay_ms(uint32_t ms);

void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode);
void selectAlternateFunction(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t AF);
void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber);

void initSerial(uint32_t baudrate);
void eputc(char c);
void eputs(const char *s);
void eprintInt(int32_t value);

#endif
