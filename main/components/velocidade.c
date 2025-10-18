#include "../include/bicaInclude.h"

void converter_normalizada_para_pps(uint16_t normalizada, uint32_t *pps, direcao_motor_t *direcao)
{
    if (normalizada == VELOCIDADE_NEUTRA)
    {
        *pps = 0;
        *direcao = direcao_atual;
        return;
    }

    // CORREÇÃO: Lógica invertida consertada
    if (normalizada < VELOCIDADE_NEUTRA)
    {
        // HORÁRIO: 0-4095 (0 = MÁXIMO horário, 4095 = MÍNIMO horário)
        *direcao = DIRECAO_HORARIA;
        uint16_t intensidade = normalizada; // 0-4095
        *pps = (uint32_t)((1.0 - (intensidade / 4096.0)) * PPS_MAXIMO);
    }
    else
    {
        // ANTI-HORÁRIO: 4097-8192 (4097 = MÍNIMO anti-horário, 8192 = MÁXIMO anti-horário)
        *direcao = DIRECAO_ANTI_HORARIA;
        uint16_t intensidade = normalizada - VELOCIDADE_NEUTRA; // 1-4096
        *pps = (uint32_t)((intensidade / 4096.0) * PPS_MAXIMO);
    }

    // Garante limites
    if (*pps < PPS_MINIMO) *pps = PPS_MINIMO;
    if (*pps > PPS_MAXIMO) *pps = PPS_MAXIMO;
    
    // CORREÇÃO: Se PPS for muito baixo, considera como parado
    if (*pps < 100) {
        *pps = 0;
        *direcao = direcao_atual;
    }
}

uint16_t converter_pps_para_normalizada(uint32_t pps, direcao_motor_t direcao)
{
    if (pps == 0)
    {
        return VELOCIDADE_NEUTRA;
    }

    float percentual = (float)pps / PPS_MAXIMO;

    if (direcao == DIRECAO_HORARIA)
    {
        return VELOCIDADE_NEUTRA - (uint16_t)(percentual * 4096.0 + 0.5); // Arredondamento
    }
    else
    {
        return VELOCIDADE_NEUTRA + (uint16_t)(percentual * 4096.0 + 0.5); // Arredondamento
    }
}

void setar_velocidade_normalizada(uint16_t nova_velocidade)
{
    // Aplica limites
    if (nova_velocidade < VELOCIDADE_MINIMA) nova_velocidade = VELOCIDADE_MINIMA;
    if (nova_velocidade > VELOCIDADE_MAXIMA) nova_velocidade = VELOCIDADE_MAXIMA;
    
    uint32_t pps_alvo;
    direcao_motor_t direcao_alvo;
    converter_normalizada_para_pps(nova_velocidade, &pps_alvo, &direcao_alvo);
    
    if (nova_velocidade == VELOCIDADE_NEUTRA) {
        alterar_velocidade(0);
        velocidade_normalizada = VELOCIDADE_NEUTRA;
        return;
    }
    
    // Verifica se precisa mudar direção
    if (direcao_atual != direcao_alvo && motor_ligado) {
        printf("   🔄 Mudando direção: %s -> %s\n",
               direcao_atual == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA",
               direcao_alvo == DIRECAO_HORARIA ? "HORÁRIA" : "ANTI-HORÁRIA");
        
        gpio_set_level(DIR_PIN, direcao_alvo);
        direcao_atual = direcao_alvo;
        vTaskDelay(pdMS_TO_TICKS(10)); // Aumentei para 10ms para garantir estabilização
    }

    alterar_velocidade(pps_alvo);
    velocidade_normalizada = nova_velocidade;
}

void alterar_velocidade(uint32_t nova_velocidade)
{
    esp_task_wdt_reset();

    // CORREÇÃO CRÍTICA: Tratamento de velocidade zero primeiro
    if (nova_velocidade == 0) {
        if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE) {
            velocidade_pps = 0;
            // Desativa completamente o RMT
            rmt_driver_uninstall(RMT_CHANNEL_0);
            xSemaphoreGive(xMutexVelocidade);
        }
        printf("🛑 Motor parado (0 PPS)\n");
        return;
    }

    // CORREÇÃO: Aplica limites APÓS verificar velocidade zero
    if (nova_velocidade < PPS_MINIMO) {
        nova_velocidade = PPS_MINIMO;
    }

    if (nova_velocidade > PPS_MAXIMO) {
        nova_velocidade = PPS_MAXIMO;
    }

    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE) {
        velocidade_pps = nova_velocidade;
        
        // CORREÇÃO: Cálculo SEGURO do período (evita divisão por zero)
        uint32_t periodo = 500000 / nova_velocidade; // Em microssegundos
        
        // CORREÇÃO: Limita período mínimo e máximo para evitar valores extremos
        if (periodo < 10) { // Mínimo 10us (50,000 PPS)
            periodo = 10;
        }
        if (periodo > 50000) { // Máximo 50ms (20 PPS)
            periodo = 50000;
        }

        // Configura RMT para a nova velocidade
        rmt_config_t rmt_tx = {
            .gpio_num = STEP_PIN,
            .channel = RMT_CHANNEL_0,
            .clk_div = 80, // 1MHz (80MHz/80)
            .mem_block_num = 1,
            .tx_config = {
                .carrier_freq_hz = 0,
                .carrier_level = RMT_CARRIER_LEVEL_LOW,
                .idle_level = RMT_IDLE_LEVEL_LOW,
                .carrier_duty_percent = 50,
                .carrier_en = false,
                .loop_en = true, // CORREÇÃO: Habilita loop para continuidade
                .idle_output_en = true,
            },
            .rmt_mode = RMT_MODE_TX
        };
        
        // CORREÇÃO: Remove driver anterior antes de instalar novo
        rmt_driver_uninstall(RMT_CHANNEL_0);
        rmt_config(&rmt_tx);
        rmt_driver_install(rmt_tx.channel, 0, 0);
        
        // Configura o padrão de pulso (50% duty cycle)
        rmt_item32_t item = {
            .duration0 = periodo, // Tempo alto em microssegundos
            .level0 = 1,
            .duration1 = periodo, // Tempo baixo em microssegundos  
            .level1 = 0
        };
        
        rmt_write_items(rmt_tx.channel, &item, 1, true);
        xSemaphoreGive(xMutexVelocidade);
    }

    printf("🔄 Velocidade alterada para %u PPS (periodo: %u us)\n", nova_velocidade, 500000 / nova_velocidade);
}