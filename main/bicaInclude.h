#ifndef BICAINCLUDE_H
#define BICAINCLUDE_H
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "driver/ledc.h"
#include "driver/uart.h"

// Configurações dos pinos
#define ENCODER_A_GPIO      GPIO_NUM_23
#define ENCODER_B_GPIO      GPIO_NUM_16
#define MOTOR_PWM_GPIO      GPIO_NUM_5
#define MOTOR_DIR_GPIO      GPIO_NUM_2    // Único pino de direção

// Configuração PWM
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY      5000

#define PCNT_UNIT           PCNT_UNIT_0
#define TAG                 "MOTOR_DIR_SIMPLES"
//extern rotary_encoder_t *rotaryEncoder;

#endif