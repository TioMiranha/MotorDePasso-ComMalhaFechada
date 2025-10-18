#include "../include/bicaInclude.h"

// Função para aplicar amortecedor a valores normalizados
uint8_t aplicar_amortecedor(uint8_t valor_desejado) {
    if (ultimo_valor_normalizado == 0 && valor_desejado == 0) {
        return 0;
    }
    
    // Aplica suavização exponencial
    uint8_t valor_suavizado = (uint8_t)(amortecedor_factor * valor_desejado + 
                                       (1.0f - amortecedor_factor) * ultimo_valor_normalizado);
    
    ultimo_valor_normalizado = valor_suavizado;
    return valor_suavizado;
}

void configurar_amortecedor(float factor)
{
    if (factor < 0.0f)
        factor = 0.0f;
    if (factor > 1.0f)
        factor = 1.0f;
    amortecedor_factor = factor;
    printf("Amortecedor configurado: %.2f\n", factor);
}