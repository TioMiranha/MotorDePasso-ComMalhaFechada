#ifndef APLICARAMORTECEDOR_H
#define APLICARAMORTECEDOR_H

#include "../include/bicaInclude.h"

void resetar_amortecedor(uint16_t valor_inicial);
uint16_t aplicar_amortecedor(uint16_t valor_desejado);
void configurar_amortecedor(float factor);

#endif