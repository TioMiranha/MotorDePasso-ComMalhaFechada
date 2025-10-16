#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_log.h"

// Configurações do motor
#define STEP_PIN GPIO_NUM_5
#define DIR_PIN GPIO_NUM_2
#define ENABLE_PIN GPIO_NUM_4

// Configuração RMT OTIMIZADA - 1 MHz resolution
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_CLK_DIV 100 // 80 MHz / 80 = 1 MHz

// Variáveis globais
static volatile int motor_ligado = 0;
static volatile uint32_t velocidade_pps = 2000;
static TaskHandle_t tarefa_motor = NULL;

static volatile int movimento_em_andamento = 0;
static SemaphoreHandle_t xMutexMovimento;
static TaskHandle_t tarefa_movimento = NULL;

// Mutex para proteger variáveis compartilhadas
static SemaphoreHandle_t xMutexVelocidade;

float amortecedor_factor = 0.2f; // Fator de suavização (0.0 - 1.0)
uint32_t ultimo_pps = 0;

static const char *TAG = "MOTOR_RMT";

// Configuração RMT corrigida
void configurar_rmt()
{
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = STEP_PIN,
        .clk_div = RMT_CLK_DIV,
        .mem_block_num = 4,
        .tx_config = {
            .carrier_freq_hz = 0,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = 0,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }};

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
    printf("RMT configurado @ 1 MHz (1 tick = 1µs)\n");
}

void configurar_gpio()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DIR_PIN) | (1ULL << ENABLE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    gpio_set_level(DIR_PIN, 0);
    gpio_set_level(ENABLE_PIN, 1);

    printf("GPIOs configurados\n");

    // Mutex de velocidade
    xMutexVelocidade = xSemaphoreCreateMutex();
    if (xMutexVelocidade == NULL)
    {
        printf("Erro ao criar mutex de velocidade!\n");
    }

    // 🔒 CORREÇÃO: Mutex de movimento
    xMutexMovimento = xSemaphoreCreateMutex();
    if (xMutexMovimento == NULL)
    {
        printf("Erro ao criar mutex de movimento!\n");
    }
    else
    {
        printf("Mutex de movimento criado\n");
    }
}

// Tarefa principal do motor - CORREÇÃO CRÍTICA
void tarefa_girar_motor(void *param)
{
    printf("Iniciando rotacao continua\n");

    uint32_t ultima_velocidade = 0;
    uint32_t periodo_ticks = 500; // Inicial a 2000 PPS

    while (motor_ligado)
    {
        // Obter velocidade atual de forma protegida
        uint32_t velocidade_atual;
        if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
        {
            velocidade_atual = velocidade_pps;
            xSemaphoreGive(xMutexVelocidade);
        }
        else
        {
            velocidade_atual = 2000;
        }

        // Só recalcular se a velocidade mudou
        if (velocidade_atual != ultima_velocidade)
        {
            if (velocidade_atual > 0)
            {
                periodo_ticks = 1000000 / velocidade_atual;
            }
            else
            {
                periodo_ticks = 500; // Fallback
            }

            // Garantir período mínimo válido
            if (periodo_ticks < 20)
                periodo_ticks = 20;

            ultima_velocidade = velocidade_atual;
        }

        // Pulso com 25% de duty cycle
        uint32_t pulse_width = periodo_ticks / 4;
        if (pulse_width < 5)
            pulse_width = 5;

        rmt_item32_t pulso = {
            .duration0 = pulse_width,
            .level0 = 1,
            .duration1 = periodo_ticks - pulse_width,
            .level1 = 0};

        ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, &pulso, 1, false));

        uint32_t delay_ticks = periodo_ticks / 1000;
        if (delay_ticks < 1)
            delay_ticks = 1;
        vTaskDelay(pdMS_TO_TICKS(delay_ticks));
    }

    printf("Rotacao parada\n");
    tarefa_motor = NULL;
    vTaskDelete(NULL);
}

// Inicia rotação contínua
void iniciar_rotacao()
{
    if (tarefa_motor != NULL)
    {
        printf("Motor ja esta girando\n");
        return;
    }

    motor_ligado = 1;
    if (xTaskCreate(tarefa_girar_motor, "MotorTask", 4096, NULL, 5, &tarefa_motor) != pdPASS)
    {
        printf("Erro ao criar tarefa do motor\n");
        motor_ligado = 0;
        return;
    }

    uint32_t vel_atual = 2000;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        vel_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    printf("Motor girando @ %u PPS\n", vel_atual);
}

// Para rotação contínua
void parar_rotacao()
{
    motor_ligado = 0;
    if (tarefa_motor != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        tarefa_motor = NULL;
    }
    printf("Motor parado\n");
}

