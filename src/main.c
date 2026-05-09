#include "eeng1030_lib.h"
#include <stdint.h>

/*
    Vehicle Engine Safety System
    Output:
    - Terminal messages through Serial Monitor at 9600 baud
    - Green LED  = ENGINE RUNNING
    - Red LED    = ENGINE OFF / ENGINE FAULT
    - Yellow LED = ENGINE WARNING
    - Buzzer     = fault siren

    Inputs:
    - Button PA8
    - BMI160 accelerometer using software I2C on PB6/PB7
*/

/* ---------------- PIN DEFINITIONS ---------------- */

#define GREEN_LED_PIN   7       // PA7
#define RED_LED_PIN     11      // PA11
#define YELLOW_LED_PIN  12      // PA12
#define BUTTON_PIN      8       // PA8
#define BUZZER_PIN      4       // PB4

#define SCL_PIN         6       // PB6
#define SDA_PIN         7       // PB7

#define BMI160_ADDR     0x69

/* ---------------- TILT SETTINGS ---------------- */

#define TILT_THRESHOLD  350     // lower = more sensitive, higher = less sensitive

/* ---------------- ENGINE STATES ---------------- */

typedef enum
{
    ENGINE_RUNNING,
    ENGINE_OFF,
    ENGINE_WARNING,
    ENGINE_FAULT
} EngineState;

/* ---------------- GLOBAL BASELINE ---------------- */

static int32_t base_x = 0;
static int32_t base_y = 0;

/* ---------------- FUNCTION DECLARATIONS ---------------- */

static void GPIO_Init_All(void);
static int ButtonPressed(void);

static void AllLEDsOff(void);
static void BuzzerOff(void);
static void SirenShort(void);

static void ShowEngineRunning(int32_t x, int32_t y);
static void ShowEngineOff(void);
static void ShowEngineWarning(void);
static void ShowEngineFault(void);
static void PrintSensorValues(int32_t x, int32_t y);

static void SoftI2C_Init(void);
static void SCL_High(void);
static void SCL_Low(void);
static void SDA_High(void);
static void SDA_Low(void);
static int SDA_Read(void);
static void i2c_delay(void);

static void SoftI2C_Start(void);
static void SoftI2C_Stop(void);
static int SoftI2C_WriteByte(uint8_t data);
static uint8_t SoftI2C_ReadByte(int ack);

static int BMI160_WriteRegister(uint8_t reg, uint8_t value);
static int BMI160_ReadRegister(uint8_t reg, uint8_t *value);
static int16_t BMI160_ReadAxis(uint8_t reg_low);
static void BMI160_ReadXYZ(int32_t *x_g, int32_t *y_g, int32_t *z_g);
static int BMI160_Configure(void);
static int BMI160_CheckID(void);
static int TiltDetected(int32_t x_g, int32_t y_g);

/* ---------------- MAIN ---------------- */

