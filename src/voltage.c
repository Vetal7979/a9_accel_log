#include "a9.h"

#define MIN_ADC_VALUE 1300
#define MAX_ADC_VALUE 1800

char GetVoltage() {
    char battery = 0;

    if (settings->adc) {
        uint16_t value = 0, mV = 0;
        if (ADC_Read(ADC_CHANNEL_0, &value, &mV)) {
            if (mV > MIN_ADC_VALUE) {
                battery = (char)((double)((double)(mV - MIN_ADC_VALUE) * 100 / (double)(MAX_ADC_VALUE - MIN_ADC_VALUE)));
                return (battery > 100 ? 100 : battery);
            }
        }
    }
    
    PM_Voltage((uint8_t *)&battery);

    return (battery > 100 ? 100 : battery);
}