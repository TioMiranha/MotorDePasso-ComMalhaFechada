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

    // Tempo de aceleração baseado na diferença de velocidade
    uint32_t diferenca = vel_alvo - velocidade_atual;
    uint32_t tempo_aceleracao;

    if (diferenca < 1000)
        tempo_aceleracao = 550;
    else if (diferenca < 3000)
        tempo_aceleracao = 800;
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

    // Garantir velocidade final exata
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

    // Cálculo melhorado do tempo de desaceleração
    uint32_t diferenca = velocidade_atual - vel_alvo;
    uint32_t tempo_desaceleracao;
    
    // Tempo baseado na diferença relativa e velocidade atual
    float diferenca_relativa = (float)diferenca / velocidade_atual;
    
    if (diferenca_relativa < 0.3f) tempo_desaceleracao = 550;
    else if (diferenca_relativa < 0.6f) tempo_desaceleracao = 800;
    else if (diferenca < 5000) tempo_desaceleracao = 1000;
    else tempo_desaceleracao = 1200;

    // Garantir tempo mínimo para desaceleração suave
    if (tempo_desaceleracao < 300) tempo_desaceleracao = 300;

    uint32_t inicio = xTaskGetTickCount();
    uint32_t tempo_decorrido = 0;
    uint32_t ultima_velocidade = velocidade_atual;

    while (tempo_decorrido < tempo_desaceleracao && motor_ligado) {
        esp_task_wdt_reset();

        float progresso = (float)tempo_decorrido / tempo_desaceleracao;
        
        // Curva de desaceleração melhorada - mais suave no início e fim
        float progresso_suavizado;
        if (progresso < 0.5f) {
            progresso_suavizado = 2.0f * progresso * progresso;
        } else {
            progresso_suavizado = 1.0f - powf(2.0f * (1.0f - progresso), 2.0f) / 2.0f;
        }
        
        uint32_t pps_interpolado = velocidade_atual - (uint32_t)(diferenca * progresso_suavizado);
        
        // Limites mais rigorosos
        if (pps_interpolado > PPS_MAXIMO) pps_interpolado = PPS_MAXIMO;
        if (pps_interpolado < PPS_MINIMO) pps_interpolado = PPS_MINIMO;

        // Evitar mudanças muito bruscas entre steps consecutivos
        if (ultima_velocidade > pps_interpolado) {
            uint32_t delta = ultima_velocidade - pps_interpolado;
            // Limitar a variação máxima por step
            if (delta > (diferenca / 20)) { // Máximo 5% da diferença total por step
                pps_interpolado = ultima_velocidade - (diferenca / 20);
            }
        }

        alterar_velocidade(pps_interpolado);
        ultima_velocidade = pps_interpolado;
        
        vTaskDelay(pdMS_TO_TICKS(10));
        tempo_decorrido = (xTaskGetTickCount() - inicio) * portTICK_PERIOD_MS;
    }

    // Garantir velocidade final exata
    alterar_velocidade(vel_alvo);
    
    // Pequena pausa para estabilização
    vTaskDelay(pdMS_TO_TICKS(20));
    
    printf("✅ Desaceleração concluída: %u PPS\n", vel_alvo);
}

