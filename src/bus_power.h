#ifndef BUS_POWER_H
#define BUS_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

bool bus_power(bool enable);
float bus_voltage_get(void);

#ifdef __cplusplus
}
#endif

#endif /* BUS_POWER_H */
