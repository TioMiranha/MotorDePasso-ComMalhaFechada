#ifndef MOVIMENTOCONTROLADO_H
#define MOVIMENTOCONTROLADO_H

#include "../include/bicaInclude.h"

void tarefa_movimento_controlado(void *param);
void mover_frente(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms);
void mover_tras(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms);
void parar_movimento();
void executar_mover_frente_rapido();
void executar_mover_frente_devagar();
void executar_mover_tras_rapido();
void executar_mover_tras_devagar();
void movimento_continuo(int direcao);
void movimento_continuo_home(int direcao, uint32_t velocidade);
void movimento_controlado(int direcao, uint32_t velocidade);


#endif