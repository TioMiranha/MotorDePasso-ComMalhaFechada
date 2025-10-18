#ifndef RAMPAS_H
#define RAMPAS_H

#include "../include/bicaInclude.h"

// Aceleração da rampa trapezoidal
void rampa_aceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms);
void executar_rampa_trapezoidal_rapida();
void executar_rampa_trapezoidal_avancada_rapida_em_transicao();

// Desaceleração da rampa trapezoidal
void executar_rampa_direta(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms);
void rampa_desaceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms);
void executar_desaceleracao_trapezoidal_rapida();
void executar_desaceleracao_trapezoidal_muito_rapida();
void executar_desaceleracao_para_zero_e_inverter(uint16_t vel_normalizada_atual, direcao_motor_t nova_direcao);

#endif