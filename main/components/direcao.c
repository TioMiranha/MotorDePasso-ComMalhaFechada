#include ".././include/bicaInclude.h"

uint16_t obter_velocidade_normalizada() {
    int8_t normalizada = converter_pps_para_normalizada(velocidade_pps, direcao_atual);
    return normalizada;
}

void alterar_direcao_suave(direcao_motor_t nova_direcao)
{
    if (direcao_atual == nova_direcao)
    {
        printf("⚠️  Motor já está na direção solicitada\n");
        return;
    }

    if (!motor_ligado)
    {
        gpio_set_level(DIR_PIN, nova_direcao);
        direcao_atual = nova_direcao;
        printf("Direção alterada para: %s\n",
               nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
        return;
    }

    printf("🔄 Iniciando inversão de direção suave...\n");

    uint16_t vel_normalizada_atual = obter_velocidade_normalizada();

    // Se o motor estiver parado, inverter imediatamente
    if (vel_normalizada_atual == VELOCIDADE_NEUTRA) {
        gpio_set_level(DIR_PIN, nova_direcao);
        direcao_atual = nova_direcao;
        printf("Direção alterada (motor parado): %s\n",
               nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
        return;
    }

    printf("📊 Velocidade atual: %u\n", vel_normalizada_atual);

    executar_desaceleracao_para_zero_e_inverter(vel_normalizada_atual, nova_direcao);
}

void direcao_horaria()
{
    printf("Solicitando direção HORÁRIA\n");
    alterar_direcao_suave(DIRECAO_HORARIA);
}

void direcao_anti_horaria()
{
    printf("Solicitando direção ANTI-HORÁRIA\n");
    alterar_direcao_suave(DIRECAO_ANTI_HORARIA);
}

void inverter_direcao()
{
    direcao_motor_t nova_direcao = (direcao_atual == DIRECAO_HORARIA) ? DIRECAO_ANTI_HORARIA : DIRECAO_HORARIA;

    printf("🔄 Invertendo direção: %s → %s\n",
           direcao_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
           nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    alterar_direcao_suave(nova_direcao);
}

void acelerar_com_direcao(uint8_t pps_inicial, uint8_t pps_final, uint32_t duracao_ms, direcao_motor_t direcao)
{
    printf("🎯 Aceleração com direção controlada\n");

    // Primeiro garantir a direção correta
    if (direcao_atual != direcao)
    {
        printf("   🔄 Ajustando direção primeiro...\n");
        alterar_direcao_suave(direcao);
    }

    // Depois acelerar
    rampa_aceleracao_trapezoidal(pps_inicial, pps_final, duracao_ms);
}

void executar_direcao_horaria_com_aceleracao()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando aceleração em sentido HORÁRIO\n");
    acelerar_com_direcao(20,49, 2000, DIRECAO_HORARIA);
}

void executar_direcao_anti_horaria_com_aceleracao()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando aceleração em sentido ANTI-HORÁRIO\n");
    acelerar_com_direcao(20, 49, 2000, DIRECAO_ANTI_HORARIA);
}

void executar_inversao_suave()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    inverter_direcao();
}