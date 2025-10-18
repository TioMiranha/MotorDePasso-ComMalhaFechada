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
        return VELOCIDADE_NEUTRA - VELOCIDADE_NEUTRA * (pps / PPS_MAXIMO);
    }
    else
    {
        return VELOCIDADE_NEUTRA + VELOCIDADE_NEUTRA * (pps / PPS_MAXIMO);
    }
}

void setar_velocidade_normalizada(uint16_t nova_velocidade)
{
    // Aplica limites
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