int main(void)
{
    initClocks();
    initSerial(9600);
    GPIO_Init_All();
    SoftI2C_Init();

    eputs("\r\n\r\n====================================\r\n");
    eputs("Vehicle Engine Safety System\r\n");
    eputs("Final Terminal Mode\r\n");
    eputs("====================================\r\n");

    eputs("Checking BMI160 accelerometer...\r\n");

    if (BMI160_CheckID())
    {
        eputs("BMI160 detected successfully.\r\n");
    }
    else
    {
        eputs("ERROR: BMI160 not detected.\r\n");
        eputs("Check wiring: PB6=SCL, PB7=SDA, CS=3.3V, SA0=3.3V.\r\n");
        eputs("System will continue, but tilt detection will not work correctly.\r\n");
    }

    if (BMI160_Configure())
    {
        eputs("BMI160 configured successfully.\r\n");
    }
    else
    {
        eputs("WARNING: BMI160 configuration may have failed.\r\n");
    }

    delay_ms(500);

    int32_t x_g = 0;
    int32_t y_g = 0;
    int32_t z_g = 0;

    BMI160_ReadXYZ(&base_x, &base_y, &z_g);

    eputs("Calibration complete.\r\n");
    eputs("Base X=");
    eprintInt(base_x);
    eputs("  Base Y=");
    eprintInt(base_y);
    eputs("\r\n");

    EngineState state = ENGINE_RUNNING;
    EngineState previous_state = (EngineState)-1;

    while (1)
    {
        BMI160_ReadXYZ(&x_g, &y_g, &z_g);

        if (state != previous_state)
        {
            previous_state = state;

            if (state == ENGINE_RUNNING)
            {
                ShowEngineRunning(x_g, y_g);
            }
            else if (state == ENGINE_OFF)
            {
                ShowEngineOff();
            }
            else if (state == ENGINE_WARNING)
            {
                ShowEngineWarning();

                delay_ms(4000);

                state = ENGINE_FAULT;
            }
            else if (state == ENGINE_FAULT)
            {
                ShowEngineFault();
            }
        }

        if (state == ENGINE_RUNNING)
        {
            PrintSensorValues(x_g, y_g);

            if (ButtonPressed())
            {
                state = ENGINE_OFF;
            }
            else if (TiltDetected(x_g, y_g))
            {
                state = ENGINE_WARNING;
            }
        }
        else if (state == ENGINE_OFF)
        {
            if (ButtonPressed())
            {
                BMI160_ReadXYZ(&base_x, &base_y, &z_g);
                state = ENGINE_RUNNING;
            }
        }
        else if (state == ENGINE_FAULT)
        {
            SirenShort();

            if (ButtonPressed())
            {
                BuzzerOff();

                BMI160_ReadXYZ(&base_x, &base_y, &z_g);

                state = ENGINE_RUNNING;
            }
        }

        delay_ms(300);
    }
}

/* ---------------- GPIO / BUTTON / OUTPUTS ---------------- */

static void GPIO_Init_All(void)
{
    RCC->AHB2ENR |= (1 << 0);   // GPIOA clock
    RCC->AHB2ENR |= (1 << 1);   // GPIOB clock

    // LEDs: PA7, PA11, PA12
    pinMode(GPIOA, GREEN_LED_PIN, 1);
    pinMode(GPIOA, RED_LED_PIN, 1);
    pinMode(GPIOA, YELLOW_LED_PIN, 1);

    // Button: PA8 input with pull-up
    pinMode(GPIOA, BUTTON_PIN, 0);
    enablePullUp(GPIOA, BUTTON_PIN);

    // Buzzer: PB4 output
    pinMode(GPIOB, BUZZER_PIN, 1);

    AllLEDsOff();
    BuzzerOff();
}

static int ButtonPressed(void)
{
    static int last_state = 1;

    int current_state = (GPIOA->IDR >> BUTTON_PIN) & 1;

    if (last_state == 1 && current_state == 0)
    {
        delay_ms(30);

        current_state = (GPIOA->IDR >> BUTTON_PIN) & 1;

        if (current_state == 0)
        {
            last_state = 0;
            return 1;
        }
    }

    last_state = current_state;
    return 0;
}

static void AllLEDsOff(void)
{
    GPIOA->ODR &= ~(1 << GREEN_LED_PIN);
    GPIOA->ODR &= ~(1 << RED_LED_PIN);
    GPIOA->ODR &= ~(1 << YELLOW_LED_PIN);
}

static void BuzzerOff(void)
{
    GPIOB->ODR &= ~(1 << BUZZER_PIN);
}

static void SirenShort(void)
{
    for (int i = 0; i < 8000; i++)
    {
        GPIOB->ODR |= (1 << BUZZER_PIN);
        delay(900);

        GPIOB->ODR &= ~(1 << BUZZER_PIN);
        delay(600);
    }
}

/* ---------------- ENGINE OUTPUT MESSAGES ---------------- */

static void ShowEngineRunning(int32_t x, int32_t y)
{
    AllLEDsOff();
    BuzzerOff();

    GPIOA->ODR |= (1 << GREEN_LED_PIN);

    eputs("\r\nSTATE: ENGINE RUNNING\r\n");
    eputs("Message: Engine is active and stable.\r\n");
    eputs("Tilt detection is active.\r\n");

    eputs("X=");
    eprintInt(x);
    eputs("  Y=");
    eprintInt(y);
    eputs("\r\n");
}

