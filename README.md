# Vehicle Engine Safety Monitoring System

## Project Overview

This project implements a Vehicle Engine Safety Monitoring System using the STM32 NUCLEO-L432KC development board and a BMI160 accelerometer. The system detects major tilt or disturbance events and responds using LEDs, a buzzer alarm, push-button control, and UART serial monitor messages.

The project was developed using PlatformIO and the STM32Cube framework. The final implementation uses software I2C on PB6 and PB7 to communicate with the BMI160 accelerometer.

## Hardware Used

- STM32 NUCLEO-L432KC
- BMI160 accelerometer module
- Green LED
- Red LED
- Yellow LED
- Push button
- Buzzer
- 220 ohm resistors
- Breadboard and jumper wires

## Pin Configuration

| Function | STM32 Pin / Connection |
|---|---|
| Green LED | PA7 |
| Red LED | PA11 |
| Yellow LED | PA12 |
| Push button | PA8 to GND |
| Buzzer | PB4 to GND |
| BMI160 SCL | PB6 |
| BMI160 SDA / SDS | PB7 |
| BMI160 VIN / 3V3 | 3.3 V |
| BMI160 GND | GND |
| BMI160 CS | 3.3 V |
| BMI160 SA0 | 3.3 V |
| Serial Monitor | USB Virtual COM Port, 9600 baud |

## System Behaviour

The system operates using four engine states:

| State | Description |
|---|---|
| ENGINE_RUNNING | Green LED ON. Sensor values are printed to the serial monitor. |
| ENGINE_OFF | Red LED ON. Engine is manually switched off using the push button. |
| ENGINE_WARNING | Yellow LED ON for 4 seconds after tilt/disturbance detection. |
| ENGINE_FAULT | Red LED ON and buzzer alarm active until reset using the push button. |

## Final Threshold Setting

The final implementation uses the following tilt threshold:

```c
#define TILT_THRESHOLD 350
