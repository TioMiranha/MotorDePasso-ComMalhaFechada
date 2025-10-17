#include "../include/bicaInclude.h"

void rampa_desaceleracao_trapezoidal(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms)
{
    printf("Iniciando rampa TRAPEZOIDAL DE DESACELERAÇÃO COM AMORTECEDOR: %u -> %u PPS em %u ms\n", pps_inicial, pps_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
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

    if (pps_inicial <= pps_final)
    {
        printf("⚠️  Para desaceleração, velocidade inicial deve ser MAIOR que final!\n");
        return;
    }

    ultimo_pps = pps_inicial;

    uint32_t velocidade_minima = pps_final;
    uint32_t tempo_desaceleracao1, tempo_constante, tempo_desaceleracao2;

    int32_t diferenca = (int32_t)pps_inicial - (int32_t)pps_final;

    if (abs(diferenca) < 1000)
    {
        velocidade_minima = pps_final;
        tempo_desaceleracao1 = duracao_ms / 2;
        tempo_constante = 0;
        tempo_desaceleracao2 = duracao_ms - tempo_desaceleracao1;
        printf("Perfil TRIANGULAR (diferença pequena)\n");
    }
    else
    {
        velocidade_minima = pps_final;

        uint32_t percentual_desaceleracao = 35 + (abs(diferenca) / 400);
        if (percentual_desaceleracao > 60)
            percentual_desaceleracao = 60;

        tempo_desaceleracao1 = (duracao_ms * percentual_desaceleracao) / 100;
        tempo_desaceleracao2 = tempo_desaceleracao1;

        if ((tempo_desaceleracao1 + tempo_desaceleracao2) >= duracao_ms)
        {

            tempo_desaceleracao1 = duracao_ms / 2;
            tempo_desaceleracao2 = duracao_ms - tempo_desaceleracao1;
            tempo_constante = 0;
            printf("Perfil TRIANGULAR (ajustado - tempo insuficiente)\n");
        }
        else
        {
            tempo_constante = duracao_ms - tempo_desaceleracao1 - tempo_desaceleracao2;
            printf("Perfil TRAPEZOIDAL (desaceleração: %u%%)\n", percentual_desaceleracao);
        }
    }

    printf("🔄 Fases da desaceleração:\n");
    printf("   • Desaceleração inicial: %u ms (%u → %u PPS)\n", tempo_desaceleracao1, pps_inicial, velocidade_minima);
    if (tempo_constante > 0)
    {
        printf("   • Velocidade constante: %u ms (%u PPS)\n", tempo_constante, velocidade_minima);
    }
    else
    {
        printf("   • Velocidade constante: 0 ms (perfil triangular)\n");
    }
    printf("   • Desaceleração final: %u ms (%u → %u PPS)\n", tempo_desaceleracao2, velocidade_minima, pps_final);

    if (tempo_desaceleracao1 > 0 && motor_ligado)
    {
        uint32_t passos_desac1 = tempo_desaceleracao1 / 5;
        if (passos_desac1 < 3)
            passos_desac1 = 3;

        for (uint32_t passo = 0; passo < passos_desac1 && motor_ligado; passo++)
        {
            float progresso = (float)passo / (passos_desac1 - 1);
            float curva_suave = progresso * progresso * progresso;

            uint32_t pps_desejado = pps_inicial - (uint32_t)(curva_suave * (pps_inicial - velocidade_minima));

            uint32_t pps_amortecido = aplicar_amortecedor(pps_desejado);

            if (pps_amortecido <= 0)
                pps_amortecido = 0;
            if (pps_amortecido > 50000)
                pps_amortecido = 50000;

            alterar_velocidade(pps_amortecido);

            if (passo % 10 == 0 || passo == passos_desac1 - 1)
            {
                printf("   ↘ Desaceleração 1: %u/%u - %u PPS (desejado: %u)\n",
                       passo, passos_desac1, pps_amortecido, pps_desejado);
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (tempo_constante > 0 && motor_ligado)
    {
        uint32_t pps_amortecido = aplicar_amortecedor(velocidade_minima);
        alterar_velocidade(pps_amortecido);
        printf("Velocidade constante: %u PPS por %u ms\n", pps_amortecido, tempo_constante);

        uint32_t check_interval = 100;
        uint32_t checks = tempo_constante / check_interval;

        if (checks == 0)
            checks = 1;

        for (uint32_t i = 0; i < checks && motor_ligado; i++)
        {
            if (i % 2 == 0)
            {
                uint32_t pps_atual = aplicar_amortecedor(velocidade_minima);
                alterar_velocidade(pps_atual);
            }
            vTaskDelay(pdMS_TO_TICKS(check_interval));
        }
    }

    if (tempo_desaceleracao2 > 0 && motor_ligado)
    {
        uint32_t passos_desac2 = tempo_desaceleracao2 / 5;
        if (passos_desac2 < 3)
            passos_desac2 = 3;

        for (uint32_t passo = 0; passo < passos_desac2 && motor_ligado; passo++)
        {
            float progresso = (float)passo / (passos_desac2 - 1);
            float curva_extra_suave = 1.0 - ((1.0 - progresso) * (1.0 - progresso));

            uint32_t pps_desejado = velocidade_minima - (uint32_t)(curva_extra_suave * (velocidade_minima - pps_final));

            uint32_t pps_amortecido = aplicar_amortecedor(pps_desejado);

            if (pps_amortecido <= 0)
                pps_amortecido = 0;
            if (pps_amortecido > 50000)
                pps_amortecido = 50000;

            alterar_velocidade(pps_amortecido);

            if (passo % 10 == 0 || passo == passos_desac2 - 1)
            {
                printf("   🎯 Desaceleração 2: %u/%u - %u PPS (desejado: %u)\n",
                       passo, passos_desac2, pps_amortecido, pps_desejado);
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (motor_ligado)
    {
        uint32_t pps_amortecido = aplicar_amortecedor(pps_final);
        alterar_velocidade(pps_amortecido);
    }

    printf("✅ Desaceleração trapezoidal com amortecedor concluída: %u PPS\n", pps_final);
}

void executar_desaceleracao_trapezoidal_rapida()
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("🛑 Executando desaceleração trapezoidal RÁPIDA\n");

    rampa_desaceleracao_trapezoidal(15000, 2000, 1500);
}

void executar_desaceleracao_trapezoidal_muito_rapida()
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando desaceleração trapezoidal MUITO RÁPIDA\n");

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    configurar_amortecedor(0.3f);
    rampa_desaceleracao_trapezoidal(velocidade_atual, velocidade_atual / 2, 1000);
}

void executar_desaceleracao_para_zero_e_inverter(uint32_t vel_atual, direcao_motor_t nova_direcao)
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }

    printf("Executando desaceleração DIRETA PARA ZERO e inversão\n");

    printf("Desacelerando de %u para 0 PPS...\n", vel_atual);

    configurar_amortecedor(0.025f);
    rampa_desaceleracao_trapezoidal(vel_atual, 0, 1100);

    printf("Motor parado - alterando direção...\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_level(DIR_PIN, nova_direcao);
    direcao_atual = nova_direcao;

    printf("Direção alterada para: %s\n",
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    printf("Acelerando de 0 para %u PPS...\n", vel_atual);

    configurar_amortecedor(0.25f);

    rampa_aceleracao_trapezoidal(100, vel_atual, 1100);

    printf("Transição de direção com parada completa concluída!\n");
}