// Altera velocidade - CORRIGIDA
void alterar_velocidade(uint32_t nova_velocidade)
{
    // Limites realistas para 1 MHz
    if (nova_velocidade <= 0)
    {
        printf("Velocidade muito baixa! Minimo: 10 PPS\n");
        nova_velocidade = 0;
    }

    if (nova_velocidade > 50000)
    {
        printf("Velocidade limitada a 50.000 PPS\n");
        nova_velocidade = 50000;
    }

    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_pps = nova_velocidade;
        xSemaphoreGive(xMutexVelocidade);
    }

    printf("Velocidade alterada para %u PPS\n", nova_velocidade);
}

uint32_t aplicar_amortecedor(uint32_t pps_desejado)
{
    if (ultimo_pps == 0)
    {
        ultimo_pps = pps_desejado;
        return pps_desejado;
    }

    uint32_t pps_suavizado = (uint32_t)(amortecedor_factor * pps_desejado +
                                        (1.0f - amortecedor_factor) * ultimo_pps);

    ultimo_pps = pps_suavizado;
    return pps_suavizado;
}

void configurar_amortecedor(float factor)
{
    if (factor < 0.0f)
        factor = 0.0f;
    if (factor > 1.0f)
        factor = 1.0f;
    amortecedor_factor = factor;
    printf("Amortecedor configurado: %.2f\n", factor);
}

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

typedef enum
{
    DIRECAO_HORARIA = 0,
    DIRECAO_ANTI_HORARIA = 1
} direcao_motor_t;

static direcao_motor_t direcao_atual = DIRECAO_HORARIA;

void executar_desaceleracao_trapezoidal_em_transicao(uint32_t vel_atual, uint32_t vel_transicao, direcao_motor_t direcao)
{

    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("⚡ Executando desaceleração trapezoidal MUITO RÁPIDA\n");
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        vel_atual = velocidade_pps;

        xSemaphoreGive(xMutexVelocidade);
    }

    configurar_amortecedor(000.1f);
    rampa_desaceleracao_trapezoidal(vel_atual, vel_transicao, 1500);
    
    gpio_set_level(DIR_PIN, direcao);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    direcao_atual = direcao;
    
    printf("   ✅ Direção alterada para: %s\n",
        direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
    printf("Acelerando de %u para %u PPS...\n", vel_transicao, vel_atual);

    configurar_amortecedor(000.1f);
    rampa_aceleracao_trapezoidal(vel_transicao, vel_atual, 1500);

    printf("Transição de direção concluída suavemente!\n");
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
        // Se motor está parado, muda direção diretamente
        gpio_set_level(DIR_PIN, nova_direcao);
        direcao_atual = nova_direcao;
        printf("Direção alterada para: %s\n",
               nova_direcao == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
        return;
    }

    printf("Invertendo direção com transição suave...\n");

    uint32_t velocidade_atual = 0;
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }
    else
    {
        velocidade_atual = 2000;
    }

    uint32_t velocidade_transicao = movimento_em_andamento;

    if (velocidade_atual < velocidade_transicao)
    {
        velocidade_transicao = velocidade_atual;
    }

    printf("   📉 Desacelerando de %u para %u PPS...\n", velocidade_atual, velocidade_transicao);

    if (velocidade_atual > velocidade_transicao)
    {
        executar_desaceleracao_trapezoidal_em_transicao(velocidade_atual, velocidade_transicao, nova_direcao);
    }
}

void direcao_horaria()
{
    printf("Solicitando direção HORÁRIA\n");
    alterar_direcao_suave(DIRECAO_HORARIA);
}

void direcao_anti_horaria()
{
    printf("🔄 Solicitando direção ANTI-HORÁRIA\n");
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

void acelerar_com_direcao(uint32_t pps_inicial, uint32_t pps_final, uint32_t duracao_ms, direcao_motor_t direcao)
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
    acelerar_com_direcao(100, 5000, 2000, DIRECAO_HORARIA);
}

void executar_direcao_anti_horaria_com_aceleracao()
{
    if (!motor_ligado)
    {
        printf("Ligue o motor primeiro! (Opção 1)\n");
        return;
    }
    printf("Executando aceleração em sentido ANTI-HORÁRIO\n");
    acelerar_com_direcao(100, 5000, 2000, DIRECAO_ANTI_HORARIA);
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
        printf("🛑 Movimento interrompido\n");
    }

    // Limpeza final
    movimento_em_andamento = 0;
    tarefa_movimento = NULL;
    vTaskDelete(NULL);
}

