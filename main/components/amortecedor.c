#include "../include/bicaInclude.h"

uint32_t aplicar_amortecedor(uint32_t pps_desejado)
{
    if (ultimo_pps == 0)
    {
        ultimo_pps = pps_desejado;
        return pps_desejado;
    }

    uint32_t pps_suavizado = (uint32_t)(amortecedor_factor * pps_desejado +
                                        (1.0f - amortecedor_factor) * ultimo_pps);

    ultimo_pps = pps_suavizado;
    return pps_suavizado;
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