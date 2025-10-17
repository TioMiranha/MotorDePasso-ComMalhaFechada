#ifndef GLOBAL_VARS
#define GLOBAL_VARS

#include "../include/bicaInclude.h"
#include "../structs/ST_direcaoMotor.h"

// Variáveis globais
extern volatile int motor_ligado = 0;
extern volatile uint32_t velocidade_pps = 2000;
extern TaskHandle_t tarefa_motor = NULL;

extern volatile int movimento_em_andamento = 0;
extern SemaphoreHandle_t xMutexMovimento;
uint32_t movimento_padrao = 2000;
extern TaskHandle_t tarefa_movimento = NULL;

// Mutex para proteger variáveis compartilhadas
extern SemaphoreHandle_t xMutexVelocidade;

extern direcao_motor_t direcao_atual = DIRECAO_HORARIA;

float amortecedor_factor = 0.2f; // Fator de suavização (0.0 - 1.0)
uint32_t ultimo_pps = 0;

extern const char *TAG = "MOTOR_RMT";

#endif