static void ShowEngineOff(void)
{
    AllLEDsOff();
    BuzzerOff();

    GPIOA->ODR |= (1 << RED_LED_PIN);

    eputs("\r\nSTATE: ENGINE OFF\r\n");
    eputs("Message: Engine manually switched off.\r\n");
    eputs("Press button again to start engine.\r\n");
}

static void ShowEngineWarning(void)
{
    AllLEDsOff();
    BuzzerOff();

    GPIOA->ODR |= (1 << YELLOW_LED_PIN);

    eputs("\r\nSTATE: ENGINE WARNING\r\n");
    eputs("Message: Tilt/disturbance detected.\r\n");
    eputs("Warning active for 4 seconds.\r\n");
}

static void ShowEngineFault(void)
{
    AllLEDsOff();

    GPIOA->ODR |= (1 << RED_LED_PIN);

    eputs("\r\nSTATE: ENGINE FAULT\r\n");
    eputs("Message: Engine stopped due to safety fault.\r\n");
    eputs("Buzzer alarm active.\r\n");
    eputs("Press button to reset and restart engine.\r\n");
}

static void PrintSensorValues(int32_t x, int32_t y)
{
    eputs("RUNNING  X=");
    eprintInt(x);
    eputs("  Y=");
    eprintInt(y);
    eputs("  dX=");

    int32_t dx = x - base_x;
    if (dx < 0)
    {
        dx = -dx;
    }

    eprintInt(dx);
    eputs("  dY=");

    int32_t dy = y - base_y;
    if (dy < 0)
    {
        dy = -dy;
    }

    eprintInt(dy);
    eputs("\r\n");
}

/* ---------------- SOFTWARE I2C ---------------- */

static void SoftI2C_Init(void)
{
    RCC->AHB2ENR |= (1 << 1); // GPIOB clock

    /*
        Simulated open-drain:
        LOW  = output low
        HIGH = input with pull-up
    */

    GPIOB->ODR &= ~((1 << SCL_PIN) | (1 << SDA_PIN));

    GPIOB->PUPDR &= ~((3u << (SCL_PIN * 2)) | (3u << (SDA_PIN * 2)));
    GPIOB->PUPDR |=  ((1u << (SCL_PIN * 2)) | (1u << (SDA_PIN * 2)));

    SCL_High();
    SDA_High();
}

static void SCL_High(void)
{
    GPIOB->MODER &= ~(3u << (SCL_PIN * 2)); // input mode
}

static void SCL_Low(void)
{
    GPIOB->ODR &= ~(1 << SCL_PIN);

    GPIOB->MODER &= ~(3u << (SCL_PIN * 2));
    GPIOB->MODER |=  (1u << (SCL_PIN * 2)); // output mode
}

static void SDA_High(void)
{
    GPIOB->MODER &= ~(3u << (SDA_PIN * 2)); // input mode
}

static void SDA_Low(void)
{
    GPIOB->ODR &= ~(1 << SDA_PIN);

    GPIOB->MODER &= ~(3u << (SDA_PIN * 2));
    GPIOB->MODER |=  (1u << (SDA_PIN * 2)); // output mode
}

static int SDA_Read(void)
{
    return (GPIOB->IDR >> SDA_PIN) & 1;
}

static void i2c_delay(void)
{
    delay(80);
}

static void SoftI2C_Start(void)
{
    SDA_High();
    SCL_High();
    i2c_delay();

    SDA_Low();
    i2c_delay();

    SCL_Low();
    i2c_delay();
}

static void SoftI2C_Stop(void)
{
    SDA_Low();
    i2c_delay();

    SCL_High();
    i2c_delay();

    SDA_High();
    i2c_delay();
}

static int SoftI2C_WriteByte(uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            SDA_High();
        }
        else
        {
            SDA_Low();
        }

        i2c_delay();

        SCL_High();
        i2c_delay();

        SCL_Low();
        i2c_delay();

        data <<= 1;
    }

    SDA_High();
    i2c_delay();

    SCL_High();
    i2c_delay();

    int ack = SDA_Read();

    SCL_Low();
    i2c_delay();

    return ack == 0;
}

