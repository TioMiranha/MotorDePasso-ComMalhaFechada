#include "../include/bicaInclude.h"

void rampa_desaceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms)
{
    printf("🎯 Iniciando rampa de DESACELERAÇÃO trapezoidal: %u -> %u em %u ms\n", 
           normalizada_inicial, normalizada_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    // Garantir valores válidos (0-8192)
    if (normalizada_inicial > VELOCIDADE_MAXIMA) normalizada_inicial = VELOCIDADE_MAXIMA;
    if (normalizada_final > VELOCIDADE_MAXIMA) normalizada_final = VELOCIDADE_MAXIMA;

    // CORREÇÃO: Verificar mudança de direção usando a mesma lógica da rampa de aceleração
    direcao_motor_t dir_inicial, dir_final;
    uint32_t pps_temp;
    
    converter_normalizada_para_pps(normalizada_inicial, &pps_temp, &dir_inicial);
    converter_normalizada_para_pps(normalizada_final, &pps_temp, &dir_final);

    // CORREÇÃO: Se há mudança de direção, usar a rampa de aceleração que já trata isso
    if (dir_inicial != dir_final && normalizada_inicial != VELOCIDADE_NEUTRA && normalizada_final != VELOCIDADE_NEUTRA) {
        printf("🔄 ALERTA: Mudança de direção detectada! Usando rampa de aceleração...\n");
        rampa_aceleracao_trapezoidal(normalizada_inicial, normalizada_final, duracao_ms);
        return;
    }

    // CORREÇÃO: Verificar se é realmente uma desaceleração (PPS diminui)
    uint32_t pps_inicial, pps_final;
    converter_normalizada_para_pps(normalizada_inicial, &pps_inicial, &dir_inicial);
    converter_normalizada_para_pps(normalizada_final, &pps_final, &dir_final);

    if (pps_inicial <= pps_final && pps_inicial > 0)
    {
        printf("⚠️  Para desaceleração, a velocidade inicial deve ser MAIOR que a final!\n");
        printf("   PPS inicial: %u, PPS final: %u\n", pps_inicial, pps_final);
        return;
    }

    // 🟢 CORREÇÃO: Usar percentuais fixos de 33% para trapezoidal equilibrado
    uint32_t tempo_desaceleracao1, tempo_constante, tempo_desaceleracao2;
    uint32_t percentual_desaceleracao = 33;

    tempo_desaceleracao1 = (duracao_ms * percentual_desaceleracao) / 100;
    tempo_desaceleracao2 = tempo_desaceleracao1;
    tempo_constante = duracao_ms - tempo_desaceleracao1 - tempo_desaceleracao2;

    // 🟢 CORREÇÃO: Garantir que a fase constante tenha pelo menos 10ms
    if (tempo_constante < 10) {
        tempo_constante = 10;
        tempo_desaceleracao1 = (duracao_ms - tempo_constante) / 2;
        tempo_desaceleracao2 = duracao_ms - tempo_constante - tempo_desaceleracao1;
    }

    if (tempo_constante <= 0)
    {
        tempo_desaceleracao1 = duracao_ms / 2;
        tempo_desaceleracao2 = duracao_ms - tempo_desaceleracao1;
        tempo_constante = 0;
        printf("📐 Perfil TRIANGULAR (desaceleração1: %ums, desaceleração2: %ums)\n", 
               tempo_desaceleracao1, tempo_desaceleracao2);
    }
    else
    {
        printf("📊 Perfil TRAPEZOIDAL (desaceleração1: %u%%, constante: %u%%, desaceleração2: %u%%)\n", 
               percentual_desaceleracao, 100 - 2 * percentual_desaceleracao, percentual_desaceleracao);
    }

    printf("🔄 Fases da desaceleração:\n");
    printf("   • Desaceleração inicial: %u ms (%u → %u)\n", tempo_desaceleracao1, normalizada_inicial, normalizada_final);
    if (tempo_constante > 0)
    {
        printf("   • Velocidade constante: %u ms (%u)\n", tempo_constante, normalizada_final);
    }
    printf("   • Desaceleração final: %u ms (%u → %u)\n", tempo_desaceleracao2, normalizada_final, normalizada_final);

    uint32_t tempo_inicio = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t ultimo_watchdog_reset = tempo_inicio;

    esp_task_wdt_reset();

    // 🟢 CORREÇÃO: Fase de desaceleração inicial LINEAR PURA
    if (tempo_desaceleracao1 > 0 && motor_ligado)
    {
        printf("🛑 Iniciando fase de DESACELERAÇÃO INICIAL LINEAR\n");
        uint32_t tempo_atual = 0;
        
        while (tempo_atual < tempo_desaceleracao1 && motor_ligado)
        {
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 50)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // 🟢 CORREÇÃO: Progresso LINEAR PURA (sem curvas)
            float progresso = (float)tempo_atual / tempo_desaceleracao1;
            
            // 🟢 CORREÇÃO: Interpolação linear direta
            uint16_t normalizada_atual = normalizada_inicial - 
                                       (uint16_t)(progresso * (normalizada_inicial - normalizada_final));

            setar_velocidade_normalizada(normalizada_atual);

            tempo_atual = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tempo_inicio;
            vTaskDelay(pdMS_TO_TICKS(10)); // 🟢 CORREÇÃO: Delay constante para linearidade
        }
        
        // 🟢 CORREÇÃO: Garantir valor exato SEM overshoot
        setar_velocidade_normalizada(normalizada_final);
        printf("✅ Desaceleração inicial linear concluída: %u\n", normalizada_final);
    }

    // 🟢 CORREÇÃO: Fase constante EXATA
    if (tempo_constante > 0 && motor_ligado)
    {
        printf("⚡ Mantendo velocidade CONSTANTE: %u\n", normalizada_final);
        
        // 🟢 CORREÇÃO: Garantir que está exatamente na velocidade final
        setar_velocidade_normalizada(normalizada_final);
        
        // Pequena pausa para estabilização
        vTaskDelay(pdMS_TO_TICKS(5));
        
        // Manter velocidade constante pelo tempo determinado
        uint32_t tempo_fim_constante = tempo_inicio + tempo_desaceleracao1 + tempo_constante;
        
        while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < tempo_fim_constante && motor_ligado)
        {
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 50)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }
            
            // 🟢 CORREÇÃO: Reforçar velocidade constante periodicamente
            setar_velocidade_normalizada(normalizada_final);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        printf("✅ Fase constante concluída\n");
    }

    // 🟢 CORREÇÃO: Fase de desaceleração final LINEAR PURA
    if (tempo_desaceleracao2 > 0 && motor_ligado)
    {
        printf("🛑 Iniciando fase de DESACELERAÇÃO FINAL LINEAR\n");
        uint32_t tempo_inicio_desac2 = tempo_inicio + tempo_desaceleracao1 + tempo_constante;
        uint32_t tempo_atual = 0;

        while (tempo_atual < tempo_desaceleracao2 && motor_ligado)
        {
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 50)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            float progresso = (float)tempo_atual / tempo_desaceleracao2;
            
            setar_velocidade_normalizada(normalizada_final);

            tempo_atual = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tempo_inicio_desac2;
            vTaskDelay(pdMS_TO_TICKS(10)); // 🟢 CORREÇÃO: Delay constante para linearidade
        }
        
        // 🟢 CORREÇÃO: Garantir valor final exato SEM overshoot
        setar_velocidade_normalizada(normalizada_final);
        printf("✅ Desaceleração final linear concluída: %u\n", normalizada_final);
    }

    // Garantir velocidade final exata
    if (motor_ligado)
    {
        setar_velocidade_normalizada(normalizada_final);
    }

    esp_task_wdt_reset();
    printf("🎊 Desaceleração trapezoidal LINEAR concluída: %u/8192\n", normalizada_final);
}

