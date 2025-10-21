#ifndef PCA9555_H
#define PCA9555_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/semphr.h>
#include "driver/i2c.h"
#include "../include/bicaInclude.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Endereço I2C do PCA9555 (definido pelos pinos A0, A1, A2)
#define PCA9555_I2C_ADDR 0x20

// Registradores do PCA9555
#define PCA9555_REG_INPUT 0x00
#define PCA9555_REG_OUTPUT 0x02
#define PCA9555_REG_POLARITY 0x04
#define PCA9555_REG_CONFIG 0x06

   // extern SemaphoreHandle_t spi_mutex;
   extern SemaphoreHandle_t i2c_master_0;

   void pca9555_init();
   void pca9555_write_reg(uint8_t device_addr, uint8_t reg_addr, uint16_t data);
   void pca9555_write_bit(uint8_t adrr, uint8_t reg, uint8_t bit, uint8_t value);
   uint16_t pca9555_read_reg(uint8_t device_addr, uint8_t reg_addr);
   uint8_t pca9555_read_bit(uint8_t adrr, uint8_t reg, uint8_t bit);

#ifdef __cplusplus
}
#endif
#endif