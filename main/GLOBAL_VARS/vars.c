#include ".././include/bicaInclude.h"

// Tags
const char *TAG = "MOTOR_RMT";

// Variáveis globais

//MOtor
volatile int motor_ligado = 0;
volatile uint32_t velocidade_pps = 2000;
TaskHandle_t tarefa_motor = NULL;
//Movimento
volatile int movimento_em_andamento = 0;
SemaphoreHandle_t xMutexMovimento;
TaskHandle_t tarefa_movimento = NULL;

// Mutex de velocidade para proteger variáveis compartilhadas
SemaphoreHandle_t xMutexVelocidade;

//Direção
direcao_motor_t direcao_atual = DIRECAO_HORARIA;

//Amortecedor de rampa
float amortecedor_factor = 0.2f;
uint32_t ultimo_pps = 0;
uint8_t ultimo_valor_normalizado = VELOCIDADE_NEUTRA;
uint16_t velocidade_normalizada = VELOCIDADE_NEUTRA;

// Tarefa do Encoder
volatile int encoder_ativo = 0;
TaskHandle_t tarefa_encoder = NULL;
SemaphoreHandle_t xMutexEncoder;