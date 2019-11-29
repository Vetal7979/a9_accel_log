#include "a9.h"

/*
static GPIO_config_t sdPWR = { // SD-CARD VCC
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN6,
    .defaultLevel = GPIO_LEVEL_LOW
};

void sd_init() {
#ifdef NEW_PLATA
    GPIO_Init(sdPWR);
#endif
}

void sd_reopen() {
#ifdef NEW_PLATA
    GPIO_SetLevel(sdPWR, GPIO_LEVEL_LOW);
    OS_Sleep(100);
    GPIO_SetLevel(sdPWR, GPIO_LEVEL_HIGH);
#endif
}
*/