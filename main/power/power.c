#include "TPS546.h"
#include "INA260.h"
#include "DS4432U.h"

#include "power.h"
#include "vcore.h"
#include "nvs_config.h"

#define GPIO_ASIC_ENABLE CONFIG_GPIO_ASIC_ENABLE

#define SUPRA_POWER_OFFSET 5 //Watts
#define GAMMA_POWER_OFFSET 5 //Watts
#define GAMMATURBO_POWER_OFFSET 5 //Watts

// max power settings
#define BITFORGE_NANO_MAX_POWER 60 //Watts

// nominal voltage settings
#define NOMINAL_VOLTAGE_5 5 //volts
#define NOMINAL_VOLTAGE_12 12//volts

// Calibration multiplier is cached and only re-read from NVS after a settings write.
static uint16_t power_cal_milli = POWER_CAL_DEFAULT_MILLI;
static uint32_t power_cal_generation = UINT32_MAX;

uint16_t Power_get_calibration_milli(void) {
    uint32_t generation = nvs_config_get_generation();
    if (generation != power_cal_generation) {
        uint16_t value = nvs_config_get_u16(NVS_CONFIG_POWER_CAL, POWER_CAL_DEFAULT_MILLI);
        if (value < POWER_CAL_MIN_MILLI || value > POWER_CAL_MAX_MILLI) {
            value = POWER_CAL_DEFAULT_MILLI;
        }
        power_cal_milli = value;
        power_cal_generation = generation;
    }
    return power_cal_milli;
}

static float power_calibration(void) {
    return Power_get_calibration_milli() / 1000.0f;
}

esp_err_t Power_disable(GlobalState * GLOBAL_STATE) {

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
            // Turn off core voltage
            VCORE_set_voltage(0.0, GLOBAL_STATE);
            break;
        default:
    }
    return ESP_OK;

}

float Power_get_max_settings(GlobalState * GLOBAL_STATE) {

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
            return BITFORGE_NANO_MAX_POWER;
        default:
            return BITFORGE_NANO_MAX_POWER;
    }
}

float Power_get_current(GlobalState * GLOBAL_STATE) {
    float current = 0.0;

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
            current = INA260_read_current() * power_calibration();
            break;
        default:
    }

    return current;
}

float Power_get_power(GlobalState * GLOBAL_STATE) {
    float power = 0.0;

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
            power = INA260_read_power() / 1000.0 * power_calibration();
            break;
        default:
    }

    return power;
}


float Power_get_input_voltage(GlobalState * GLOBAL_STATE) {

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
            return INA260_read_voltage();
            break;
        default:
    }

    return 0.0;
}

int Power_get_nominal_voltage(GlobalState * GLOBAL_STATE) {
    switch (GLOBAL_STATE->device_model)
    {
        case BITFORGE_NANO:
            return NOMINAL_VOLTAGE_12;
        default:
            return NOMINAL_VOLTAGE_12;
    }
}

float Power_get_vreg_temp(GlobalState * GLOBAL_STATE) {

    switch (GLOBAL_STATE->device_model) {
        case BITFORGE_NANO:
                return TPS546_get_temperature();
            break;
        default:
    }

    return 0.0;
}
