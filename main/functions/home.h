#ifndef HOME_H
#define HOME_H

#include "../include/bicaInclude.h"
#include <unistd.h>
#include "../functions/motion.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/pcnt.h"

// Configurações do Home
#define FIM_DE_CURSO_PIN      GPIO_NUM_27
#define HOMING_FAST_SPEED     2000        
#define HOMING_SLOW_SPEED     500        
#define HOMING_ACCEL          1000       
#define HOMING_TIMEOUT_MS     30000   
#define PCNT_H_LIM            3251
#define PCNT_L_LIM            -3251   
#define PCNT_ENCODER_UNIT  PCNT_UNIT_0

// Protótipos das funções
void home_init(void);
bool home_start(void);
void home_stop(void);
bool home_is_complete(void);
int32_t home_get_position(void);
bool home_is_homing(void);
void home_task(void *pvParameters);
// Funções do encoder
int32_t encoder_get_position(void);
void encoder_reset_position(void);
bool home_switch_activated(void);



#endif 