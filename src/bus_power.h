#ifndef BUS_POWER_H
#define BUS_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

float bus_voltage_get(void);
void bus_voltage_adc_conversion(void);
bool bus_power(bool enable);
void bus_power_toggle(void);
void bus_power_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BUS_POWER_H */
