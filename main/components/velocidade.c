#include "../include/bicaInclude.h"

void converter_normalizada_para_pps(uint16_t normalizada, uint32_t *pps, direcao_motor_t *direcao)
{
    if (normalizada == VELOCIDADE_NEUTRA)
    {
        *pps = VELOCIDADE_NEUTRA;
        *direcao = direcao_atual;
        return;
    }

    if (normalizada < VELOCIDADE_NEUTRA)
    {
        *direcao = DIRECAO_HORARIA;
        uint16_t bits = normalizada;
        *pps = (uint32_t)(((bits - VELOCIDADE_MINIMA) * (PPS_MAXIMO - PPS_MINIMO) / 4096) + PPS_MINIMO);
    }
    else
    {
        *direcao = DIRECAO_ANTI_HORARIA;
        uint16_t bits = normalizada;
        *pps = (uint32_t)(((bits - VELOCIDADE_NEUTRA) * (PPS_MAXIMO - PPS_MINIMO) / 4096) + PPS_MINIMO);
    }

    // Garante limites
    if (*pps < PPS_MINIMO)
        *pps = PPS_MINIMO;
    if (*pps > PPS_MAXIMO)
        *pps = PPS_MAXIMO;

    if (*pps < 100)
    {
        *pps = 0;
        *direcao = direcao_atual;
    }
}

uint16_t converter_pps_para_normalizada(uint32_t pps, direcao_motor_t direcao)
{
    if (pps == 0)
    {
        return VELOCIDADE_NEUTRA;
    }

    if (direcao == DIRECAO_HORARIA)
    {
        return (uint16_t)(VELOCIDADE_NEUTRA - VELOCIDADE_NEUTRA * (pps / PPS_MAXIMO));
    }
    else
    {
        return (uint16_t)(VELOCIDADE_NEUTRA + VELOCIDADE_NEUTRA * (pps / PPS_MAXIMO));
    }
}

void setar_velocidade_normalizada(uint16_t nova_velocidade)
{
    if (nova_velocidade < VELOCIDADE_MINIMA)
        nova_velocidade = VELOCIDADE_MINIMA;
    if (nova_velocidade > VELOCIDADE_MAXIMA)
        nova_velocidade = VELOCIDADE_MAXIMA;

    uint32_t pps_alvo;
    direcao_motor_t direcao_alvo;
    converter_normalizada_para_pps(nova_velocidade, &pps_alvo, &direcao_alvo);

    if (nova_velocidade == VELOCIDADE_NEUTRA)
    {
        alterar_velocidade(0);
        velocidade_normalizada = VELOCIDADE_NEUTRA;
        return;
    }

    if (direcao_atual != direcao_alvo && motor_ligado)
    {
        printf("   🔄 Mudando direção: %s -> %s\n",
               direcao_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
               direcao_alvo == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

        gpio_set_level(DIR_PIN, direcao_alvo);
        direcao_atual = direcao_alvo;
        vTaskDelay(pdMS_TO_TICKS(10)); // Aumentei para 10ms para garantir estabilização
    }

    alterar_velocidade(pps_alvo);
    velocidade_normalizada = nova_velocidade;
}

int comparar_velocidades(uint16_t vel1, uint16_t vel2) {
    if ((vel1 < VELOCIDADE_NEUTRA && vel2 < VELOCIDADE_NEUTRA) || 
        (vel1 > VELOCIDADE_NEUTRA && vel2 > VELOCIDADE_NEUTRA)) {
      
        if (vel1 < VELOCIDADE_NEUTRA) {
            return (vel1 < vel2) ? 1 : ((vel1 > vel2) ? -1 : 0);
        } else {
            return (vel1 > vel2) ? 1 : ((vel1 < vel2) ? -1 : 0);
        }
    }
    return 0; 
}

uint16_t obter_magnitude_velocidade(uint16_t normalizada) {
    if (normalizada == VELOCIDADE_NEUTRA) return 0;
    
    if (normalizada < VELOCIDADE_NEUTRA) {
        return VELOCIDADE_NEUTRA - normalizada;
    } else {
        return normalizada - VELOCIDADE_NEUTRA;
    }
}

void converter_pps_para_normalizada_durante_a_inversao(uint32_t pps, direcao_motor_t direcao, uint16_t *normalizada) {
    if (pps == 0) {
        *normalizada = VELOCIDADE_NEUTRA;
        return;
    }
    
    // Garantir que PPS está dentro dos limites
    if (pps < PPS_MINIMO) pps = PPS_MINIMO;
    if (pps > PPS_MAXIMO) pps = PPS_MAXIMO;
    
    // Converter PPS para uma escala linear 0-4095
    uint32_t faixa_pps = PPS_MAXIMO - PPS_MINIMO;
    uint32_t pps_relativo = pps - PPS_MINIMO;
    uint16_t magnitude = (uint16_t)((pps_relativo * 4095) / faixa_pps);
    
    if (direcao == DIRECAO_HORARIA) {
        // 🟢 CORREÇÃO CRÍTICA: Horária - 0 = rápido, 4095 = lento
        *normalizada = 4095 - magnitude;
        // Garantir que não ultrapassa os limites
        if (*normalizada > 4095) *normalizada = 4095;
        if (*normalizada < VELOCIDADE_MINIMA) *normalizada = VELOCIDADE_MINIMA;
    } else {
        // 🟢 CORREÇÃO CRÍTICA: Anti-horária - 4097 = lento, 8192 = rápido
        *normalizada = VELOCIDADE_NEUTRA + 1 + magnitude;
        // Garantir que não ultrapassa os limites
        if (*normalizada < 4097) *normalizada = 4097;
        if (*normalizada > VELOCIDADE_MAXIMA) *normalizada = VELOCIDADE_MAXIMA;
    }
}

void converter_normalizada_para_pps_corrigida(uint16_t normalizada, uint32_t *pps, direcao_motor_t *direcao) {
    if (normalizada == VELOCIDADE_NEUTRA) {
        *pps = 0;
        *direcao = direcao_atual;
        return;
    }

    if (normalizada < VELOCIDADE_NEUTRA) {
        *direcao = DIRECAO_HORARIA;
        // 🟢 CORREÇÃO: Horária - valores baixos = PPS alto, valores altos = PPS baixo
        uint16_t magnitude = 4095 - normalizada;
        *pps = PPS_MINIMO + (uint32_t)((magnitude * (PPS_MAXIMO - PPS_MINIMO)) / 4095);
    } else {
        *direcao = DIRECAO_ANTI_HORARIA;
        // 🟢 CORREÇÃO: Anti-horária - valores baixos = PPS baixo, valores altos = PPS alto
        uint16_t magnitude = normalizada - (VELOCIDADE_NEUTRA + 1);
        *pps = PPS_MINIMO + (uint32_t)((magnitude * (PPS_MAXIMO - PPS_MINIMO)) / 4095);
    }

    // Garantir limites
    if (*pps < PPS_MINIMO) *pps = PPS_MINIMO;
    if (*pps > PPS_MAXIMO) *pps = PPS_MAXIMO;
}

void alterar_velocidade(uint32_t nova_velocidade)
{
    if (nova_velocidade <= 0)
    {
        printf("Velocidade muito baixa! Minimo: 10 PPS\n");
        nova_velocidade = 0;
    }

    if (nova_velocidade > 20000)
    {
        printf("Velocidade limitada a 50.000 PPS\n");
        nova_velocidade = 20000;
    }

    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_pps = nova_velocidade;
        xSemaphoreGive(xMutexVelocidade);
    }

    printf("Velocidade alterada para %u PPS\n", nova_velocidade);
}