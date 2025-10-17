#include "../include/bicaInclude.h"

void rampa_aceleracao_trapezoidal(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms)
{
    printf("Iniciando rampa TRAPEZOIDAL COM AMORTECEDOR: %u -> %u PPS em %u ms\n", pps_inicial, pps_final, duracao_ms);

    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro!\n");
        return;
    }

    // Limites de segurança
    if (pps_inicial <= 0)
        pps_inicial = 0;
    if (pps_final <= 0)
        pps_final = 0;
    if (pps_inicial > 50000)
        pps_inicial = 50000;
    if (pps_final > 50000)
        pps_final = 50000;

    int32_t diferenca = (int32_t)pps_final - (int32_t)pps_inicial;
    if (diferenca == 0)
    {
        printf("⚠️  Velocidade inicial e final são iguais!\n");
        return;
    }

    // Reset do amortecedor ao iniciar nova rampa
    ultimo_pps = pps_inicial;

    uint32_t velocidade_maxima;
    uint32_t tempo_aceleracao, tempo_constante, tempo_desaceleracao;

    if (abs(diferenca) < 1000)
    {
        velocidade_maxima = pps_final;
        tempo_aceleracao = duracao_ms / 2;
        tempo_constante = 0;
        tempo_desaceleracao = duracao_ms - tempo_aceleracao;
        printf("Perfil TRIANGULAR (diferença pequena)\n");
    }
    else
    {
        velocidade_maxima = pps_final;

        uint32_t percentual_aceleracao = 30 + (abs(diferenca) / 500);
        if (percentual_aceleracao > 50)
            percentual_aceleracao = 50;

        tempo_aceleracao = (duracao_ms * percentual_aceleracao) / 100;
        tempo_desaceleracao = tempo_aceleracao;
        tempo_constante = duracao_ms - tempo_aceleracao - tempo_desaceleracao;

        printf("Perfil TRAPEZOIDAL (aceleração: %u%%)\n", percentual_aceleracao);
    }

    printf("🔄 Fases da rampa:\n");
    printf("   • Aceleração: %u ms (%u → %u PPS)\n", tempo_aceleracao, pps_inicial, velocidade_maxima);
    if (tempo_constante > 0)
    {
        printf("   • Constante: %u ms (%u PPS)\n", tempo_constante, velocidade_maxima);
    }
    printf("   • Desaceleração: %u ms (%u → %u PPS)\n", tempo_desaceleracao, velocidade_maxima, pps_final);

    // Fase de aceleração com amortecedor
    if (tempo_aceleracao > 0 && motor_ligado)
    {
        uint32_t passos_aceleracao = tempo_aceleracao / 5;
        if (passos_aceleracao < 3)
            passos_aceleracao = 3;

        for (uint32_t passo = 0; passo < passos_aceleracao && motor_ligado; passo++)
        {
            float progresso = (float)passo / (passos_aceleracao - 1);
            float curva_suave = progresso * progresso;

            uint32_t pps_desejado = pps_inicial + (uint32_t)(curva_suave * (velocidade_maxima - pps_inicial));

            // Aplica amortecedor
            uint32_t pps_amortecido = aplicar_amortecedor(pps_desejado);

            // Limites de segurança
            if (pps_amortecido <= 0)
                pps_amortecido = 0;
            if (pps_amortecido > 50000)
                pps_amortecido = 50000;

            alterar_velocidade(pps_amortecido);

            if (passo % 8 == 0 || passo == passos_aceleracao - 1)
            {
                printf("   ↗ Aceleração: %u/%u - %u PPS (desejado: %u)\n",
                       passo, passos_aceleracao, pps_amortecido, pps_desejado);
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    // Fase constante
    if (tempo_constante > 0 && motor_ligado)
    {
        uint32_t pps_amortecido = aplicar_amortecedor(velocidade_maxima);
        alterar_velocidade(pps_amortecido);
        printf("   ➡ Velocidade constante: %u PPS por %u ms\n", pps_amortecido, tempo_constante);

        uint32_t check_interval = 100;
        uint32_t checks = tempo_constante / check_interval;

        for (uint32_t i = 0; i < checks && motor_ligado; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(check_interval));
        }
    }

    // Fase de desaceleração com amortecedor
    if (tempo_desaceleracao > 0 && motor_ligado)
    {
        uint32_t passos_desaceleracao = tempo_desaceleracao / 5;
        if (passos_desaceleracao < 3)
            passos_desaceleracao = 3;

        for (uint32_t passo = 0; passo < passos_desaceleracao && motor_ligado; passo++)
        {
            float progresso = (float)passo / (passos_desaceleracao - 1);
            float curva_suave = 1.0 - ((1.0 - progresso) * (1.0 - progresso));

            uint32_t pps_desejado = velocidade_maxima - (uint32_t)(curva_suave * (velocidade_maxima - pps_final));

            // Aplica amortecedor
            uint32_t pps_amortecido = aplicar_amortecedor(pps_desejado);

            // Limites de segurança
            if (pps_amortecido <= 0)
                pps_amortecido = 0;
            if (pps_amortecido > 50000)
                pps_amortecido = 50000;

            alterar_velocidade(pps_amortecido);

            if (passo % 8 == 0 || passo == passos_desaceleracao - 1)
            {
                printf("   ↘ Desaceleração: %u/%u - %u PPS (desejado: %u)\n",
                       passo, passos_desaceleracao, pps_amortecido, pps_desejado);
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (motor_ligado)
    {
        uint32_t pps_amortecido = aplicar_amortecedor(pps_final);
        alterar_velocidade(pps_amortecido);
    }

    printf("✅ Rampa trapezoidal com amortecedor concluída: %u PPS\n", pps_final);
}

void rampa_aceleracao_trapezoidal_avancada(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms, uint32_t aceleracao_max_pps_s)
{
    printf("🎯 Iniciando rampa TRAPEZOIDAL AVANÇADA: %u -> %u PPS em %u ms\n", pps_inicial, pps_final, duracao_ms);
    printf("⚡ Aceleração máxima: %u PPS/s\n", aceleracao_max_pps_s);

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
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("🚀 Executando rampa trapezoidal RÁPIDA\n");
    configurar_amortecedor(0.3f);
    rampa_aceleracao_trapezoidal(2000, 15000, 1500); // 100→5000 PPS em 1.5s (mais rápido)
}

void executar_rampa_trapezoidal_suave()
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("🕊️  Executando rampa trapezoidal SUAVE\n");
    rampa_aceleracao_trapezoidal(100, 5000, 3000); // 100→5000 PPS em 3s (mais suave)
}

void executar_rampa_trapezoidal_avancada_rapida()
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando rampa trapezoidal AVANÇADA RÁPIDA\n");
    rampa_aceleracao_trapezoidal_avancada(100, 5000, 2000, 15000); // Aceleração máxima de 15000 PPS/s
}

void executar_rampa_trapezoidal_avancada_rapida_em_transicao()
{
    if (!motor_ligado)
    {
        printf("❌ Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando rampa trapezoidal AVANÇADA RÁPIDA\n");
    rampa_aceleracao_trapezoidal_avancada(0, velocidade_pps, 2000, 15000); // Aceleração máxima de 15000 PPS/s
}