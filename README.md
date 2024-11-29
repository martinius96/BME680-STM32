# BME680 /w STM32
* implementation for BME680 support for STM32 microcontroller
* deprecated Driver by Bosch Sensortec at 2022, not recommend for new designs, use BME68x_SensorAPI instead --> https://github.com/boschsensortec/BME68x_SensorAPI
* this implementation is compatible with all STM32 boards, devkits
![image](https://github.com/martinius96/BME680-STM32/assets/14253034/dae1febb-091d-4d9e-991d-4de60b503084)
# Buses, logic
* LPUART1 and I2C3 is initialized (I don't provide .ioc file, so do your own)
* Firmware will perform I2C scan and will print all detected addresses on LPUART1
* will try to init BME680 sensor via address 0x76 and 0x77 for 5 times each
* then will periodically return informations about temperature, humidity, Gas resistance, Pressure
* FORCED mode enabled (no continous measurement, delays, less precise) 
![image](https://github.com/martinius96/BME680-STM32/assets/14253034/b916f6ec-6424-42a6-b62b-265f0d6046f5)
![image](https://github.com/martinius96/BME680-STM32/assets/14253034/8c240971-e8ee-469e-b3c1-16a718132c60)
