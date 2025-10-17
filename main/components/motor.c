#include "../include/bicaInclude.h"

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