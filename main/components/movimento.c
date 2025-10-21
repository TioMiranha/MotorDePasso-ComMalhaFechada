#include "../include/bicaInclude.h"

void tarefa_movimento_controlado(void *param)
{
    uint32_t distancia_passos = ((uint32_t *)param)[0];
    uint32_t velocidade_maxima = ((uint32_t *)param)[1];
    uint32_t tempo_total_ms = ((uint32_t *)param)[2];
    int direcao = ((uint32_t *)param)[3]; // 0=frente, 1=trás

    free(param); // Libera a memória alocada

    printf("Iniciando movimento %s: %u passos @ %u PPS em %u ms\n",
           direcao == 0 ? "FRENTE" : "TRÁS",
           distancia_passos, velocidade_maxima, tempo_total_ms);

    // 1. Configurar direção
    gpio_set_level(DIR_PIN, direcao);
    direcao_atual = direcao;
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2. Calcular parâmetros SEGUROS
    uint32_t passos_aceleracao = distancia_passos * 0.3;
    uint32_t passos_constante = distancia_passos * 0.4;
    uint32_t passos_desaceleracao = distancia_passos - passos_aceleracao - passos_constante;

    if (passos_aceleracao + passos_constante > distancia_passos)
    {
        passos_aceleracao = distancia_passos * 0.5;
        passos_constante = 0;
        passos_desaceleracao = distancia_passos - passos_aceleracao;
    }

    uint32_t tempo_aceleracao = tempo_total_ms * 0.3;
    uint32_t tempo_constante = tempo_total_ms * 0.4;
    uint32_t tempo_desaceleracao = tempo_total_ms - tempo_aceleracao - tempo_constante;

    printf("📊 Perfil: Acel=%up/%ums, Const=%up, Desacel=%up/%ums\n",
           passos_aceleracao, tempo_aceleracao, passos_constante,
           passos_desaceleracao, tempo_desaceleracao);

    gpio_set_level(ENABLE_PIN, 0);

    if (passos_aceleracao > 0 && movimento_em_andamento)
    {
        printf("   ↗ Acelerando...\n");

        for (uint32_t i = 0; i < passos_aceleracao && movimento_em_andamento; i++)
        {
            // Cálculo progressivo da velocidade
            float progresso = (float)i / (float)passos_aceleracao;
            uint32_t velocidade_atual = 500 + (uint32_t)(progresso * (velocidade_maxima - 500));

            // 🔒 CORREÇÃO: Garantir velocidade válida
            if (velocidade_atual < 10)
                velocidade_atual = 10;
            if (velocidade_atual > 50000)
                velocidade_atual = 50000;

            uint32_t periodo_ticks = 1000000 / velocidade_atual;
            if (periodo_ticks < 20)
                periodo_ticks = 20; // Mínimo 20µs

            // Gerar pulso
            rmt_item32_t pulso = {
                .duration0 = periodo_ticks / 4,
                .level0 = 1,
                .duration1 = periodo_ticks - (periodo_ticks / 4),
                .level1 = 0};

            ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, &pulso, 1, false));

            // 🔒 CORREÇÃO: Delay mais inteligente
            uint32_t delay_us = periodo_ticks;
            if (delay_us < 1000)
            {
                vTaskDelay(1); // Mínimo 1ms
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
            }

            // Feedback a cada 10%
            if (i % (passos_aceleracao / 10) == 0)
            {
                printf("     Passo %u/%u - %u PPS\n", i, passos_aceleracao, velocidade_atual);
            }
        }
    }

    // 5. FASE 2: VELOCIDADE CONSTANTE
    if (passos_constante > 0 && movimento_em_andamento)
    {
        printf("   ➡ Velocidade constante: %u PPS\n", velocidade_maxima);

        uint32_t periodo_ticks = 1000000 / velocidade_maxima;
        if (periodo_ticks < 20)
            periodo_ticks = 20;

        for (uint32_t i = 0; i < passos_constante && movimento_em_andamento; i++)
        {
            rmt_item32_t pulso = {
                .duration0 = periodo_ticks / 4,
                .level0 = 1,
                .duration1 = periodo_ticks - (periodo_ticks / 4),
                .level1 = 0};

            ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, &pulso, 1, false));

            uint32_t delay_us = periodo_ticks;
            if (delay_us < 1000)
            {
                vTaskDelay(1);
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
            }

            if (i % (passos_constante / 10) == 0)
            {
                printf("     Passo %u/%u\n", i, passos_constante);
            }
        }
    }

    // 6. FASE 3: DESACELERAÇÃO
    if (passos_desaceleracao > 0 && movimento_em_andamento)
    {
        printf("   ↘ Desacelerando...\n");

        for (uint32_t i = 0; i < passos_desaceleracao && movimento_em_andamento; i++)
        {
            float progresso = (float)i / (float)passos_desaceleracao;
            uint32_t velocidade_atual = velocidade_maxima - (uint32_t)(progresso * (velocidade_maxima - 500));

            // 🔒 CORREÇÃO: Garantir velocidade válida
            if (velocidade_atual < 10)
                velocidade_atual = 10;
            if (velocidade_atual > 50000)
                velocidade_atual = 50000;

            uint32_t periodo_ticks = 1000000 / velocidade_atual;
            if (periodo_ticks < 20)
                periodo_ticks = 20;

            rmt_item32_t pulso = {
                .duration0 = periodo_ticks / 4,
                .level0 = 1,
                .duration1 = periodo_ticks - (periodo_ticks / 4),
                .level1 = 0};

            ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, &pulso, 1, false));

            uint32_t delay_us = periodo_ticks;
            if (delay_us < 1000)
            {
                vTaskDelay(1);
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
            }

            if (i % (passos_desaceleracao / 10) == 0)
            {
                printf("     Passo %u/%u - %u PPS\n", i, passos_desaceleracao, velocidade_atual);
            }
        }
    }

    // 7. FINALIZAR
    if (movimento_em_andamento)
    {
        printf("✅ Movimento %s concluído: %u passos\n",
               direcao == 0 ? "FRENTE" : "TRÁS", distancia_passos);
    }
    else
    {
        printf("Movimento interrompido\n");
    }

    // Limpeza final
    movimento_em_andamento = 0;
    tarefa_movimento = NULL;
    vTaskDelete(NULL);
}

