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
    if (*pps <= PPS_MINIMO)
        *pps = PPS_MINIMO;
    if (*pps >= PPS_MAXIMO)
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
    if (nova_velocidade <= VELOCIDADE_MINIMA)
        nova_velocidade = VELOCIDADE_MINIMA;
    if (nova_velocidade >= VELOCIDADE_MAXIMA)
        nova_velocidade = VELOCIDADE_MAXIMA;

    uint32_t pps_alvo = 0;
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    alterar_velocidade(pps_alvo);
    velocidade_normalizada = nova_velocidade;
}

int comparar_velocidades(uint16_t vel1, uint16_t vel2)
{
    if ((vel1 < VELOCIDADE_NEUTRA && vel2 < VELOCIDADE_NEUTRA) ||
        (vel1 > VELOCIDADE_NEUTRA && vel2 > VELOCIDADE_NEUTRA))
    {

        if (vel1 < VELOCIDADE_NEUTRA)
        {
            return (vel1 < vel2) ? 1 : ((vel1 > vel2) ? -1 : 0);
        }
        else
        {
            return (vel1 > vel2) ? 1 : ((vel1 < vel2) ? -1 : 0);
        }
    }
    return 0;
}

uint16_t obter_magnitude_velocidade(uint16_t normalizada)
{
    if (normalizada == VELOCIDADE_NEUTRA)
        return 0;

    if (normalizada < VELOCIDADE_NEUTRA)
    {
        return VELOCIDADE_NEUTRA - normalizada;
    }
    else
    {
        return normalizada - VELOCIDADE_NEUTRA;
    }
}

void acelerar_suavemente_para(uint32_t vel_alvo)
{
    if (!motor_ligado)
    {
        printf("❌ Motor desligado!\n");
        return;
    }

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    if (vel_alvo <= velocidade_atual)
    {
        printf("⚠️  Velocidade alvo (%u) menor ou igual à atual (%u)\n", vel_alvo, velocidade_atual);
        return;
    }

    if (vel_alvo > PPS_MAXIMO)
        vel_alvo = PPS_MAXIMO;

    printf("🚀 Aceleração suave: %u → %u PPS\n", velocidade_atual, vel_alvo);

    uint32_t diferenca = vel_alvo - velocidade_atual;
    uint32_t tempo_aceleracao;

    if (diferenca < 1000)
        tempo_aceleracao = 600;
    else if (diferenca < 3000)
        tempo_aceleracao = 850;
    else if (diferenca < 8000)
        tempo_aceleracao = 1000;
    else
        tempo_aceleracao = 1200;

    uint32_t inicio = xTaskGetTickCount();
    uint32_t tempo_decorrido = 0;

    while (tempo_decorrido < tempo_aceleracao && motor_ligado)
    {
        esp_task_wdt_reset();

        float progresso = (float)tempo_decorrido / tempo_aceleracao;
        float progresso_suavizado = powf(progresso, 1.8f);

        uint32_t pps_interpolado = velocidade_atual + (uint32_t)(diferenca * progresso_suavizado);

        if (pps_interpolado > PPS_MAXIMO)
            pps_interpolado = PPS_MAXIMO;
        if (pps_interpolado <= PPS_MINIMO && pps_interpolado > 0)
            pps_interpolado = PPS_MINIMO;

        alterar_velocidade(pps_interpolado);

        vTaskDelay(pdMS_TO_TICKS(15));
        tempo_decorrido = (xTaskGetTickCount() - inicio) * portTICK_PERIOD_MS;
    }

    alterar_velocidade(vel_alvo);
    printf("✅ Aceleração concluída: %u PPS\n", vel_alvo);
}

void desacelerar_suavemente_para(uint32_t vel_alvo)
{
    if (!motor_ligado) {
        printf("❌ Motor desligado!\n");
        return;
    }

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE) {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    if (vel_alvo >= velocidade_atual) {
        printf("⚠️  Velocidade alvo (%u) maior ou igual à atual (%u)\n", vel_alvo, velocidade_atual);
        return;
    }

    if (vel_alvo < PPS_MINIMO) vel_alvo = PPS_MINIMO;

    printf("📉 Desaceleração suave: %u → %u PPS\n", velocidade_atual, vel_alvo);

    uint32_t diferenca = velocidade_atual - vel_alvo;
    uint32_t tempo_desaceleracao;
    
    float diferenca_relativa = (float)diferenca / velocidade_atual;
    
    if (diferenca_relativa < 0.3f) tempo_desaceleracao = 550;
    else if (diferenca_relativa < 0.6f) tempo_desaceleracao = 800;
    else if (diferenca < 5000) tempo_desaceleracao = 1000;
    else tempo_desaceleracao = 1200;

    if (tempo_desaceleracao < 300) tempo_desaceleracao = 300;

    uint32_t inicio = xTaskGetTickCount();
    uint32_t tempo_decorrido = 0;
    uint32_t ultima_velocidade = velocidade_atual;

    while (tempo_decorrido < tempo_desaceleracao && motor_ligado) {
        esp_task_wdt_reset();

        float progresso = (float)tempo_decorrido / tempo_desaceleracao;
        
        float progresso_suavizado;
        if (progresso < 0.5f) {
            progresso_suavizado = 2.0f * progresso * progresso;
        } else {
            progresso_suavizado = 1.0f - powf(2.0f * (1.0f - progresso), 2.0f) / 2.0f;
        }
        
        uint32_t pps_interpolado = velocidade_atual - (uint32_t)(diferenca * progresso_suavizado);
        
        if (pps_interpolado > PPS_MAXIMO) pps_interpolado = PPS_MAXIMO;
        if (pps_interpolado < PPS_MINIMO) pps_interpolado = PPS_MINIMO;

        if (ultima_velocidade > pps_interpolado) {
            uint32_t delta = ultima_velocidade - pps_interpolado;
            if (delta > (diferenca / 20)) { 
                pps_interpolado = ultima_velocidade - (diferenca / 20);
            }
        }

        alterar_velocidade(pps_interpolado);
        ultima_velocidade = pps_interpolado;
        
        vTaskDelay(pdMS_TO_TICKS(10));
        tempo_decorrido = (xTaskGetTickCount() - inicio) * portTICK_PERIOD_MS;
    }

    alterar_velocidade(vel_alvo);
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    printf("✅ Desaceleração concluída: %u PPS\n", vel_alvo);
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