void mover_frente(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms)
{
    // 🔒 CORREÇÃO: Verificações mais robustas
    if (motor_ligado)
    {
        printf("❌ Pare a rotação contínua primeiro! (Opção 2)\n");
        return;
    }

    if (movimento_em_andamento)
    {
        printf("⚠️  Já existe um movimento em andamento!\n");
        return;
    }

    if (tarefa_movimento != NULL)
    {
        printf("⚠️  Tarefa de movimento já existe!\n");
        return;
    }

    // 🔒 CORREÇÃO: Validar parâmetros
    if (distancia_passos == 0 || velocidade_maxima == 0 || tempo_total_ms == 0)
    {
        printf("❌ Parâmetros inválidos!\n");
        return;
    }

    if (velocidade_maxima > 50000)
    {
        printf("⚠️  Velocidade limitada a 50000 PPS\n");
        velocidade_maxima = 50000;
    }

    if (xSemaphoreTake(xMutexMovimento, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        printf("❌ Timeout ao acessar controle de movimento\n");
        return;
    }

    movimento_em_andamento = 1;
    xSemaphoreGive(xMutexMovimento);

    // 🔒 CORREÇÃO: Alocar memória para parâmetros
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
    parametros[3] = DIRECAO_HORARIA; // 0 = frente

    // Criar tarefa de movimento
    if (xTaskCreate(tarefa_movimento_controlado, "MovimentoTask", 4096, parametros, 4, &tarefa_movimento) != pdPASS)
    {
        printf("❌ Erro ao criar tarefa de movimento!\n");
        movimento_em_andamento = 0;
        free(parametros);
        return;
    }

    printf("🔼 Movimento para FRENTE iniciado em background\n");
}

void mover_tras(uint32_t distancia_passos, uint32_t velocidade_maxima, uint32_t tempo_total_ms)
{
    // Mesmas verificações do mover_frente
    if (motor_ligado)
    {
        printf("❌ Pare a rotação contínua primeiro! (Opção 2)\n");
        return;
    }

    if (movimento_em_andamento)
    {
        printf("⚠️  Já existe um movimento em andamento!\n");
        return;
    }

    if (tarefa_movimento != NULL)
    {
        printf("⚠️  Tarefa de movimento já existe!\n");
        return;
    }

    if (distancia_passos == 0 || velocidade_maxima == 0 || tempo_total_ms == 0)
    {
        printf("❌ Parâmetros inválidos!\n");
        return;
    }

    if (velocidade_maxima > 50000)
    {
        velocidade_maxima = 50000;
    }

    if (xSemaphoreTake(xMutexMovimento, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        printf("❌ Timeout ao acessar controle de movimento\n");
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
    parametros[3] = DIRECAO_ANTI_HORARIA; // 1 = trás

    if (xTaskCreate(tarefa_movimento_controlado, "MovimentoTask", 4096, parametros, 4, &tarefa_movimento) != pdPASS)
    {
        printf("❌ Erro ao criar tarefa de movimento!\n");
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
    mover_frente(800, 2000, 1500); // 🔒 REDUZIDO: 800 passos, 2000 PPS, 1.5 segundos
}

void executar_mover_frente_devagar()
{
    printf("🐢 Movimento SUAVE para FRENTE\n");
    mover_frente(400, 1000, 2000); // 🔒 REDUZIDO: 400 passos, 1000 PPS, 2 segundos
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
    if (motor_ligado && direcao_atual == direcao)
    {
        return;
    }

    // Se motor está ligado mas em direção diferente, para e reinicia
    if (motor_ligado)
    {
        parar_rotacao();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Configurar direção
    gpio_set_level(DIR_PIN, direcao);
    direcao_atual = direcao;

    // Iniciar rotação
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        velocidade_pps = 2000; // Velocidade padrão para movimento contínuo
        xSemaphoreGive(xMutexVelocidade);
    }

    motor_ligado = 1;

    // Criar tarefa de rotação se não existir
    if (tarefa_motor == NULL)
    {
        xTaskCreate(tarefa_girar_motor, "MotorTask", 4096, NULL, 3, &tarefa_motor);
    }

    printf("%s: MOVIMENTO CONTÍNUO\n", direcao == DIRECAO_HORARIA ? "▶ FRENTE" : "◀ TRÁS");
}

// Menu interativo
void mostrar_menu()
{
    printf("\n");
    printf("┌─────────────────────────────────┐\n");
    printf("│     🏎️  CONTROLE MOTOR PASSO    │\n");
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
    printf("│ A → Mover FRENTE rápido         │\n");
    printf("│ B → Mover FRENTE suave          │\n");
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
                executar_desaceleracao_trapezoidal_rapida();
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
                break;

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