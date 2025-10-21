#ifndef ST_MOVIMENTOENCODER_H
#define ST_MOVIMENTOENCODER_H

#include "../include/bicaInclude.h"

// Tipos adicionais
typedef uint8_t u8;
typedef uint8_t u8t;
typedef uint16_t u16;
typedef uint32_t u32;

typedef union {
    struct {
        uint32_t d0;
        uint32_t d1;
    } Dword;
    struct {
        uint16_t word0;
        uint16_t word1;
        uint16_t word2;
        uint16_t word3;
    } word;
} uUnion64;

typedef struct {
    // Controle de posição
    int32_t currentLocation;      // Posição atual do eixo
    int32_t targetLocation;       // Posição alvo/destino
    int32_t decelLocation;        // Posição onde inicia a desaceleração
    
    // Controle de velocidade e aceleração
    float moveAccelSteps;         // Aceleração configurada do movimento
    float moveMaxSpeedSteps;      // Velocidade máxima configurada
    int32_t accel;                // Valor de aceleração calculado
    int32_t maxSpeed;             // Velocidade máxima calculada
    int32_t currentSpeed;         // Velocidade atual do eixo
    int32_t accumulator;          // Acumulador para controle de passos
    
    // Flags de estado e controle
    int8_t motionDirection;       // Direção do movimento (DIRECTION_FORWARD/DIRECTION_REVERSE)
    bool bEnableMotion;           // Flag indicando se o movimento está habilitado
    bool bDecel;                  // Flag indicando se está em desaceleração
    bool ativo;                   // Flag indicando se o eixo está ativo
    bool flg_zrn;                 // Flag para zero return (referência)
    bool flg_stop;                // Flag indicando parada solicitada
    
} move_axis_t;

#endif