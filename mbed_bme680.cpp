#include "mbed_bme680.h"

BME680::BME680() {
    BME680(BME680_DEFAULT_ADDRESS);
}

BME680::BME680(uint8_t adr) {
    _filterEnabled = _tempEnabled = _humEnabled = _presEnabled = _gasEnabled = false;
    _adr = adr;
}

bool BME680::begin() {
    gas_sensor.dev_id = _adr;
    gas_sensor.intf = BME680_I2C_INTF;
    gas_sensor.read = &BME680::i2c_read;
    gas_sensor.write = &BME680::i2c_write;
    gas_sensor.delay_ms = BME680::delay_msec;

    int8_t result = BME680_OK;
    result = bme680_init(&gas_sensor);

    if (result != BME680_OK)
        return false;

    setHumidityOversampling(BME680_OS_2X);
    setPressureOversampling(BME680_OS_4X);
    setTemperatureOversampling(BME680_OS_8X);
    setIIRFilterSize(BME680_FILTER_SIZE_3);
    setGasHeater(320, 150); // 320*C for 150 ms

    return true;
}

/**
 * Performs a full reading of all 4 sensors in the BME680.
 * Assigns the internal BME680#temperature, BME680#pressure, BME680#humidity and BME680#gas_resistance member variables
 * @return True on success, False on failure
 */
bool BME680::performReading(void) {
    uint8_t set_required_settings = 0;
    int8_t result;

    /* Select the power mode */
    /* Must be set before writing the sensor configuration */
    gas_sensor.power_mode = BME680_FORCED_MODE;

    /* Set the required sensor settings needed */
    if (_tempEnabled)
        set_required_settings |= BME680_OST_SEL;
    if (_humEnabled)
        set_required_settings |= BME680_OSH_SEL;
    if (_presEnabled)
        set_required_settings |= BME680_OSP_SEL;
    if (_filterEnabled)
        set_required_settings |= BME680_FILTER_SEL;
    if (_gasEnabled)
        set_required_settings |= BME680_GAS_SENSOR_SEL;

    /* Set the desired sensor configuration */
    result = bme680_set_sensor_settings(set_required_settings, &gas_sensor);
    log("Set settings, result %d \r\n", result);
    if (result != BME680_OK)
        return false;

    /* Set the power mode */
    result = bme680_set_sensor_mode(&gas_sensor);
    log("Set power mode, result %d \r\n", result);
    if (result != BME680_OK)
        return false;

    /* Get the total measurement duration so as to sleep or wait till the
     * measurement is complete */
    uint16_t meas_period;
    bme680_get_profile_dur(&meas_period, &gas_sensor);

    /* Delay till the measurement is ready */
    delay_msec(meas_period * 2);

    result = bme680_get_sensor_data(&data, &gas_sensor);
    log("Get sensor data, result %d \r\n", result);
    if (result != BME680_OK)
        return false;

    return true;
}


int16_t BME680::getRawTemperature() {
    return data.temperature;
}

uint32_t BME680::getRawPressure() {
    return data.pressure;
}

uint32_t BME680::getRawHumidity() {
    return data.humidity;
}

uint32_t BME680::getRawGasResistance() {
    return data.gas_resistance;
}

/**
 * Get last read temperature
 * @return Temperature in degree celsius
 */
float BME680::getTemperature() {
    float temperature = NAN;

    if (_tempEnabled) {
        temperature = data.temperature / 100.0;
        log("Temperature Raw Data %d \r\n", temperature);
    }

    return temperature;
}

/**
 * Get last read humidity
 * @return Humidity in % relative humidity
 */
float BME680::getHumidity() {
    float humidity = NAN;

    if (_humEnabled) {
        humidity = data.humidity / 1000.0;
        log("Humidity Raw Data %d \r\n", humidity);
    }

    return humidity;

}


/**
 * Get last read pressure
 * @return Pressure in Pascal
 */
float BME680::getPressure() {
    float pressure = NAN;

    if (_presEnabled) {
        pressure = data.pressure;
        log("Pressure Raw Data %d \r\n", pressure);
    }

    return pressure;
}

/**
 * Get last read gas resistance
 * @return Gas resistance in Ohms
 */
float BME680::getGasResistance() {
    float gas_resistance = 0;

    /* Avoid using measurements from an unstable heating setup */
    if (_gasEnabled) {
        if (data.status & BME680_HEAT_STAB_MSK) {
            gas_resistance = data.gas_resistance;
            log("Gas Resistance Raw Data %d \r\n", gas_resistance);
        } else {
            log("Gas reading unstable \r\n");
        }
    }

    return gas_resistance;
}

/**
 * Enable and configure gas reading + heater
 * @param heaterTemp Desired temperature in degrees Centigrade
 * @param heaterTime Time to keep heater on in milliseconds
 * @return True on success, False on failure
 */
