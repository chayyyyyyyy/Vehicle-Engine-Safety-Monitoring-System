#include "eeng1030_lib.h"

void initClocks(void)
{
    RCC->CR &= ~(1 << 24);

    RCC->PLLCFGR = (1 << 25) |
                   (1 << 24) |
                   (1 << 22) |
                   (1 << 21) |
                   (1 << 17) |
                   (80 << 8) |
                   (1 << 0);

    RCC->CR |= (1 << 24);

    while ((RCC->CR & (1 << 25)) == 0)
    {
    }

    FLASH->ACR &= ~((1 << 2) | (1 << 1) | (1 << 0));
    FLASH->ACR |= (1 << 2);

    RCC->CFGR |= (1 << 1) | (1 << 0);
}

void delay(volatile uint32_t dly)
{
    while (dly--)
    {
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay(8000);
    }
}

void pinMode(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t Mode)
{
    Port->MODER &= ~(3u << (BitNumber * 2));
    Port->MODER |=  (Mode << (BitNumber * 2));
}

void selectAlternateFunction(GPIO_TypeDef *Port, uint32_t BitNumber, uint32_t AF)
{
    if (BitNumber < 8)
    {
        Port->AFR[0] &= ~(0x0Fu << (4 * BitNumber));
        Port->AFR[0] |=  (AF << (4 * BitNumber));
    }
    else
    {
        BitNumber -= 8;
        Port->AFR[1] &= ~(0x0Fu << (4 * BitNumber));
        Port->AFR[1] |=  (AF << (4 * BitNumber));
    }
}

void enablePullUp(GPIO_TypeDef *Port, uint32_t BitNumber)
{
    Port->PUPDR &= ~(3u << (BitNumber * 2));
    Port->PUPDR |=  (1u << (BitNumber * 2));
}

void initSerial(uint32_t baudrate)
{
    RCC->AHB2ENR |= (1 << 0);      // GPIOA clock
    RCC->APB1ENR1 |= (1 << 17);    // USART2 clock

    // PA2 = USART2_TX
    pinMode(GPIOA, 2, 2);
    selectAlternateFunction(GPIOA, 2, 7);

    USART2->BRR = 80000000 / baudrate;
    USART2->CR1 = (1 << 3) |      // transmitter enable
                  (1 << 2) |      // receiver enable
                  (1 << 0);       // USART enable
}

void eputc(char c)
{
    while ((USART2->ISR & (1 << 7)) == 0)
    {
    }

    USART2->TDR = c;
}

void eputs(const char *s)
{
    while (*s)
    {
        eputc(*s++);
    }
}

void eprintInt(int32_t value)
{
    char buffer[16];
    int index = 0;
    int negative = 0;

    if (value == 0)
    {
        eputc('0');
        return;
    }

    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    while (value > 0 && index < 15)
    {
        buffer[index++] = (value % 10) + '0';
        value = value / 10;
    }

    if (negative)
    {
        eputc('-');
    }

    while (index > 0)
    {
        eputc(buffer[--index]);
    }
}