void mover_frente(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms)
{
    if (motor_ligado)
    {
        printf("Pare a rotação contínua primeiro! (Opção 2)\n");
        return;
    }

    if (movimento_em_andamento)
    {
        printf("Já existe um movimento em andamento!\n");
        return;
    }

    if (tarefa_movimento != NULL)
    {
        printf("Tarefa de movimento já existe!\n");
        return;
    }

    if (distancia_passos == 0 || velocidade_maxima == 0 || tempo_total_ms == 0)
    {
        printf("Parâmetros inválidos!\n");
        return;
    }

    if (velocidade_maxima > 50000)
    {
        printf("Velocidade limitada a 50000 PPS\n");
        velocidade_maxima = 50000;
    }

    if (xSemaphoreTake(xMutexMovimento, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        printf("Timeout ao acessar controle de movimento\n");
        return;
    }

    movimento_em_andamento = 1;
    xSemaphoreGive(xMutexMovimento);

    uint32_t *parametros = malloc(4 * sizeof(uint32_t));
    if (parametros == NULL)
    {
        printf("❌ Erro ao alocar memória para movimento!\n");
        movimento_em_andamento = 0;
        return;
    }

    parametros[0] = distancia_passos;
    parametros[1] = velocidade_maxima;
    parametros[2] = tempo_total_ms;
    parametros[3] = DIRECAO_HORARIA;

    if (xTaskCreate(tarefa_movimento_controlado, "MovimentoTask", 4096, parametros, 4, &tarefa_movimento) != pdPASS)
    {
        printf("Erro ao criar tarefa de movimento!\n");
        movimento_em_andamento = 0;
        free(parametros);
        return;
    }

    printf("🔼 Movimento para FRENTE iniciado em background\n");
}

void mover_tras(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms)
{
    if (motor_ligado)
    {
        printf("Pare a rotação contínua primeiro! (Opção 2)\n");
        return;
    }

    if (movimento_em_andamento)
    {
        printf("Já existe um movimento em andamento!\n");
        return;
    }

    if (tarefa_movimento != NULL)
    {
        printf("Tarefa de movimento já existe!\n");
        return;
    }

    if (distancia_passos == 0 || velocidade_maxima == 0 || tempo_total_ms == 0)
    {
        printf("Parâmetros inválidos!\n");
        return;
    }

    if (velocidade_maxima > 50000)
    {
        velocidade_maxima = 50000;
    }

    if (xSemaphoreTake(xMutexMovimento, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        printf("Timeout ao acessar controle de movimento\n");
        return;
    }

    movimento_em_andamento = 1;
    xSemaphoreGive(xMutexMovimento);

    uint32_t *parametros = malloc(4 * sizeof(uint32_t));
    if (parametros == NULL)
    {
        printf("Erro ao alocar memória para movimento!\n");
        movimento_em_andamento = 0;
        return;
    }

    parametros[0] = distancia_passos;
    parametros[1] = velocidade_maxima;
    parametros[2] = tempo_total_ms;
    parametros[3] = DIRECAO_ANTI_HORARIA;

    if (xTaskCreate(tarefa_movimento_controlado, "MovimentoTask", 4096, parametros, 4, &tarefa_movimento) != pdPASS)
    {
        printf("Erro ao criar tarefa de movimento!\n");
        movimento_em_andamento = 0;
        free(parametros);
        return;
    }

    printf("🔽 Movimento para TRÁS iniciado em background\n");
}

void parar_movimento()
{
    if (xSemaphoreTake(xMutexMovimento, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        movimento_em_andamento = 0;
        xSemaphoreGive(xMutexMovimento);
        printf("🛑 Movimento interrompido\n");

        // Pequeno delay para garantir que a tarefa veja a flag
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    else
    {
        printf("❌ Erro ao parar movimento\n");
    }
}

void executar_mover_frente_rapido()
{
    printf("🚀 Movimento RÁPIDO para FRENTE\n");
    mover_frente(800, 2000, 1500); 
}

void executar_mover_frente_devagar()
{
    printf("🐢 Movimento SUAVE para FRENTE\n");
    mover_frente(400, 1000, 2000);
}

void executar_mover_tras_rapido()
{
    printf("🚀 Movimento RÁPIDO para TRÁS\n");
    mover_tras(800, 2000, 1500);
}

void executar_mover_tras_devagar()
{
    printf("🐢 Movimento SUAVE para TRÁS\n");
    mover_tras(400, 1000, 2000);
}

void movimento_continuo(int direcao)
{
    // Verifica se o home foi realizado para movimentos normais
    if (direcao != 0 && direcao != 1 && !home_finalizado_com_sucesso()) {
        printf("⚠️  Execute o HOME primeiro antes de movimentos normais\n");
        return;
    }

    if (motor_ligado && direcao_atual == direcao)
    {
        printf("ℹ️  Motor já está movendo na mesma direção\n");
        return;
    }

    if (motor_ligado)
    {
        printf("🔄 Alterando direção...\n");
        parar_rotacao();
        vTaskDelay(pdMS_TO_TICKS(100)); // Aumentei para garantir parada completa
    }

    gpio_set_level(DIR_PIN, direcao);
    direcao_atual = direcao;

    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        // Velocidade padrão para operação normal
        velocidade_pps = 2000;
        xSemaphoreGive(xMutexVelocidade);
    }

    motor_ligado = 1;

    if (tarefa_motor == NULL)
    {
        xTaskCreate(tarefa_girar_motor, "MotorTask", 4096, NULL, 3, &tarefa_motor);
    }

    const char* direcao_str = (direcao == DIRECAO_HORARIA) ? "▶ FRENTE" : "◀ TRÁS";
    printf("%s: MOVIMENTO CONTÍNUO a %u PPS\n", direcao_str, velocidade_pps);
}

// Nova função para home com velocidade controlada
void movimento_continuo_home(int direcao, uint32_t velocidade)
{
    printf("🏠 MOVIMENTO HOME: Direção %s a %u PPS\n", 
           (direcao == DIRECAO_HORARIA) ? "HORÁRIA" : "ANTI-HORÁRIA", 
           velocidade);

    if (motor_ligado && direcao_atual == direcao)
    {
        // Apenas atualiza a velocidade se já está na mesma direção
        alterar_velocidade_suave(velocidade);
        return;
    }

    if (motor_ligado)
    {
        parar_rotacao();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    gpio_set_level(DIR_PIN, direcao);
    direcao_atual = direcao;

    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_pps = velocidade;
        xSemaphoreGive(xMutexVelocidade);
    }

    motor_ligado = 1;

    if (tarefa_motor == NULL)
    {
        xTaskCreate(tarefa_girar_motor, "MotorTask", 4096, NULL, 3, &tarefa_motor);
    }
}