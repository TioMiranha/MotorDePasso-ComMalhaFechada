#ifndef RAMPAS_H
#define RAMPAS_H

#include "../include/bicaInclude.h"

// Aceleração da rampa trapezoidal
void rampa_aceleracao_trapezoidal(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms);
void rampa_aceleracao_trapezoidal_avancada(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms, uint32_t aceleracao_max_pps_s);
void executar_rampa_trapezoidal_rapida();
void executar_rampa_trapezoidal_suave();
void executar_rampa_trapezoidal_avancada_rapida();
void executar_rampa_trapezoidal_avancada_rapida_em_transicao();

// Desaceleração da rampa trapezoidal
void rampa_desaceleracao_trapezoidal(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms);
void executar_desaceleracao_trapezoidal_rapida();
void executar_desaceleracao_trapezoidal_muito_rapida();
void executar_desaceleracao_para_zero_e_inverter(uint32_t vel_atual, direcao_motor_t nova_direcao);

#endif