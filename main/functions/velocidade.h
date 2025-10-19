#ifndef ACELERACAO_H
#define ACELERACAO_H
#include "../include/bicaInclude.h"

#define VELOCIDADE_NEUTRA 4096
#define VELOCIDADE_MINIMA 0
#define VELOCIDADE_MAXIMA 8192
#define PPS_MAXIMO 20000
#define PPS_MINIMO 0


void alterar_velocidade(uint32_t nova_velocidade);
void converter_normalizada_para_pps(uint16_t normalizada, uint32_t *pps, direcao_motor_t *direcao);
uint16_t converter_pps_para_normalizada(uint32_t pps, direcao_motor_t direcao);
void converter_pps_para_normalizada_durante_a_inversao(uint32_t pps, direcao_motor_t direcao, uint16_t *normalizada);
void converter_normalizada_para_pps_corrigida(uint16_t normalizada, uint32_t *pps, direcao_motor_t *direcao);
int comparar_velocidades(uint16_t vel1, uint16_t vel2);
uint16_t obter_magnitude_velocidade(uint16_t normalizada);
void setar_velocidade_normalizada(uint16_t nova_velocidade);

#endif