static uint8_t SoftI2C_ReadByte(int ack)
{
    uint8_t data = 0;

    SDA_High();

    for (int i = 0; i < 8; i++)
    {
        data <<= 1;

        SCL_High();
        i2c_delay();

        if (SDA_Read())
        {
            data |= 1;
        }

        SCL_Low();
        i2c_delay();
    }

    if (ack)
    {
        SDA_Low();
    }
    else
    {
        SDA_High();
    }

    i2c_delay();

    SCL_High();
    i2c_delay();

    SCL_Low();
    i2c_delay();

    SDA_High();

    return data;
}

/* ---------------- BMI160 SENSOR ---------------- */

static int BMI160_WriteRegister(uint8_t reg, uint8_t value)
{
    SoftI2C_Start();

    if (!SoftI2C_WriteByte((BMI160_ADDR << 1) | 0))
    {
        SoftI2C_Stop();
        return 0;
    }

    if (!SoftI2C_WriteByte(reg))
    {
        SoftI2C_Stop();
        return 0;
    }

    if (!SoftI2C_WriteByte(value))
    {
        SoftI2C_Stop();
        return 0;
    }

    SoftI2C_Stop();
    return 1;
}

static int BMI160_ReadRegister(uint8_t reg, uint8_t *value)
{
    SoftI2C_Start();

    if (!SoftI2C_WriteByte((BMI160_ADDR << 1) | 0))
    {
        SoftI2C_Stop();
        return 0;
    }

    if (!SoftI2C_WriteByte(reg))
    {
        SoftI2C_Stop();
        return 0;
    }

    SoftI2C_Start();

    if (!SoftI2C_WriteByte((BMI160_ADDR << 1) | 1))
    {
        SoftI2C_Stop();
        return 0;
    }

    *value = SoftI2C_ReadByte(0);

    SoftI2C_Stop();
    return 1;
}

static int16_t BMI160_ReadAxis(uint8_t reg_low)
{
    uint8_t low = 0;
    uint8_t high = 0;

    BMI160_ReadRegister(reg_low, &low);
    BMI160_ReadRegister(reg_low + 1, &high);

    return (int16_t)((high << 8) | low);
}

static void BMI160_ReadXYZ(int32_t *x_g, int32_t *y_g, int32_t *z_g)
{
    int16_t raw_x = BMI160_ReadAxis(0x12);
    int16_t raw_y = BMI160_ReadAxis(0x14);
    int16_t raw_z = BMI160_ReadAxis(0x16);

    *x_g = ((int32_t)raw_x * 981) / 16384;
    *y_g = ((int32_t)raw_y * 981) / 16384;
    *z_g = ((int32_t)raw_z * 981) / 16384;
}

static int BMI160_CheckID(void)
{
    uint8_t id = 0;

    if (!BMI160_ReadRegister(0x00, &id))
    {
        eputs("Chip ID read failed.\r\n");
        return 0;
    }

    eputs("Chip ID decimal = ");
    eprintInt(id);
    eputs("\r\n");

    if (id == 209)
    {
        return 1;
    }

    return 0;
}

static int BMI160_Configure(void)
{
    uint8_t id = 0;

    eputs("Sending BMI160 soft reset...\r\n");

    if (!BMI160_WriteRegister(0x7E, 0xB6))
    {
        return 0;
    }

    delay_ms(200);

    if (BMI160_ReadRegister(0x00, &id))
    {
        eputs("Chip ID after reset = ");
        eprintInt(id);
        eputs("\r\n");
    }

    eputs("Setting ACC_CONF...\r\n");
    BMI160_WriteRegister(0x40, 0x28);
    delay_ms(50);

    eputs("Setting ACC_RANGE...\r\n");
    BMI160_WriteRegister(0x41, 0x03);
    delay_ms(50);

    eputs("Setting accelerometer normal mode...\r\n");
    BMI160_WriteRegister(0x7E, 0x11);
    delay_ms(1000);

    return 1;
}

static int TiltDetected(int32_t x_g, int32_t y_g)
{
    int32_t dx = x_g - base_x;
    int32_t dy = y_g - base_y;

    if (dx < 0)
    {
        dx = -dx;
    }

    if (dy < 0)
    {
        dy = -dy;
    }

    if (dx > TILT_THRESHOLD || dy > TILT_THRESHOLD)
    {
        return 1;
    }

    return 0;
}
