#include "a9.h"

GPIO_config_t gpioLedBlue = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN27,
    .defaultLevel = GPIO_LEVEL_HIGH
};
GPIO_LEVEL ledBlueLevel = GPIO_LEVEL_HIGH;

/*GPIO_config_t gpioLedBlue2 = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN28,
    .defaultLevel = GPIO_LEVEL_HIGH
};
GPIO_LEVEL ledBlueLevel2 = GPIO_LEVEL_LOW;*/

void led_init() {
    //char battery = 0;
    //PM_Voltage((uint8_t *)&battery);

    /*if (GetVoltage() < 50) {
        gpioLedBlue2.defaultLevel = GPIO_LEVEL_LOW;
    }*/

    GPIO_Init(gpioLedBlue);
    // GPIO_Init(gpioLedBlue2);

    // OS_Sleep(2000);

    // GPIO_SetLevel(gpioLedBlue, GPIO_LEVEL_LOW);
    // GPIO_SetLevel(gpioLedBlue2, GPIO_LEVEL_LOW);
}

void led_blink() {
    //ledBlueLevel = (ledBlueLevel == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
    //GPIO_SetLevel(gpioLedBlue, ledBlueLevel);
}

void led_off() {
    //ledBlueLevel = GPIO_LEVEL_LOW;
    //GPIO_SetLevel(gpioLedBlue, ledBlueLevel);
}