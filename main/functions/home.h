#ifndef HOME_H
#define HOME_H

#include "../include/bicaInclude.h"
#include "M8000.h"
#include <unistd.h>
#include "../functions/motion.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/pcnt.h"

// Configurações do Home
#define HOME_SWITCH_GPIO      GPIO_NUM_34  
#define HOMING_FAST_SPEED     2000        
#define HOMING_SLOW_SPEED     500        
#define HOMING_ACCEL          1000       
#define HOMING_TIMEOUT_MS     30000      

// Estados do Home
typedef enum {
    HOME_IDLE = 0,
    HOME_FAST_SEARCH,
    HOME_BACK_OFF,
    HOME_SLOW_SEARCH,
    HOME_COMPLETE,
    HOME_ERROR
} home_state_t;

// Estrutura de controle do home
typedef struct {
    home_state_t state;
    bool home_found;
    int32_t home_position;
    uint32_t start_time;
    uint8_t target_axis;
    bool homing_in_progress;
} home_control_t;

// Variáveis globais
extern home_control_t home_control;
extern SemaphoreHandle_t xMutexHome;


#endif 