bool BME680::setGasHeater(uint16_t heaterTemp, uint16_t heaterTime) {
    gas_sensor.gas_sett.heatr_temp = heaterTemp;
    gas_sensor.gas_sett.heatr_dur = heaterTime;

    if ((heaterTemp == 0) || (heaterTime == 0)) {
        // disabled!
        gas_sensor.gas_sett.run_gas = BME680_DISABLE_GAS_MEAS;
        _gasEnabled = false;
    } else {
        gas_sensor.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
        _gasEnabled = true;
    }
    return true;
}

/**
 * Setter for Temperature oversampling
 * @param oversample Oversampling setting, can be BME680_OS_NONE (turn off Temperature reading),
 * BME680_OS_1X, BME680_OS_2X, BME680_OS_4X, BME680_OS_8X or BME680_OS_16X
 * @return True on success, False on failure
 */
bool BME680::setTemperatureOversampling(uint8_t oversample) {
    if (oversample > BME680_OS_16X) return false;

    gas_sensor.tph_sett.os_temp = oversample;

    if (oversample == BME680_OS_NONE)
        _tempEnabled = false;
    else
        _tempEnabled = true;

    return true;
}

/**
 * Setter for Humidity oversampling
 * @param oversample Oversampling setting, can be BME680_OS_NONE (turn off Humidity reading),
 * BME680_OS_1X, BME680_OS_2X, BME680_OS_4X, BME680_OS_8X or BME680_OS_16X
 * @return True on success, False on failure
 */
bool BME680::setHumidityOversampling(uint8_t oversample) {
    if (oversample > BME680_OS_16X) return false;

    gas_sensor.tph_sett.os_hum = oversample;

    if (oversample == BME680_OS_NONE)
        _humEnabled = false;
    else
        _humEnabled = true;

    return true;
}

/**
 * Setter for Pressure oversampling
 * @param oversample Oversampling setting, can be BME680_OS_NONE (turn off Humidity reading),
 * BME680_OS_1X, BME680_OS_2X, BME680_OS_4X, BME680_OS_8X or BME680_OS_16X
 * @return True on success, False on failure
 */
bool BME680::setPressureOversampling(uint8_t oversample) {
    if (oversample > BME680_OS_16X) return false;

    gas_sensor.tph_sett.os_pres = oversample;

    if (oversample == BME680_OS_NONE)
        _presEnabled = false;
    else
        _presEnabled = true;

    return true;
}

/**
 * Setter for IIR filter.
 * @param filter_seize Size of the filter (in samples).
 * Can be BME680_FILTER_SIZE_0 (no filtering), BME680_FILTER_SIZE_1, BME680_FILTER_SIZE_3, BME680_FILTER_SIZE_7,
 * BME680_FILTER_SIZE_15, BME680_FILTER_SIZE_31, BME680_FILTER_SIZE_63, BME680_FILTER_SIZE_127
 * @return True on success, False on failure
 */
bool BME680::setIIRFilterSize(uint8_t filter_seize) {
    if (filter_seize > BME680_FILTER_SIZE_127) return false;

    gas_sensor.tph_sett.filter = filter_seize;

    if (filter_seize == BME680_FILTER_SIZE_0)
        _filterEnabled = false;
    else
        _filterEnabled = true;

    return true;
}


/**
 * Reads 8 bit values over I2C
 * @param dev_id Device ID (8 bits I2C address)
 * @param reg_addr Register address to read from
 * @param reg_data Read data buffer
 * @param len Number of bytes to read
 * @return 0 on success, non-zero for failure
 */
int8_t BME680::i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len) {
    int8_t result;
    char data[1];

    data[0] = (char) reg_addr;

    log("[0x%X] I2C $%X => ", dev_id >> 1, data[0]);

    result = i2c.write(dev_id, data, 1);
    log("[W: %d] ", result);

    result = i2c.read(dev_id, (char *) reg_data, len);

    for (uint8_t i = 0; i < len; i++) log("0x%X ", reg_data[i]);

    log("[R: %d, L: %d] \r\n", result, len);

    return result;
}

/**
 * Writes 8 bit values over I2C
 * @param dev_id Device ID (8 bits I2C address)
 * @param reg_addr Register address to write to
 * @param reg_data Write data buffer
 * @param len Number of bytes to write
 * @return 0 on success, non-zero for failure
 */
int8_t BME680::i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len) {
    int8_t result;
    char data[len + 1];

    data[0] = (char) reg_addr;

    for (uint8_t i = 1; i < len + 1; i++) {
        data[i] = (char) reg_data[i - 1];
    }

    log("[0x%X] I2C $%X <= ", dev_id >> 1, data[0]);

    result = i2c.write(dev_id, data, len + 1);

    for (uint8_t i = 1; i < len + 1; i++) log("0x%X ", data[i]);

    log("[W: %d, L: %d] \r\n", result, len);

    return result;
}

void BME680::delay_msec(uint32_t ms) {
    log(" * wait %d ms ... \r\n", ms);
    wait_ms(ms);
}

void BME680::log(const char *format, ...) {
#ifdef BME680_DEBUG_MODE
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
#endif
}