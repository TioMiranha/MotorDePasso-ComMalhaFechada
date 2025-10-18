#include "../include/bicaInclude.h"

void rampa_aceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms)
{
    printf("🎯 Iniciando rampa trapezoidal LINEAR: %u -> %u em %u ms\n",
           normalizada_inicial, normalizada_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    // Aplica limites
    if (normalizada_inicial > VELOCIDADE_MAXIMA) normalizada_inicial = VELOCIDADE_MAXIMA;
    if (normalizada_final > VELOCIDADE_MAXIMA) normalizada_final = VELOCIDADE_MAXIMA;

    int32_t diferenca = (int32_t)normalizada_final - (int32_t)normalizada_inicial;
    if (diferenca == 0)
    {
        printf("⚠️  Velocidade inicial e final são iguais!\n");
        return;
    }

    // CORREÇÃO: Determinar velocidade de pico baseada na direção real
    uint16_t velocidade_pico;
    direcao_motor_t dir_inicial, dir_final;
    uint32_t pps_inicial, pps_final;
    
    converter_normalizada_para_pps(normalizada_inicial, &pps_inicial, &dir_inicial);
    converter_normalizada_para_pps(normalizada_final, &pps_final, &dir_final);

    // CORREÇÃO: Se mesma direção, velocidade_pico é a que tem maior PPS
    if (dir_inicial == dir_final) {
        // Determinação correta da velocidade_pico baseada em PPS
        if (pps_inicial > pps_final) {
            velocidade_pico = normalizada_inicial; // Desacelerando
        } else {
            velocidade_pico = normalizada_final;   // Acelerando
        }
    } else {
        // CORREÇÃO: Em caso de mudança de direção, usa rampa completa
        printf("🔄 ALERTA: Mudança de direção detectada! Executando rampa completa...\n");
        velocidade_pico = (pps_inicial > pps_final) ? normalizada_inicial : normalizada_final;
    }

    // CORREÇÃO: Percentuais fixos para rampa trapezoidal clássica
    uint32_t tempo_aceleracao, tempo_constante, tempo_desaceleracao;
    uint32_t percentual_aceleracao = 33;

    tempo_aceleracao = (duracao_ms * percentual_aceleracao) / 100;
    tempo_desaceleracao = tempo_aceleracao;
    tempo_constante = duracao_ms - tempo_aceleracao - tempo_desaceleracao;

    // CORREÇÃO: Garantir que a fase constante tenha pelo menos 10ms
    if (tempo_constante < 10) {
        tempo_constante = 10;
        tempo_aceleracao = (duracao_ms - tempo_constante) / 2;
        tempo_desaceleracao = duracao_ms - tempo_constante - tempo_aceleracao;
    }

    if (tempo_constante <= 0)
    {
        tempo_aceleracao = duracao_ms / 2;
        tempo_desaceleracao = duracao_ms - tempo_aceleracao;
        tempo_constante = 0;
        printf("📐 Perfil TRIANGULAR (aceleração: %ums, desaceleração: %ums)\n", 
               tempo_aceleracao, tempo_desaceleracao);
    }
    else
    {
        printf("📊 Perfil TRAPEZOIDAL (aceleração: %u%%, constante: %u%%, desaceleração: %u%%)\n", 
               percentual_aceleracao, 100 - 2 * percentual_aceleracao, percentual_aceleracao);
    }

    printf("🔄 Fases da rampa:\n");
    printf("   • Aceleração: %u ms (%u → %u)\n", tempo_aceleracao, normalizada_inicial, velocidade_pico);
    if (tempo_constante > 0) printf("   • Constante: %u ms (%u)\n", tempo_constante, velocidade_pico);
    printf("   • Desaceleração: %u ms (%u → %u)\n", tempo_desaceleracao, velocidade_pico, normalizada_final);

    uint32_t tempo_inicio = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t ultimo_watchdog_reset = tempo_inicio;

    // CORREÇÃO: Resetar watchdog antes de iniciar a rampa
    esp_task_wdt_reset();

    // Fase de aceleração
    if (tempo_aceleracao > 0 && motor_ligado)
    {
        printf("🚀 Iniciando fase de ACELERAÇÃO LINEAR\n");
        uint32_t tempo_atual = 0;
        
        while (tempo_atual < tempo_aceleracao && motor_ligado)
        {
            // CORREÇÃO CRÍTICA: Resetar watchdog a cada 20ms
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 20)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // Progresso LINEAR
            float progresso = (float)tempo_atual / tempo_aceleracao;

            // Interpolação linear direta
            uint16_t normalizada_atual = normalizada_inicial + 
                                       (uint16_t)(progresso * (velocidade_pico - normalizada_inicial));

            setar_velocidade_normalizada(normalizada_atual);

            tempo_atual = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tempo_inicio;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Garantir que chegou no valor exato
        setar_velocidade_normalizada(velocidade_pico);
        printf("✅ Aceleração linear concluída: %u\n", velocidade_pico);
    }

    // Fase constante
    if (tempo_constante > 0 && motor_ligado)
    {
        printf("⚡ Mantendo velocidade CONSTANTE: %u\n", velocidade_pico);
        
        // Garantir que está exatamente na velocidade_pico
        setar_velocidade_normalizada(velocidade_pico);
        
        // Pequena pausa para estabilização
        vTaskDelay(pdMS_TO_TICKS(5));
        
        // Manter velocidade constante pelo tempo determinado
        uint32_t tempo_fim_constante = tempo_inicio + tempo_aceleracao + tempo_constante;
        
        while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < tempo_fim_constante && motor_ligado)
        {
            // CORREÇÃO CRÍTICA: Resetar watchdog periodicamente
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 20)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }
            
            // Reforçar velocidade constante periodicamente
            setar_velocidade_normalizada(velocidade_pico);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        printf("✅ Fase constante concluída\n");
    }

    // Fase de desaceleração
    if (tempo_desaceleracao > 0 && motor_ligado)
    {
        printf("🛑 Iniciando fase de DESACELERAÇÃO LINEAR\n");
        uint32_t tempo_inicio_desac = tempo_inicio + tempo_aceleracao + tempo_constante;
        uint32_t tempo_atual = 0;

        while (tempo_atual < tempo_desaceleracao && motor_ligado)
        {
            // CORREÇÃO CRÍTICA: Resetar watchdog a cada 20ms
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 20)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // Progresso LINEAR
            float progresso = (float)tempo_atual / tempo_desaceleracao;

            // Interpolação linear direta
            uint16_t normalizada_atual;
            if (velocidade_pico > normalizada_final) {
                normalizada_atual = velocidade_pico - (uint16_t)(progresso * (velocidade_pico - normalizada_final));
            } else {
                normalizada_atual = velocidade_pico + (uint16_t)(progresso * (normalizada_final - velocidade_pico));
            }

            setar_velocidade_normalizada(normalizada_atual);

            tempo_atual = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tempo_inicio_desac;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Garantir valor final exato
        setar_velocidade_normalizada(normalizada_final);
        printf("✅ Desaceleração linear concluída: %u\n", normalizada_final);
    }

    // Garantir valor final exato
    if (motor_ligado)
    {
        setar_velocidade_normalizada(normalizada_final);
    }

    // CORREÇÃO: Reset final do watchdog
    esp_task_wdt_reset();
    printf("🎊 Rampa trapezoidal LINEAR concluída: %u/8192\n", normalizada_final);
}

