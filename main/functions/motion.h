#ifndef MOTION_H
#define MOTION_H

#include "../include/bicaInclude.h"

void iniciar_home(void);
void parar_home(void);
bool sistema_referenciado(void);
bool home_em_andamento(void);
int32_t obter_posicao_absoluta(void);
void imprimir_estado_home(void);
void inicializar_sistema(void);
void exemplo_uso_completo(void);
void comando_emergencia(void);
void testar_encoder(void);
void teste_completo_sistema(void);
void mover_para_posicao(int32_t posicao);

#endif