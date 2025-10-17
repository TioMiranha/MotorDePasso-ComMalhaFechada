#include "include/bicaInclude.h"
#include "GLOBAL_VARS/vars.h"


// Menu interativo
void mostrar_menu()
{
    printf("\n");
    printf("┌─────────────────────────────────┐\n");
    printf("│     CONTROLE MOTOR PASSO        │\n");
    printf("│      CONTROLE DE DIREÇÃO        │\n");
    printf("├─────────────────────────────────┤\n");
    printf("│ 1 → Iniciar rotação             │\n");
    printf("│ 2 → Parar rotação               │\n");
    printf("│ 3 → Velocidade +1000            │\n");
    printf("│ 4 → Velocidade -1000            │\n");
    printf("│ 5 → Velocidade personalizada    │\n");
    printf("│ 6 → Aceleração trapezoidal      │\n");
    printf("│ 7 → Desaceleração trapezoidal   │\n");
    printf("│ 8 → Direção HORÁRIA             │\n");
    printf("│ 9 → Direção ANTI-HORÁRIA        │\n");
    printf("│ A → Mover FRENTE suave          │\n");
    printf("│ B → Mover FRENTE rapido         │\n");
    printf("│ C → Mover TRÁS rápido           │\n");
    printf("│ D → Mover TRÁS suave            │\n");
    printf("│ E → Parar movimento             |\n");
    printf("│ F → Mover Trás                  |\n");
    printf("│ G → Mover Frente                |\n");
    printf("│ 0 → Sair                        │\n");
    printf("└─────────────────────────────────┘\n");
    printf("Opção: ");
}

void app_main()
{
    printf("Iniciando controle de motor passo com RMT\n");

    configurar_gpio();
    configurar_rmt();

    printf("\n🔧 CONFIGURAÇÃO RMT OTIMIZADA:\n");
    printf("   • Frequência RMT: 1 MHz (1 tick = 1µs)\n");
    printf("   • Clock divisor: %d\n", RMT_CLK_DIV);
    printf("   • Velocidade atual: %u PPS\n", velocidade_pps);
    printf("   • Range: 10 - 50.000 PPS\n");
    printf("   • Rampas suaves com atualização a cada 10ms\n");

    char opcao[10];
    gpio_set_level(DIR_PIN, direcao_atual);
    printf("   • Direção inicial: %s\n",
           direcao_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");

    while (1)
    {
        mostrar_menu();

        if (fgets(opcao, sizeof(opcao), stdin))
        {
            switch (opcao[0])
            {
            case '1':
                iniciar_rotacao();
                break;

            case '2':
                parar_rotacao();
                break;

            case '3':
            {
                uint32_t nova_vel = velocidade_pps + 1000;
                if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
                {
                    nova_vel = velocidade_pps + 1000;
                    xSemaphoreGive(xMutexVelocidade);
                }
                alterar_velocidade(nova_vel);
                break;
            }

            case '4':
            {
                uint32_t nova_vel = (velocidade_pps > 1000) ? velocidade_pps - 1000 : 10;
                if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
                {
                    nova_vel = (velocidade_pps > 1000) ? velocidade_pps - 1000 : 10;
                    xSemaphoreGive(xMutexVelocidade);
                }
                alterar_velocidade(nova_vel);
                break;
            }

            case '5':
            {
                printf("Digite a velocidade (10-50000 PPS): ");
                char entrada[20];
                if (fgets(entrada, sizeof(entrada), stdin))
                {
                    uint32_t nova_vel = atoi(entrada);
                    alterar_velocidade(nova_vel);
                }
                break;
            }

            case '6':
                executar_rampa_trapezoidal_rapida();
                break;

            case '7':
                executar_desaceleracao_trapezoidal_muito_rapida();
                break;

            case '8':
                direcao_horaria();
                break;

            case '9':
                direcao_anti_horaria();
                break;

            case 'A':
            case 'a':
                executar_mover_frente_rapido();
            case 'B':
            case 'b':
                executar_mover_frente_devagar();
                break;

            case 'C':
            case 'c':
                executar_mover_tras_rapido();
                break;

            case 'D':
            case 'd':
            {
                executar_mover_tras_devagar();
                break;
            }

            case 'E':
            case 'e':
            {
                parar_movimento();
                break;
            }

            case 'F':
            case 'f':
            {
                movimento_continuo(0);
                break;
            }

            case 'G':
            case 'g':
            {
                movimento_continuo(1);
                break;
            }

            case '0':
                printf("\n👋 Saindo...\n");
                parar_rotacao();
                gpio_set_level(ENABLE_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                return;

            default:
                printf("❌ Opção inválida!\n");
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}