void rampa_aceleracao_trapezoidal_avancada(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms, uint32_t aceleracao_max_pps_s)
{
    printf("🎯 Iniciando rampa TRAPEZOIDAL AVANÇADA: %u -> %u PPS em %u ms\n", pps_inicial, pps_final, duracao_ms);
    printf("⚡ Aceleração máxima: %u PPS/s\n", aceleracao_max_pps_s);

    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro!\n");
        return;
    }

    // Garantir valores válidos
    if (pps_inicial <= 0)
        pps_inicial = 0;
    if (pps_final <= 0)
        pps_final = 0;
    if (pps_inicial > 50000)
        pps_inicial = 50000;
    if (pps_final > 50000)
        pps_final = 50000;

    int32_t diferenca_velocidade = (int32_t)pps_final - (int32_t)pps_inicial;

    if (diferenca_velocidade == 0)
    {
        printf("Velocidade inicial e final são iguais!\n");
        return;
    }

    uint32_t aceleracao_necessaria = abs(diferenca_velocidade) * 1000 / duracao_ms;

    uint32_t aceleracao_usada = (aceleracao_necessaria < aceleracao_max_pps_s) ? aceleracao_necessaria : aceleracao_max_pps_s;

    uint32_t tempo_acel_desac = abs(diferenca_velocidade) * 1000 / aceleracao_usada;

    if (tempo_acel_desac * 2 > duracao_ms * 0.8)
    {
        tempo_acel_desac = (duracao_ms * 0.8) / 2;
        printf("🔄 Ajuste: tempo de aceleração otimizado para %u ms\n", tempo_acel_desac);
    }

    uint32_t tempo_constante = duracao_ms - (2 * tempo_acel_desac);
    uint32_t velocidade_maxima;

    if (diferenca_velocidade > 0)
    {
        // ACELERAÇÃO
        velocidade_maxima = pps_inicial + (aceleracao_usada * tempo_acel_desac) / 1000;
        if (velocidade_maxima > pps_final)
            velocidade_maxima = pps_final;
    }
    else
    {
        // DESACELERAÇÃO
        velocidade_maxima = pps_inicial;
        uint32_t nova_inicial = pps_final + (aceleracao_usada * tempo_acel_desac) / 1000;
        if (nova_inicial < pps_inicial)
            pps_inicial = nova_inicial;
    }

    printf("📊 Perfil calculado:\n");
    printf("   • Aceleração/Desaceleração: %u ms cada\n", tempo_acel_desac);
    printf("   • Velocidade constante: %u ms\n", tempo_constante);
    printf("   • Velocidade máxima: %u PPS\n", velocidade_maxima);
    printf("   • Aceleração usada: %u PPS/s\n", aceleracao_usada);

    // 🎯 EXECUÇÃO OTIMIZADA
    if (diferenca_velocidade > 0)
    {
        // Caso aceleração
        rampa_aceleracao_trapezoidal(pps_inicial, velocidade_maxima, tempo_acel_desac);

        if (tempo_constante > 0 && motor_ligado)
        {
            alterar_velocidade(velocidade_maxima);
            printf("   ⏱️  Fase constante: %u ms\n", tempo_constante);
            vTaskDelay(pdMS_TO_TICKS(tempo_constante));
        }

        rampa_aceleracao_trapezoidal(velocidade_maxima, pps_final, tempo_acel_desac);
    }
    else
    {
        // Caso desaceleração
        rampa_aceleracao_trapezoidal(pps_inicial, velocidade_maxima, tempo_acel_desac);

        if (tempo_constante > 0 && motor_ligado)
        {
            alterar_velocidade(velocidade_maxima);
            printf("   ⏱️  Fase constante: %u ms\n", tempo_constante);
            vTaskDelay(pdMS_TO_TICKS(tempo_constante));
        }

        rampa_aceleracao_trapezoidal(velocidade_maxima, pps_final, tempo_acel_desac);
    }

    printf("✅ Rampa trapezoidal avançada concluída: %u PPS\n", pps_final);
}

void executar_rampa_trapezoidal_rapida()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando rampa trapezoidal RÁPIDA\n");
    //configurar_amortecedor(0.1f);
    rampa_aceleracao_trapezoidal(500, 1024, 1200);
}

void executar_rampa_trapezoidal_suave()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando rampa trapezoidal SUAVE\n");
    rampa_aceleracao_trapezoidal(20, 100, 3000); 
}

void executar_rampa_trapezoidal_avancada_rapida()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando rampa trapezoidal AVANÇADA RÁPIDA\n");
    rampa_aceleracao_trapezoidal_avancada(20, 49, 2000, 15000); // Aceleração máxima de 15000 PPS/s
}

void executar_rampa_trapezoidal_avancada_rapida_em_transicao()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando rampa trapezoidal AVANÇADA RÁPIDA\n");
}