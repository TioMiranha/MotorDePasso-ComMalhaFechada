#include "../include/bicaInclude.h"

void tarefa_girar_motor(void *param)
{
    printf("🎯 Iniciando tarefa de rotação contínua\n");

    uint32_t ultima_velocidade = 0;
    uint32_t periodo_ticks = 500;
    uint32_t ultimo_watchdog_reset = xTaskGetTickCount();

    esp_task_wdt_add(NULL);

    while (motor_ligado)
    {
        if ((xTaskGetTickCount() - ultimo_watchdog_reset) * portTICK_PERIOD_MS > 100)
        {
            esp_task_wdt_reset();
            ultimo_watchdog_reset = xTaskGetTickCount();
        }

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

        if (velocidade_atual != ultima_velocidade)
        {
            if (velocidade_atual > 0)
            {
                periodo_ticks = 1000000 / velocidade_atual;
            }
            else
            {
                periodo_ticks = 500;
            }

            if (periodo_ticks < 20)
                periodo_ticks = 20;

            ultima_velocidade = velocidade_atual;

            printf("🔄 Velocidade alterada para %u PPS (periodo: %u us)\n",
                   velocidade_atual, periodo_ticks);
        }

        if (velocidade_atual == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint32_t pulse_width = periodo_ticks / 4;
        if (pulse_width < 5)
            pulse_width = 5;

        rmt_item32_t pulso = {
            .duration0 = pulse_width,
            .level0 = 1,
            .duration1 = periodo_ticks - pulse_width,
            .level1 = 0};

        ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, &pulso, 1, false));

        uint32_t delay_ms = periodo_ticks / 1000;
        if (delay_ms < 1)
            delay_ms = 1;
        if (delay_ms > 50)
            delay_ms = 50;

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    printf("🛑 Rotação parada - tarefa do motor finalizada\n");
    esp_task_wdt_delete(NULL);
    tarefa_motor = NULL;
    vTaskDelete(NULL);
}

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

    uint32_t vel_atual = 0;
    velocidade_pps = converter_pps_para_normalizada(2000, direcao_atual);
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE)
    {
        vel_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
    }

    printf("Motor girando @ %u PPS\n", vel_atual);
}

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