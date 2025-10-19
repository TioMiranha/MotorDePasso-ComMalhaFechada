#include "../include/bicaInclude.h"

void rampa_aceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms)
{
    printf("🎯 Iniciando rampa de ACELERAÇÃO INTELIGENTE: %u -> %u em %u ms\n",
           normalizada_inicial, normalizada_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    if(normalizada_inicial == normalizada_final) return;

    // 🟢 CORREÇÃO: Validação considerando ambas as direções
    if (normalizada_inicial > VELOCIDADE_MAXIMA) normalizada_inicial = VELOCIDADE_MAXIMA;
    if (normalizada_final > VELOCIDADE_MAXIMA) normalizada_final = VELOCIDADE_MAXIMA;

    // 🟢 CORREÇÃO CRÍTICA: Verificar se as direções são diferentes
    direcao_motor_t dir_inicial, dir_final;
    uint32_t pps_temp;
    
    converter_normalizada_para_pps(normalizada_inicial, &pps_temp, &dir_inicial);
    converter_normalizada_para_pps(normalizada_final, &pps_temp, &dir_final);

    // 🟢 CORREÇÃO: Se as direções são diferentes, NÃO converter para desaceleração
    // Em vez disso, forçar a aceleração dentro da mesma direção
    if (dir_inicial != dir_final) {
        printf("⚠️  ALERTA: Direções diferentes detectadas! Forçando aceleração na direção atual.\n");
        printf("   Direção atual: %s, Direção solicitada: %s\n",
               dir_inicial == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
               dir_final == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
        
        // 🟢 CORREÇÃO: Ajustar a velocidade final para a mesma direção da inicial
        if (dir_inicial == DIRECAO_HORARIA) {
            // Manter na direção horária, garantir que está abaixo de 4096
            if (normalizada_final >= VELOCIDADE_NEUTRA) {
                normalizada_final = VELOCIDADE_NEUTRA - 1;
                printf("   Ajustando velocidade final para: %u (HORÁRIA)\n", normalizada_final);
            }
        } else {
            // Manter na direção anti-horária, garantir que está acima de 4096
            if (normalizada_final <= VELOCIDADE_NEUTRA) {
                normalizada_final = VELOCIDADE_NEUTRA + 1;
                printf("   Ajustando velocidade final para: %u (ANTI-HORÁRIA)\n", normalizada_final);
            }
        }
    }

    // 🟢 CORREÇÃO CRÍTICA: Verificação inteligente de aceleração (apenas para mesma direção)
    int comparacao = comparar_velocidades(normalizada_inicial, normalizada_final);
    
    if (comparacao >= 0 && dir_inicial == dir_final) {
        // Não é uma aceleração válida (apenas se estiver na mesma direção)
        printf("⚠️  AVISO: Para aceleração, a velocidade inicial deve ser MENOR que a final!\n");
        printf("   Inicial: %u (%s), Final: %u (%s)\n",
               normalizada_inicial,
               normalizada_inicial < VELOCIDADE_NEUTRA ? "HORÁRIA" : 
               normalizada_inicial > VELOCIDADE_NEUTRA ? "ANTI-HORÁRIA" : "NEUTRA",
               normalizada_final,
               normalizada_final < VELOCIDADE_NEUTRA ? "HORÁRIA" : 
               normalizada_final > VELOCIDADE_NEUTRA ? "ANTI-HORÁRIA" : "NEUTRA");
        
        printf("🔁 Convertendo para rampa de desaceleração...\n");
        rampa_desaceleracao_trapezoidal(normalizada_inicial, normalizada_final, duracao_ms);
        return;
    }

    // 🟢 CORREÇÃO: Salvar e configurar amortecedor baseado na magnitude
    float amortecedor_original = amortecedor_factor;
    
    uint16_t magnitude_final = obter_magnitude_velocidade(normalizada_final);
    float amortecedor_aceleracao = 0.12f + (magnitude_final * 0.08f / 4095); // 0.12 a 0.20
    if (amortecedor_aceleracao > 0.22f) amortecedor_aceleracao = 0.22f;
    
    configurar_amortecedor(amortecedor_aceleracao);

    // 🟢 CORREÇÃO: Resetar amortecedor no início
    resetar_amortecedor(normalizada_inicial);
    setar_velocidade_normalizada(normalizada_inicial);
    vTaskDelay(pdMS_TO_TICKS(20)); // Estabilização inicial

    printf("📊 Magnitude final: %u/4095, Amortecedor: %.3f\n", magnitude_final, amortecedor_aceleracao);

    // 🟢 CORREÇÃO: Cálculo adaptativo do perfil baseado na magnitude
    uint32_t tempo_aceleracao, tempo_constante, tempo_desaceleracao;
    
    if (magnitude_final > 3000) {
        // Alta velocidade: mais tempo para acelerar suavemente
        tempo_aceleracao = duracao_ms * 35 / 100;
        tempo_desaceleracao = tempo_aceleracao;
        tempo_constante = duracao_ms - tempo_aceleracao - tempo_desaceleracao;
    } else if (magnitude_final > 1500) {
        // Velocidade média: balanceado
        tempo_aceleracao = duracao_ms * 40 / 100;
        tempo_desaceleracao = tempo_aceleracao;
        tempo_constante = duracao_ms - tempo_aceleracao - tempo_desaceleracao;
    } else {
        // Baixa velocidade: menos tempo de aceleração
        tempo_aceleracao = duracao_ms * 45 / 100;
        tempo_desaceleracao = tempo_aceleracao;
        tempo_constante = duracao_ms - tempo_aceleracao - tempo_desaceleracao;
    }

    // 🟢 CORREÇÃO: Garantir tempo mínimo para cada fase
    if (tempo_constante < 25) {
        tempo_constante = 0;
        tempo_aceleracao = duracao_ms / 2;
        tempo_desaceleracao = duracao_ms - tempo_aceleracao;
        printf("📐 Perfil TRIANGULAR (aceleração: %ums, desaceleração: %ums)\n", 
               tempo_aceleracao, tempo_desaceleracao);
    } else {
        printf("📊 Perfil TRAPEZOIDAL (aceleração: %ums, constante: %ums, desaceleração: %ums)\n",
               tempo_aceleracao, tempo_constante, tempo_desaceleracao);
    }

    // 🟢 CORREÇÃO: Determinar velocidade_pico considerando direções
    uint16_t velocidade_pico = normalizada_final;

    printf("🔄 Fases da aceleração:\n");
    printf("   • Aceleração: %u ms (%u → %u)\n", tempo_aceleracao, normalizada_inicial, velocidade_pico);
    if (tempo_constante > 0)
        printf("   • Velocidade constante: %u ms (%u)\n", tempo_constante, velocidade_pico);
    printf("   • Desaceleração: %u ms (%u → %u)\n", tempo_desaceleracao, velocidade_pico, normalizada_final);

    uint32_t tempo_inicio = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t ultimo_watchdog_reset = tempo_inicio;

    // FASE 1: ACELERAÇÃO INTELIGENTE
    if (tempo_aceleracao > 0 && motor_ligado)
    {
        printf("🚀 Iniciando fase de ACELERAÇÃO INTELIGENTE\n");
        
        uint32_t inicio_aceleracao = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (uint32_t tempo_decorrido = 0; tempo_decorrido <= tempo_aceleracao && motor_ligado; tempo_decorrido += 10)
        {
            // Watchdog
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 25)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // 🟢 CORREÇÃO: Progresso com easing adaptativo
            float progresso = (float)tempo_decorrido / tempo_aceleracao;
            
            // 🟢 CORREÇÃO: Easing mais suave para altas velocidades
            float progresso_suavizado;
            if (magnitude_final > 2500) {
                // Alta velocidade: easing quadrático mais suave
                progresso_suavizado = progresso * progresso;
            } else {
                // Velocidade baixa/média: easing linear com suavização no início
                progresso_suavizado = (progresso < 0.5f) ? 
                    2.0f * progresso * progresso : 
                    1.0f - 2.0f * (1.0f - progresso) * (1.0f - progresso);
            }

            // 🟢 CORREÇÃO: Interpolação correta para ambas as direções
            uint16_t normalizada_atual;
            if (normalizada_inicial < VELOCIDADE_NEUTRA && normalizada_final < VELOCIDADE_NEUTRA) {
                // Ambos horários
                normalizada_atual = normalizada_inicial + 
                                  (uint16_t)(progresso_suavizado * (normalizada_final - normalizada_inicial));
            } else if (normalizada_inicial > VELOCIDADE_NEUTRA && normalizada_final > VELOCIDADE_NEUTRA) {
                // Ambos anti-horários
                normalizada_atual = normalizada_inicial + 
                                  (uint16_t)(progresso_suavizado * (normalizada_final - normalizada_inicial));
            } else {
                // 🟢 CORREÇÃO: Caso especial - direções diferentes (agora ajustado)
                // Usar magnitude para interpolação
                uint16_t mag_inicial = obter_magnitude_velocidade(normalizada_inicial);
                uint16_t mag_final = obter_magnitude_velocidade(normalizada_final);
                uint16_t mag_atual = mag_inicial + (uint16_t)(progresso_suavizado * (mag_final - mag_inicial));
                
                // 🟢 CORREÇÃO: Manter na direção inicial
                if (dir_inicial == DIRECAO_HORARIA) {
                    normalizada_atual = VELOCIDADE_NEUTRA - mag_atual;
                } else {
                    normalizada_atual = VELOCIDADE_NEUTRA + mag_atual;
                }
            }

            // Aplicar velocidade
            setar_velocidade_normalizada(normalizada_atual);

            vTaskDelay(pdMS_TO_TICKS(10)); // Intervalo constante
        }

        // 🟢 CORREÇÃO: Garantir valor final exato da fase
        setar_velocidade_normalizada(velocidade_pico);
        printf("✅ Aceleração inteligente concluída: %u\n", velocidade_pico);
    }

    // FASE 2: VELOCIDADE CONSTANTE
    if (tempo_constante > 0 && motor_ligado)
    {
        printf("⚡ Mantendo velocidade CONSTANTE: %u\n", velocidade_pico);
        
        setar_velocidade_normalizada(velocidade_pico);
        vTaskDelay(pdMS_TO_TICKS(30)); // Estabilização inicial
        
        uint32_t tempo_fim_constante = tempo_inicio + tempo_aceleracao + tempo_constante;
        uint32_t tempo_atual = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        while (tempo_atual < tempo_fim_constante && motor_ligado)
        {
            if (tempo_atual - ultimo_watchdog_reset > 25)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = tempo_atual;
            }
            
            // Manter velocidade constante
            setar_velocidade_normalizada(velocidade_pico);
            vTaskDelay(pdMS_TO_TICKS(20));
            
            tempo_atual = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
        printf("✅ Fase constante concluída\n");
    }

    // FASE 3: DESACELERAÇÃO FINAL
    if (tempo_desaceleracao > 0 && motor_ligado)
    {
        printf("🛑 Iniciando fase de DESACELERAÇÃO FINAL\n");
        
        uint32_t inicio_desaceleracao = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (uint32_t tempo_decorrido = 0; tempo_decorrido <= tempo_desaceleracao && motor_ligado; tempo_decorrido += 10)
        {
            // Watchdog
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 25)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // 🟢 CORREÇÃO: Progresso com easing para suavizar o final
            float progresso = (float)tempo_decorrido / tempo_desaceleracao;
            float progresso_suavizado = 1.0f - (1.0f - progresso) * (1.0f - progresso);

            // 🟢 CORREÇÃO: Interpolação correta para ambas as direções
            uint16_t normalizada_atual;
            if (velocidade_pico < VELOCIDADE_NEUTRA && normalizada_final < VELOCIDADE_NEUTRA) {
                // Ambos horários
                normalizada_atual = velocidade_pico - 
                                  (uint16_t)(progresso_suavizado * (velocidade_pico - normalizada_final));
            } else if (velocidade_pico > VELOCIDADE_NEUTRA && normalizada_final > VELOCIDADE_NEUTRA) {
                // Ambos anti-horários
                normalizada_atual = velocidade_pico - 
                                  (uint16_t)(progresso_suavizado * (velocidade_pico - normalizada_final));
            } else {
                // 🟢 CORREÇÃO: Caso especial - direções diferentes (agora ajustado)
                uint16_t mag_pico = obter_magnitude_velocidade(velocidade_pico);
                uint16_t mag_final = obter_magnitude_velocidade(normalizada_final);
                uint16_t mag_atual = mag_pico - (uint16_t)(progresso_suavizado * (mag_pico - mag_final));
                
                // 🟢 CORREÇÃO: Manter na direção inicial
                if (dir_inicial == DIRECAO_HORARIA) {
                    normalizada_atual = VELOCIDADE_NEUTRA - mag_atual;
                } else {
                    normalizada_atual = VELOCIDADE_NEUTRA + mag_atual;
                }
            }

            setar_velocidade_normalizada(normalizada_atual);
            
            vTaskDelay(pdMS_TO_TICKS(10)); // Intervalo constante
        }

        setar_velocidade_normalizada(normalizada_final);
        printf("✅ Desaceleração final concluída: %u\n", normalizada_final);
    }

    // 🟢 CORREÇÃO: Estabilização final adaptativa
    uint32_t pausa_estabilizacao = 25 + (magnitude_final * 15 / 4095);
    vTaskDelay(pdMS_TO_TICKS(pausa_estabilizacao));

    // 🟢 CORREÇÃO: Restaurar configuração original do amortecedor
    configurar_amortecedor(amortecedor_original);

    // Garantir valor final
    if (motor_ligado)
    {
        setar_velocidade_normalizada(normalizada_final);
    }

    esp_task_wdt_reset();
    printf("🎊 Aceleração inteligente concluída: %u/8192\n", normalizada_final);
}

void executar_rampa_trapezoidal_rapida()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando rampa trapezoidal RÁPIDA\n");
    // configurar_amortecedor(0.1f);
    rampa_aceleracao_trapezoidal(100, 1024, 1200);
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