void desacelerar_e_desligar()
{
    if (!motor_ligado) {
        printf("❌ Motor já está desligado!\n");
        return;
    }

    printf("🛑 Iniciando desaceleração e desligamento do motor\n");

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE) {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    // Se já estiver parado ou quase parado, apenas vai para neutro
    if (velocidade_atual <= PPS_MINIMO) {
        printf("⚡ Motor quase parado - indo para posição neutra\n");
        
        // Apenas vai para neutro sem desligar completamente
        setar_velocidade_normalizada(VELOCIDADE_NEUTRA);
        
        printf("✅ Motor em posição neutra\n");
        return;
    }

    // DESACELERAÇÃO ULTRA-SUAVE PARA PARADA COMPLETA
    printf("📉 Desacelerando de %u PPS até parar completamente\n", velocidade_atual);

    // Converter PPS atual para valor normalizado para manter a direção correta
    uint16_t normalizada_atual = converter_pps_para_normalizada(velocidade_atual, direcao_atual);
    
    // Calcular a trajetória de desaceleração no espaço normalizado
    uint16_t normalizada_neutra = VELOCIDADE_NEUTRA;
    
    // Determinar se estamos no sentido horário ou anti-horário
    uint16_t magnitude_atual = obter_magnitude_velocidade(normalizada_atual);
    uint8_t sentido_horario = (normalizada_atual < VELOCIDADE_NEUTRA);

    printf("🎯 Desaceleração no espaço normalizado: %u → %u (%s)\n", 
           normalizada_atual, normalizada_neutra,
           sentido_horario ? "HORÁRIO" : "ANTI-HORÁRIO");

    // Estratégia de desaceleração adaptativa baseada na magnitude
    uint32_t num_steps;
    
    if (magnitude_atual < 1000) num_steps = 15;
    else if (magnitude_atual < 2500) num_steps = 20;
    else if (magnitude_atual < 4000) num_steps = 25;
    else num_steps = 30;

    for (uint32_t step = 1; step <= num_steps; step++) {
        // Verificar se o motor ainda deve continuar desacelerando
        if (!motor_ligado) {
            printf("⚠️  Desaceleração interrompida - motor foi religado\n");
            return;
        }

        esp_task_wdt_reset();

        float progresso = (float)step / num_steps;
        
        // Curva de desaceleração muito suave para parada total
        float progresso_suavizado;
        if (progresso < 0.3f) {
            progresso_suavizado = progresso * 0.4f; // Bem suave no início
        } else if (progresso < 0.7f) {
            progresso_suavizado = 0.12f + (progresso - 0.3f) * 0.93f; // CORREÇÃO: 0.24f para 0.12f
        } else {
            progresso_suavizado = 0.48f + (progresso - 0.7f) * 1.73f; // CORREÇÃO: ajuste matemático
        }
        
        if (progresso_suavizado > 1.0f) progresso_suavizado = 1.0f;
        
        // Calcular valor normalizado interpolado
        uint16_t normalizada_interpolada;
        if (sentido_horario) {
            // Horário: vai de normalizada_atual até VELOCIDADE_NEUTRA
            int32_t diferenca = VELOCIDADE_NEUTRA - normalizada_atual;
            normalizada_interpolada = normalizada_atual + (uint16_t)(diferenca * progresso_suavizado);
        } else {
            // Anti-horário: vai de normalizada_atual até VELOCIDADE_NEUTRA
            int32_t diferenca = normalizada_atual - VELOCIDADE_NEUTRA;
            normalizada_interpolada = normalizada_atual - (uint16_t)(diferenca * progresso_suavizado);
        }

        // Garantir que não ultrapassemos o ponto neutro
        if (sentido_horario && normalizada_interpolada > VELOCIDADE_NEUTRA) {
            normalizada_interpolada = VELOCIDADE_NEUTRA;
        } else if (!sentido_horario && normalizada_interpolada < VELOCIDADE_NEUTRA) {
            normalizada_interpolada = VELOCIDADE_NEUTRA;
        }

        // Usar setar_velocidade_normalizada para garantir conversão correta
        setar_velocidade_normalizada(normalizada_interpolada);

        // Delay progressivamente maior conforme nos aproximamos do neutro
        uint32_t magnitude_atual_step = obter_magnitude_velocidade(normalizada_interpolada);
        uint32_t delay_ms;
        
        if (magnitude_atual_step > 2000) delay_ms = 8;
        else if (magnitude_atual_step > 800) delay_ms = 12;
        else if (magnitude_atual_step > 200) delay_ms = 15;
        else delay_ms = 20;
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // Se chegou ao neutro, para imediatamente
        if (normalizada_interpolada == VELOCIDADE_NEUTRA) {
            break;
        }
    }

    // GARANTIR PARADA COMPLETA NO PONTO NEUTRO
    setar_velocidade_normalizada(VELOCIDADE_NEUTRA);
    
    // Pequena pausa para estabilização final
    vTaskDelay(pdMS_TO_TICKS(50));

    printf("✅ Motor desacelerado e em posição neutra (%u)\n", VELOCIDADE_NEUTRA);
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