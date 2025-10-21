#include <stdio.h>
#include "PCA9555.h"

SemaphoreHandle_t i2c_master_0;
i2c_cmd_handle_t cmd;

/**
 * @brief Initialize the I2C master interface
 *
 * This function configures and initializes the I2C master interface
 * with preset parameters such as SDA and SCL pin numbers, pull-up
 * resistors, and clock speed. It sets up the I2C configuration, applies
 * it, and installs the I2C driver for the specified I2C master port.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if parameter error
 *     - ESP_FAIL if driver installation fails
 */
static esp_err_t i2c_master_init()
{
   int i2c_master_port = 0;

   i2c_config_t conf = {
       .mode = I2C_MODE_MASTER,
       .sda_io_num = 21,
       .scl_io_num = 22,
       .sda_pullup_en = GPIO_PULLUP_ENABLE,
       .scl_pullup_en = GPIO_PULLUP_ENABLE,
       .master.clk_speed = 400000,           // barramento I2C a 400kHz
   };

   i2c_param_config(i2c_master_port, &conf);

   return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

/**
 * @brief  Read a 16-bit register value from a PCA9555 device
 * @param  device_addr: The I2C address of the PCA9555 device
 * @param  reg_addr: The address of the register to read
 * @return  The 16-bit value read from the register
 */
uint16_t pca9555_read_reg(uint8_t device_addr, uint8_t reg_addr)
{
   uint8_t data[2] = {0, 0};
   if (xSemaphoreTake(i2c_master_0, portMAX_DELAY))
   {
      i2c_master_write_read_device(0, device_addr, &reg_addr, 1, data, 2, 1000 / portTICK_RATE_MS);
      xSemaphoreGive(i2c_master_0);
   }
   else
   {
      // Não conseguiu adquirir o mutex, tratamento de erro
      printf(" Não conseguiu adquirir o mutex\n");
   }
   return (uint16_t)((data[1] << 8) | data[0]);
}

/**
 * @brief  Write a 16-bit register value to a PCA9555 device
 * @param  device_addr: The I2C address of the PCA9555 device
 * @param  reg_addr: The address of the register to write
 * @param  data: The 16-bit value to write to the register
 */
void pca9555_write_reg(uint8_t device_addr, uint8_t reg_addr, uint16_t data)
{
   if (xSemaphoreTake(i2c_master_0, portMAX_DELAY))
   {
      uint8_t write_buf[3] = {reg_addr, (uint8_t)(data & 0x00FF), (uint8_t)((data & 0xFF00) >> 8)};
      i2c_master_write_to_device(0, device_addr, write_buf, sizeof(write_buf), 1000 / portTICK_RATE_MS);
      xSemaphoreGive(i2c_master_0);
   }
   else
   {
      // Não conseguiu adquirir o mutex, tratamento de erro
      printf(" Não conseguiu adquirir o mutex\n");
   }
}

/**
 * @brief  Read a single bit from a PCA9555 register
 * @param  adrr: The I2C address of the PCA9555 device
 * @param  reg: The address of the register to read
 * @param  bit: The bit index to read (0-15)
 * @return  The value of the specified bit (0 or 1)
 */
uint8_t pca9555_read_bit(uint8_t adrr, uint8_t reg, uint8_t bit)
{
   // Ler o valor atual da porta de entrada
   uint16_t input_data = pca9555_read_reg(adrr, reg);

   // Retornar o valor do bit específico
   return (input_data >> bit) & 0x01;
}

/**
 * @brief  Write a single bit to a PCA9555 register
 * @param  adrr: The I2C address of the PCA9555 device
 * @param  reg: The address of the register to write
 * @param  bit: The bit index to write (0-15)
 * @param  value: The value to write to the bit (0 or 1)
 */
void pca9555_write_bit(uint8_t adrr, uint8_t reg, uint8_t bit, uint8_t value)
{
   // Ler o valor atual da porta de saída
   uint16_t output_data = pca9555_read_reg(adrr, reg);
   
   //  Modificar o bit desejado
   if (value == 0)
   {
      output_data &= ~(1 << bit);
   }
   else
   {
      output_data |= (1 << bit);
   }

   // Escrever o novo valor de volta na porta de saída
   pca9555_write_reg(adrr, reg, output_data);
}

/**
 * @brief Initialize the PCA9555 device and I2C interface
 *
 * This function initializes the I2C master interface and creates a mutex
 * for synchronizing I2C access. It is essential to call this function
 * before performing any read or write operations with the PCA9555 device.
 * The function ensures that the I2C driver is installed and the necessary
 * resources are allocated for communication with the PCA9555.
 */

void pca9555_init()
{
   i2c_master_init();
   i2c_master_0 = xSemaphoreCreateMutex();
   vTaskDelay(10 / portTICK_PERIOD_MS);
}