void executar_desaceleracao_trapezoidal_rapida()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando desaceleração trapezoidal RÁPIDA\n");

    rampa_desaceleracao_trapezoidal(50, 10, 100);
}

void executar_desaceleracao_trapezoidal_muito_rapida()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando desaceleração trapezoidal MUITO RÁPIDA\n");

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    rampa_desaceleracao_trapezoidal(1024, 100, 1000);

    // rampa_desaceleracao_trapezoidal(velocidade_atual, velocidade_atual / 2, 1000);
}

void executar_desaceleracao_para_zero_e_inverter(uint16_t vel_normalizada_atual, direcao_motor_t nova_direcao)
{
    if (!motor_ligado) {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    // CORREÇÃO: Resetar amortecedor no início da transição
    resetar_amortecedor(vel_normalizada_atual);
    
    // Configurar amortecedor extra-suave para transição
    float amortecedor_original = amortecedor_factor;
    configurar_amortecedor(0.15f); // Muito suave para transição

    uint32_t pps_atual;
    direcao_motor_t dir_atual;
    converter_normalizada_para_pps(vel_normalizada_atual, &pps_atual, &dir_atual);
    
    printf("🔄 Invertendo direção - PPS atual: %u, Normalizada: %u, Direção: %s\n", 
           pps_atual, vel_normalizada_atual,
           dir_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    // Calcular intensidade atual
    uint16_t intensidade;
    if (vel_normalizada_atual < VELOCIDADE_NEUTRA) {
        intensidade = VELOCIDADE_NEUTRA - vel_normalizada_atual;
    } else if (vel_normalizada_atual > VELOCIDADE_NEUTRA) {
        intensidade = vel_normalizada_atual - VELOCIDADE_NEUTRA;
    } else {
        intensidade = 0;
    }

    // Calcular nova velocidade mantendo intensidade
    uint16_t nova_vel_normalizada;
    if (nova_direcao == DIRECAO_HORARIA) {
        nova_vel_normalizada = VELOCIDADE_NEUTRA - intensidade;
        if (nova_vel_normalizada < VELOCIDADE_MINIMA) {
            nova_vel_normalizada = VELOCIDADE_MINIMA;
        }
    } else {
        nova_vel_normalizada = VELOCIDADE_NEUTRA + intensidade;
        if (nova_vel_normalizada > VELOCIDADE_MAXIMA) {
            nova_vel_normalizada = VELOCIDADE_MAXIMA;
        }
    }

    printf("🎯 Transição: %u (%s) -> PARADA -> %u (%s)\n", 
           vel_normalizada_atual,
           dir_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
           nova_vel_normalizada,
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    // FASE 1: Desacelerar até PARADA COMPLETA com amortecedor
    printf("📉 Fase 1: Desacelerando até parada completa...\n");
    rampa_desaceleracao_trapezoidal(vel_normalizada_atual, VELOCIDADE_NEUTRA, 1000);

    // Garantir parada completa
    setar_velocidade_normalizada(VELOCIDADE_NEUTRA);
    vTaskDelay(pdMS_TO_TICKS(150));
    printf("🛑 Motor parado completamente\n");

    // FASE 2: Alterar direção física
    printf("🔀 Alterando direção física...\n");
    gpio_set_level(DIR_PIN, nova_direcao);
    direcao_atual = nova_direcao;
    vTaskDelay(pdMS_TO_TICKS(80));
    printf("✅ Direção alterada para: %s\n", 
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    // CORREÇÃO: Resetar amortecedor para a nova direção
    resetar_amortecedor(VELOCIDADE_NEUTRA);

    // FASE 3: Acelerar suavemente com amortecedor
    printf("📈 Fase 3: Acelerando suavemente na nova direção...\n");
    
    // CORREÇÃO: Usar rampa mais longa e com amortecedor ativo
    rampa_aceleracao_trapezoidal(VELOCIDADE_NEUTRA, nova_vel_normalizada, 1200);

    // CORREÇÃO: Restaurar amortecedor original
    configurar_amortecedor(amortecedor_original);
    
    printf("🎊 Inversão de direção SUAVE concluída!\n");
}