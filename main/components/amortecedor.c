#include "../include/bicaInclude.h"

#include "../include/bicaInclude.h"

uint16_t aplicar_amortecedor(uint16_t valor_desejado) {
    if (ultimo_valor_normalizado == VELOCIDADE_NEUTRA && valor_desejado != VELOCIDADE_NEUTRA) {
        uint16_t valor_suavizado = (uint16_t)(0.1f * valor_desejado + 0.9f * ultimo_valor_normalizado);
        ultimo_valor_normalizado = valor_suavizado;
        return valor_suavizado;
    }
    
    if (valor_desejado == VELOCIDADE_NEUTRA && ultimo_valor_normalizado != VELOCIDADE_NEUTRA) {
        uint16_t valor_suavizado = (uint16_t)(0.2f * valor_desejado + 0.8f * ultimo_valor_normalizado);
        ultimo_valor_normalizado = valor_suavizado;
        return valor_suavizado;
    }
    
    int32_t diferenca = abs((int32_t)valor_desejado - (int32_t)ultimo_valor_normalizado);
    float factor_adaptativo = amortecedor_factor;
    
    if (diferenca > 1000) {
        factor_adaptativo = amortecedor_factor * 0.5f;
    }
    
    uint16_t valor_suavizado = (uint16_t)(factor_adaptativo * valor_desejado + 
                                        (1.0f - factor_adaptativo) * ultimo_valor_normalizado);
    
    ultimo_valor_normalizado = valor_suavizado;
    return valor_suavizado;
}

void resetar_amortecedor(uint16_t valor_inicial) {
    ultimo_valor_normalizado = valor_inicial;
    printf("🔄 Amortecedor resetado para: %u\n", valor_inicial);
}

void configurar_amortecedor(float factor) {
    if (factor < 0.05f) factor = 0.05f;  
    if (factor > 0.8f) factor = 0.8f;    
    
    amortecedor_factor = factor;
    printf("🎛️  Amortecedor configurado: %.2f\n", factor);
}

uint16_t obter_ultimo_valor_amortecedor() {
    return ultimo_valor_normalizado;
}