#ifndef DIRECAO_H
#define DIRECAO_H
#include "../include/bicaInclude.h"

void alterar_direcao_suave(direcao_motor_t nova_direcao);
void alterar_direcao_suave(direcao_motor_t nova_direcao);
void direcao_horaria();
void direcao_anti_horaria();
void inverter_direcao();
void acelerar_com_direcao(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms, direcao_motor_t direcao);
void executar_direcao_horaria_com_aceleracao();
void executar_direcao_anti_horaria_com_aceleracao();
void executar_inversao_suave();


#endif