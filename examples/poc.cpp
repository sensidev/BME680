#include "mbed.h"
#include "mbed_bme680.h"

I2C i2c(I2C_SDA, I2C_SCL);  // Used inside the BME680 Mbed Lib.

Serial pc(USBTX, USBRX);

BME680 bme680(0x77 << 1);


int main() {
    pc.baud(115200);


    if (!bme680.begin()) {
        pc.printf("BME680 Begin failed \r\n");
        return 1;
    }

    while (true) {
        pc.printf("-------------------- \r\n");

        if (bme680.performReading()) {
            pc.printf("BME680 Temperature: %.2f degC \r\n", bme680.temperature);
            pc.printf("BME680 Humidity: %.2f %% \r\n", bme680.humidity);
            pc.printf("BME680 Pressure: %.2f hPa \r\n", bme680.pressure / 100.0);
            pc.printf("BME680 VOC: %0.2f KOhms \r\n", bme680.gas_resistance / 1000.0);
        }

        wait(2);
    }
}