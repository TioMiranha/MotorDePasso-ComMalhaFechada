#ifndef BICAINCLUDE_H
#define BICAINCLUDE_H

// Includes do sistema
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <math.h>
// Variáveis globais
#include ".././GLOBAL_VARS/vars.h"

//Structs
#include "../structs/ST_direcaoMotor.h"

// funções
#include ".././functions/initializersFunctions.h"
#include ".././functions/aplicarAmortecedor.h"
#include ".././functions/direcao.h"
#include ".././functions/motor.h"
#include ".././functions/movimentoControlado.h"
#include ".././functions/rampas.h"
#include ".././functions/velocidade.h"

// Configurações do motor
#define STEP_PIN GPIO_NUM_5
#define DIR_PIN GPIO_NUM_2
#define ENABLE_PIN GPIO_NUM_4

// Configuração RMT OTIMIZADA - 1 MHz resolution
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_CLK_DIV 100 // 80 MHz / 80 = 1 MHz

#endif