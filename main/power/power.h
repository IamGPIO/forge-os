#ifndef POWER_H
#define POWER_H

#include <esp_err.h>
#include "global_state.h"


esp_err_t Power_disable(GlobalState * GLOBAL_STATE);

float Power_get_current(GlobalState * GLOBAL_STATE);
float Power_get_power(GlobalState * GLOBAL_STATE);
float Power_get_input_voltage(GlobalState * GLOBAL_STATE);
float Power_get_vreg_temp(GlobalState * GLOBAL_STATE);
float Power_get_max_settings(GlobalState * GLOBAL_STATE);
int Power_get_nominal_voltage(GlobalState * GLOBAL_STATE);

// Per-unit calibration multiplier for the INA260 power/current readings, stored in NVS as thousandths
// (1000 = 1.000 = no correction). Lets a unit whose sensor reads high or low be trimmed against a reference meter.
#define POWER_CAL_DEFAULT_MILLI 1000
#define POWER_CAL_MIN_MILLI 500
#define POWER_CAL_MAX_MILLI 1500
uint16_t Power_get_calibration_milli(void);

#endif // POWER_H
