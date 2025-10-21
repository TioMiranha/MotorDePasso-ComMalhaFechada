#ifndef MOVIMENTO_ENCODER_H
#define MOVIMENTO_ENCODER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "../include/bicaInclude.h"

// Definições
//#define FIM_DE_CURSO_PIN      GPIO_NUM_4  // Ajuste para seu pino

// Variáveis externas
extern bool home_encontrado;

// Protótipos das funções existentes
void parar_movimento(void);

// Novas funções do home
void inicia_fim_de_curso(void);
void home_robusto(void *pvParameters);
void iniciar_home_robusto(void);
void parar_home(void);
bool home_finalizado_com_sucesso(void);
bool verifica_timeout(uint32_t start_time, uint32_t timeout_ms);
void alterar_velocidade_suave(uint32_t nova_velocidade);

#endif