#ifndef GLOBAL_VARS
#define GLOBAL_VARS

#include "../include/bicaInclude.h"
#include "../structs/ST_direcaoMotor.h"
#include "../structs/ST_home.h"

// Variáveis globais
extern volatile int motor_ligado;
extern volatile uint32_t velocidade_pps;
extern TaskHandle_t tarefa_motor;

extern volatile int movimento_em_andamento;
extern SemaphoreHandle_t xMutexMovimento;
extern TaskHandle_t tarefa_movimento;

// Mutex para proteger variáveis compartilhadas
extern SemaphoreHandle_t xMutexVelocidade;

extern direcao_motor_t direcao_atual;

extern float amortecedor_factor; // Fator de suavização (0.0 - 1.0)
extern uint32_t ultimo_pps;
extern uint8_t ultimo_valor_normalizado;
extern const char *TAG;

extern uint16_t velocidade_normalizada;

// Encoder
extern TaskHandle_t tarefa_encoder;
extern volatile int encoder_ativo;
extern SemaphoreHandle_t xMutexEncoder;
//Home
extern home_control_t home_control;
extern SemaphoreHandle_t xMutexHome;
extern volatile int32_t posicao_acumulada;



extern bool home_encontrado;
extern SemaphoreHandle_t xMutexHome;
#endif