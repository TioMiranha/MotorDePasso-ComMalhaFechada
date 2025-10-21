#ifndef ST_HOME_H
#define ST_HOME_H

#include ".././include/bicaInclude.h"

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
    bool homing_in_progress;
} home_control_t;

#endif