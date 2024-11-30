//BME680 Driver from Bosch Sensortec (valid until end of 2022, then deprecated)
//For new designs use new official Bosch Driver: https://github.com/boschsensortec/BME68x_SensorAPI

#include "main.h"
#include "bme680.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c3;
UART_HandleTypeDef hlpuart1;

char uart_buf[100];  // Declare uart_buf to hold the UART output

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_I2C3_Init(void);
static void MX_LPUART1_UART_Init(void);
void I2C_Scan(void);
// Function prototypes
int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len);
int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len);
void user_delay_ms(uint32_t period);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    PeriphCommonClock_Config();

    MX_I2C3_Init();
    MX_LPUART1_UART_Init();

    HAL_Delay(10000); // Delay for 1 second
    I2C_Scan();
    char init_msg[] = "Initializing BME680...\r\n";
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)init_msg, sizeof(init_msg) - 1, HAL_MAX_DELAY);

    struct bme680_dev gas_sensor;
    gas_sensor.dev_id = BME680_I2C_ADDR_PRIMARY; // 0x76
    gas_sensor.intf = BME680_I2C_INTF;
    gas_sensor.read = user_i2c_read;
    gas_sensor.write = user_i2c_write;
    gas_sensor.delay_ms = user_delay_ms;
    gas_sensor.amb_temp = 25;

    int8_t rslt = BME680_OK;
    int8_t rslt_secondary = BME680_OK;
    int attempt_count = 0;

    // Try initializing with primary I2C address
    while (attempt_count < 5)
    {
        rslt = bme680_init(&gas_sensor);
        char buf[50];
        sprintf(buf, "Init attempt %d: result %d\r\n", attempt_count + 1, rslt);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

        if (rslt == BME680_OK)
        {
            break;
        }
        attempt_count++;
        HAL_Delay(200);
    }

    // If primary address fails, try secondary I2C address
    if (rslt != BME680_OK)
    {
        gas_sensor.dev_id = BME680_I2C_ADDR_SECONDARY; // 0x77
        attempt_count = 0;
        while (attempt_count < 5)
        {
            rslt_secondary = bme680_init(&gas_sensor);
            char buf[50];
            sprintf(buf, "Secondary Init attempt %d: result %d\r\n", attempt_count + 1, rslt_secondary);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

            if (rslt_secondary == BME680_OK)
            {
                break;
            }
            attempt_count++;
            HAL_Delay(200);
        }
    }

    if (rslt == BME680_OK || rslt_secondary == BME680_OK)
    {
        char success_msg[] = "BME680 initialized successfully!\r\n";
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)success_msg, sizeof(success_msg) - 1, HAL_MAX_DELAY);
    }
    else
    {
        char error_msg[] = "BME680 initialization failed!\r\n";
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)error_msg, sizeof(error_msg) - 1, HAL_MAX_DELAY);
        while (1); // Halt execution
    }
    gas_sensor.tph_sett.os_hum = BME680_OS_2X; //BME680_OS_16X for maximum 16 average samplings
    gas_sensor.tph_sett.os_pres = BME680_OS_4X; //BME680_OS_16X for maximum 16 average samplings
    gas_sensor.tph_sett.os_temp = BME680_OS_8X; //BME680_OS_16X for maximum 16 average samplings
    gas_sensor.tph_sett.filter = BME680_FILTER_SIZE_3; //BME680_FILTER_SIZE_127 max IIR filter setting

    gas_sensor.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS; //BME680_DISABLE_GAS_MEAS to disabled GAS measurements (to save power)
    gas_sensor.gas_sett.heatr_temp = 320; // Target temperature in degrees Celsius
    gas_sensor.gas_sett.heatr_dur = 150; // Heating duration in milliseconds

    gas_sensor.power_mode = BME680_FORCED_MODE; //BME680_SLEEP_MODE

    // Define desired settings
    uint16_t desired_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL;

    rslt = bme680_set_sensor_settings(desired_settings, &gas_sensor);
    if (rslt != BME680_OK) {
        char error_msg[] = "Failed to set sensor settings!\r\n";
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)error_msg, sizeof(error_msg) - 1, HAL_MAX_DELAY);
    }
    uint16_t meas_period;
        bme680_get_profile_dur(&meas_period, &gas_sensor);

        struct bme680_field_data data;
        while (1)
        {
            rslt = bme680_get_sensor_data(&data, &gas_sensor);
            if (rslt == BME680_OK)
            {
                sprintf(uart_buf, "Temperature: %.1f °C, Pressure: %.1f hPa, Humidity: %.1f %%, Gas: %.1f kOhms\r\n",
                        data.temperature / 100.0, data.pressure / 100.0, data.humidity / 1000.0, data.gas_resistance / 1000.0);
                HAL_UART_Transmit(&hlpuart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
            }

            if (gas_sensor.power_mode == BME680_FORCED_MODE)
            {
                rslt = bme680_set_sensor_mode(&gas_sensor);
            }

            HAL_Delay(1000); // Delay for 1 second
        }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}


static void MX_I2C3_Init(void)
{
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10707DBC;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
}


static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

// Implement user_i2c_read, user_i2c_write, and user_delay_ms functions
int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    // Implement I2C read function
    return HAL_I2C_Mem_Read(&hi2c3, dev_id << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY);
}

int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    // Implement I2C write function
    return HAL_I2C_Mem_Write(&hi2c3, dev_id << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY);
}

void user_delay_ms(uint32_t period)
{
    HAL_Delay(period);
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */

void I2C_Scan(void)
{
    char buffer[25];
    int buffer_len;
    HAL_StatusTypeDef result;
    uint8_t i;
    buffer_len = sprintf(buffer, "Scanning I2C bus:\r\n");
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)buffer, buffer_len, 1000);

    for (i = 1; i < 128; i++)
    {
        result = HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t)(i << 1), 2, 2);
        if (result == HAL_OK)
        {
            buffer_len = sprintf(buffer, "0x%02X ", i);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)buffer, buffer_len, 1000);
        }
    }
    buffer_len = sprintf(buffer, "\r\n");
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)buffer, buffer_len, 1000);
}
