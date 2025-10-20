#include "../include/bicaInclude.h"

void rampa_desaceleracao_trapezoidal(uint16_t normalizada_inicial, uint16_t normalizada_final, uint32_t duracao_ms)
{
    printf("🎯 Iniciando rampa de DESACELERAÇÃO INTELIGENTE: %u -> %u em %u ms\n",
           normalizada_inicial, normalizada_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    if (normalizada_inicial == normalizada_final)
        return;

    if (normalizada_inicial > VELOCIDADE_MAXIMA)
        normalizada_inicial = VELOCIDADE_MAXIMA;
    if (normalizada_final > VELOCIDADE_MAXIMA)
        normalizada_final = VELOCIDADE_MAXIMA;


    direcao_motor_t dir_inicial, dir_final;
    uint32_t pps_temp;

    converter_normalizada_para_pps(normalizada_inicial, &pps_temp, &dir_inicial);
    converter_normalizada_para_pps(normalizada_final, &pps_temp, &dir_final);

    if (dir_inicial != dir_final)
    {
        printf("⚠️  ALERTA: Direções diferentes detectadas! Forçando desaceleração na direção atual.\n");
        printf("   Direção atual: %s, Direção solicitada: %s\n",
               dir_inicial == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
               dir_final == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

        // 🟢 CORREÇÃO: Ajustar a velocidade final para a mesma direção da inicial
        if (dir_inicial == DIRECAO_HORARIA)
        {
            // Manter na direção horária, garantir que está abaixo de 4096
            if (normalizada_final >= VELOCIDADE_NEUTRA)
            {
                normalizada_final = VELOCIDADE_NEUTRA - 1;
                printf("   Ajustando velocidade final para: %u (HORÁRIA)\n", normalizada_final);
            }
        }
        else
        {
            // Manter na direção anti-horária, garantir que está acima de 4096
            if (normalizada_final <= VELOCIDADE_NEUTRA)
            {
                normalizada_final = VELOCIDADE_NEUTRA + 1;
                printf("   Ajustando velocidade final para: %u (ANTI-HORÁRIA)\n", normalizada_final);
            }
        }
    }

    int comparacao = comparar_velocidades(normalizada_inicial, normalizada_final);

    if (comparacao <= 0 && dir_inicial == dir_final)
    {
        printf("⚠️  AVISO: Para desaceleração, a velocidade inicial deve ser MAIOR que a final!\n");
        printf("   Inicial: %u (%s), Final: %u (%s)\n",
               normalizada_inicial,
               normalizada_inicial < VELOCIDADE_NEUTRA ? "HORÁRIA" : normalizada_inicial > VELOCIDADE_NEUTRA ? "ANTI-HORÁRIA"
                                                                                                             : "NEUTRA",
               normalizada_final,
               normalizada_final < VELOCIDADE_NEUTRA ? "HORÁRIA" : normalizada_final > VELOCIDADE_NEUTRA ? "ANTI-HORÁRIA"
                                                                                                         : "NEUTRA");

        printf("🔁 Convertendo para rampa de aceleração...\n");
        rampa_aceleracao_trapezoidal(normalizada_inicial, normalizada_final, duracao_ms);
        return;
    }

    float amortecedor_original = amortecedor_factor;

    uint16_t magnitude_inicial = obter_magnitude_velocidade(normalizada_inicial);
    float amortecedor_desaceleracao = 0.10f + (magnitude_inicial * 0.10f / 4095); // 0.10 a 0.20
    if (amortecedor_desaceleracao > 0.25f)
        amortecedor_desaceleracao = 0.25f;

    configurar_amortecedor(amortecedor_desaceleracao);

    // 🟢 CORREÇÃO: Resetar amortecedor no início
    resetar_amortecedor(normalizada_inicial);
    setar_velocidade_normalizada(normalizada_inicial);
    vTaskDelay(pdMS_TO_TICKS(20)); // Estabilização inicial

    printf("📊 Magnitude inicial: %u/4095, Amortecedor: %.3f\n", magnitude_inicial, amortecedor_desaceleracao);

    // 🟢 CORREÇÃO: Cálculo adaptativo do perfil baseado na magnitude
    uint32_t tempo_desaceleracao, tempo_constante;

    if (magnitude_inicial > 3000)
    {
        // Alta velocidade: mais tempo para desacelerar suavemente
        tempo_desaceleracao = duracao_ms * 70 / 100;
        tempo_constante = duracao_ms - tempo_desaceleracao;
    }
    else if (magnitude_inicial > 1500)
    {
        // Velocidade média: balanceado
        tempo_desaceleracao = duracao_ms * 60 / 100;
        tempo_constante = duracao_ms - tempo_desaceleracao;
    }
    else
    {
        // Baixa velocidade: menos tempo de desaceleração
        tempo_desaceleracao = duracao_ms * 50 / 100;
        tempo_constante = duracao_ms - tempo_desaceleracao;
    }

    // 🟢 CORREÇÃO: Garantir tempo mínimo para desaceleração
    if (tempo_desaceleracao < 80)
    {
        tempo_desaceleracao = duracao_ms * 80 / 100;
        tempo_constante = duracao_ms - tempo_desaceleracao;
    }

    if (tempo_constante < 25)
    {
        tempo_constante = 0;
        tempo_desaceleracao = duracao_ms;
        printf("📐 Perfil TRIANGULAR (desaceleração: %ums)\n", tempo_desaceleracao);
    }
    else
    {
        printf("📊 Perfil TRAPEZOIDAL (constante: %ums, desaceleração: %ums)\n",
               tempo_constante, tempo_desaceleracao);
    }

    printf("🔄 Fases da desaceleração:\n");
    if (tempo_constante > 0)
        printf("   • Velocidade constante: %u ms (%u)\n", tempo_constante, normalizada_inicial);
    printf("   • Desaceleração: %u ms (%u → %u)\n", tempo_desaceleracao, normalizada_inicial, normalizada_final);

    uint32_t tempo_inicio = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t ultimo_watchdog_reset = tempo_inicio;

    // FASE 1: VELOCIDADE CONSTANTE INICIAL (se houver)
    if (tempo_constante > 0 && motor_ligado)
    {
        printf("⚡ Mantendo velocidade constante inicial: %u\n", normalizada_inicial);

        uint32_t tempo_fim_constante = tempo_inicio + tempo_constante;
        uint32_t tempo_atual = xTaskGetTickCount() * portTICK_PERIOD_MS;

        while (tempo_atual < tempo_fim_constante && motor_ligado)
        {
            if (tempo_atual - ultimo_watchdog_reset > 25)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = tempo_atual;
            }

            // Manter velocidade constante
            setar_velocidade_normalizada(normalizada_inicial);
            vTaskDelay(pdMS_TO_TICKS(20));

            tempo_atual = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
        printf("✅ Fase constante inicial concluída\n");
    }

    // FASE 2: DESACELERAÇÃO INTELIGENTE
    if (tempo_desaceleracao > 0 && motor_ligado)
    {
        printf("🛑 Iniciando fase de DESACELERAÇÃO INTELIGENTE\n");

        uint32_t inicio_desaceleracao = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (uint32_t tempo_decorrido = 0; tempo_decorrido <= tempo_desaceleracao && motor_ligado; tempo_decorrido += 10)
        {
            // Watchdog
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ultimo_watchdog_reset > 25)
            {
                esp_task_wdt_reset();
                ultimo_watchdog_reset = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }

            // 🟢 CORREÇÃO: Progresso com easing adaptativo
            float progresso = (float)tempo_decorrido / tempo_desaceleracao;

            // 🟢 CORREÇÃO: Easing mais suave para altas velocidades
            float progresso_suavizado;
            if (magnitude_inicial > 2500)
            {
                // Alta velocidade: easing cúbico mais suave
                progresso_suavizado = progresso * progresso * progresso;
            }
            else
            {
                // Velocidade baixa/média: easing quadrático
                progresso_suavizado = progresso * progresso;
            }

            // 🟢 CORREÇÃO: Interpolação correta para ambas as direções
            uint16_t normalizada_atual;
            if (normalizada_inicial < VELOCIDADE_NEUTRA && normalizada_final < VELOCIDADE_NEUTRA)
            {
                // Ambos horários
                normalizada_atual = normalizada_inicial -
                                    (uint16_t)(progresso_suavizado * (normalizada_inicial - normalizada_final));
            }
            else if (normalizada_inicial > VELOCIDADE_NEUTRA && normalizada_final > VELOCIDADE_NEUTRA)
            {
                // Ambos anti-horários
                normalizada_atual = normalizada_inicial -
                                    (uint16_t)(progresso_suavizado * (normalizada_inicial - normalizada_final));
            }
            else
            {
                // 🟢 CORREÇÃO: Caso especial - direções diferentes (agora ajustado)
                // Usar magnitude para interpolação
                uint16_t mag_inicial = obter_magnitude_velocidade(normalizada_inicial);
                uint16_t mag_final = obter_magnitude_velocidade(normalizada_final);
                uint16_t mag_atual = mag_inicial - (uint16_t)(progresso_suavizado * (mag_inicial - mag_final));

                // 🟢 CORREÇÃO: Manter na direção inicial
                if (dir_inicial == DIRECAO_HORARIA)
                {
                    normalizada_atual = VELOCIDADE_NEUTRA - mag_atual;
                }
                else
                {
                    normalizada_atual = VELOCIDADE_NEUTRA + mag_atual;
                }
            }

            setar_velocidade_normalizada(normalizada_atual);

            vTaskDelay(pdMS_TO_TICKS(10)); // Intervalo constante
        }

        setar_velocidade_normalizada(normalizada_final);
        printf("✅ Desaceleração inteligente concluída: %u\n", normalizada_final);
    }

    uint32_t pausa_estabilizacao = 30 + (magnitude_inicial * 20 / 4095);
    vTaskDelay(pdMS_TO_TICKS(pausa_estabilizacao));

    configurar_amortecedor(amortecedor_original);

    if (motor_ligado)
    {
        setar_velocidade_normalizada(normalizada_final);
    }

    esp_task_wdt_reset();
    printf("🎊 Desaceleração inteligente concluída: %u/8192\n", normalizada_final);
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

void executar_desaceleracao_para_zero_e_inverter(uint32_t vel_atual, direcao_motor_t nova_direcao)
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    printf("🎯 Iniciando inversão suave: %u PPS (%s → %s)\n", 
           vel_atual,
           direcao_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    // Salvar configuração original
    float amortecedor_original = amortecedor_factor;

    // Garantir limites
    uint32_t velocidade_alvo = vel_atual;
    if (velocidade_alvo > PPS_MAXIMO) velocidade_alvo = PPS_MAXIMO;
    if (velocidade_alvo < PPS_MINIMO) velocidade_alvo = PPS_MINIMO;

    printf("🎯 Velocidade alvo processada: %u PPS\n", velocidade_alvo);

    // CORREÇÃO: Tempos adaptativos baseados na velocidade - MAIS RÁPIDOS
    uint32_t tempo_desaceleracao, tempo_aceleracao;
    
    if (velocidade_alvo < 500) {
        // Velocidade muito baixa: tempos curtos
        tempo_desaceleracao = 400;
        tempo_aceleracao = 300;
    } else if (velocidade_alvo < 2000) {
        // Velocidade baixa: tempos moderados
        tempo_desaceleracao = 600;
        tempo_aceleracao = 500;
    } else if (velocidade_alvo < 8000) {
        // Velocidade média: tempos balanceados
        tempo_desaceleracao = 800;
        tempo_aceleracao = 700;
    } else if (velocidade_alvo < 15000) {
        // Velocidade alta: tempos mais longos
        tempo_desaceleracao = 1000;
        tempo_aceleracao = 900;
    } else {
        // Velocidade muito alta: tempos conservadores
        tempo_desaceleracao = 1200;
        tempo_aceleracao = 1000;
    }

    printf("⏱️  Tempos: Desaceleração=%ums, Aceleração=%ums\n", tempo_desaceleracao, tempo_aceleracao);

    // FASE 1: DESACELERAÇÃO SUAVE
    printf("📉 Fase 1: Desaceleração suave...\n");

    uint32_t pps_atual = velocidade_alvo;
    
    // Amortecedor mais suave para desaceleração rápida
    configurar_amortecedor(0.12f);

    // Desacelerar suavemente até ZERO
    uint32_t inicio_desaceleracao = xTaskGetTickCount();
    uint32_t tempo_decorrido = 0;

    while (tempo_decorrido < tempo_desaceleracao && motor_ligado)
    {
        esp_task_wdt_reset();

        float progresso = (float)tempo_decorrido / tempo_desaceleracao;
        
        // CORREÇÃO: Curva mais linear para desaceleração mais rápida
        float progresso_suavizado = progresso; // Linear - mais rápido
        if (velocidade_alvo > 5000) {
            // Para velocidades altas, usar curva ligeiramente suavizada
            progresso_suavizado = 1.0f - powf(1.0f - progresso, 1.5f);
        }
        
        // Interpolar diretamente em PPS
        uint32_t pps_interpolado = (uint32_t)(pps_atual * (1.0f - progresso_suavizado));

        // Proteção para valores muito baixos
        if (pps_interpolado < 50 && progresso > 0.3f) {
            pps_interpolado = 0; // Corta mais rápido nos últimos 30%
        }

        alterar_velocidade(pps_interpolado);
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Reduzido para 10ms - mais rápido
        tempo_decorrido = (xTaskGetTickCount() - inicio_desaceleracao) * portTICK_PERIOD_MS;
    }

    // Garantir parada completa
    alterar_velocidade(0);
    
    // CORREÇÃO: Pausa de estabilização adaptativa
    uint32_t pausa_estabilizacao;
    if (velocidade_alvo < 1000) {
        pausa_estabilizacao = 150; // Mais rápido para baixas velocidades
    } else if (velocidade_alvo < 5000) {
        pausa_estabilizacao = 200;
    } else {
        pausa_estabilizacao = 250; // Mais tempo para altas velocidades
    }
    vTaskDelay(pdMS_TO_TICKS(pausa_estabilizacao));
    
    printf("🛑 Parada completa (%ums estabilização)\n", pausa_estabilizacao);

    // FASE 2: MUDANÇA DE DIREÇÃO
    printf("🔀 Alterando direção física...\n");

    // Garantir que estamos parados
    alterar_velocidade(100);
    
    // CORREÇÃO: Pausa reduzida para mudança de direção
    vTaskDelay(pdMS_TO_TICKS(80));

    // Mudança física da direção
    gpio_set_level(DIR_PIN, nova_direcao);
    direcao_atual = nova_direcao;
    
    vTaskDelay(pdMS_TO_TICKS(80));
    printf("✅ Direção alterada para: %s\n", 
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    // FASE 3: ACELERAÇÃO SUAVE
    printf("📈 Fase 3: Aceleração suave...\n");

    // Aceleração progressiva começando de ZERO
    uint32_t inicio_aceleracao = xTaskGetTickCount();
    tempo_decorrido = 0;

    while (tempo_decorrido < tempo_aceleracao && motor_ligado)
    {
        esp_task_wdt_reset();

        float progresso = (float)tempo_decorrido / tempo_aceleracao;
        
        // CORREÇÃO: Curva adaptativa para aceleração
        float progresso_suavizado;
        if (velocidade_alvo < 1000) {
            progresso_suavizado = progresso; // Linear para baixas velocidades
        } else if (velocidade_alvo < 5000) {
            progresso_suavizado = powf(progresso, 1.8f); // Quase linear
        } else {
            progresso_suavizado = powf(progresso, 2.2f); // Mais suave para altas velocidades
        }
        
        // Interpolar diretamente em PPS de 0 até velocidade_alvo
        uint32_t pps_interpolado = (uint32_t)(velocidade_alvo * progresso_suavizado);

        // Proteção para evitar valores muito baixos que podem travar o motor
        if (pps_interpolado < PPS_MINIMO && pps_interpolado > 0) {
            pps_interpolado = PPS_MINIMO;
        }

        // Proteção para altas velocidades
        if (pps_interpolado > PPS_MAXIMO) {
            pps_interpolado = PPS_MAXIMO;
        }

        alterar_velocidade(pps_interpolado);
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Reduzido para 10ms - mais rápido
        tempo_decorrido = (xTaskGetTickCount() - inicio_aceleracao) * portTICK_PERIOD_MS;
    }

    // Garantir velocidade final exata
    alterar_velocidade(velocidade_alvo);

    printf("📊 Velocidade final: %u PPS\n", velocidade_alvo);

    // Estabilização final reduzida
    vTaskDelay(pdMS_TO_TICKS(150));

    // Restaurar configuração original
    configurar_amortecedor(amortecedor_original);

    printf("🎊 INVERSÃO CONCLUÍDA! Direção: %s, Velocidade: %u PPS\n",
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
           velocidade_alvo);
}