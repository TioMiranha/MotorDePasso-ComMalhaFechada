#include "../include/bicaInclude.h"

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