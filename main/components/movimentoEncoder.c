#include "../functions/movimentoEncoder.h"

// Variáveis globais (adicionar no seu header)

static TaskHandle_t xTaskHomeHandle = NULL;

void inicia_fim_de_curso(void)
{
    if (xMutexHome == NULL) {
        xMutexHome = xSemaphoreCreateMutex();
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FIM_DE_CURSO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

bool verifica_timeout(uint32_t start_time, uint32_t timeout_ms)
{
    return ((xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS > timeout_ms);
}

void home_robusto(void *pvParameters)
{
    (void)pvParameters;
    
    printf("🏠 INICIANDO HOME ROBUSTO\n");
    home_encontrado = false;

    // FASE 1: Busca rápida inicial
    printf("🔍 Fase 1: Busca rápida\n");
    movimento_continuo_home(0, 1000); // Direção horária, velocidade rápida

    uint32_t start_time = xTaskGetTickCount();
    bool timeout_ocorreu = false;
    
    // Aguarda ativar o fim de curso (LOW = ativado)
    while (gpio_get_level(FIM_DE_CURSO_PIN) != 0)
    {
        if (verifica_timeout(start_time, 30000)) {
            printf("❌ Timeout Fase 1: Fim de curso não encontrado em 30s\n");
            timeout_ocorreu = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    parar_movimento();
    
    if (timeout_ocorreu) {
        printf("🚫 HOME ABORTADO\n");
        xTaskHomeHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    printf("🎯 Fim de curso encontrado na Fase 1\n");
    vTaskDelay(pdMS_TO_TICKS(500));

    // FASE 2: Recuo para liberar o fim de curso
    printf("🔄 Fase 2: Recuando\n");
    movimento_continuo_home(1, 500); // Direção anti-horária, velocidade média
    vTaskDelay(pdMS_TO_TICKS(800)); // Recuo mais generoso
    parar_movimento();
    vTaskDelay(pdMS_TO_TICKS(300));

    // FASE 3: Busca lenta final
    printf("🎯 Fase 3: Busca lenta final\n");
    movimento_continuo_home(0, 500); // Direção horária, velocidade lenta

    start_time = xTaskGetTickCount();
    timeout_ocorreu = false;
    
    while (gpio_get_level(FIM_DE_CURSO_PIN) != 0)
    {
        if (verifica_timeout(start_time, 15000)) { // 15s para busca lenta
            printf("❌ Timeout Fase 3: Fim de curso não encontrado na busca lenta\n");
            timeout_ocorreu = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    parar_movimento();

    if (!timeout_ocorreu) {
        // ZERAR POSIÇÃO ABSOLUTA
        if (xSemaphoreTake(xMutexEncoder, pdMS_TO_TICKS(100)) == pdTRUE) {
            posicao_acumulada = 0;
            pcnt_counter_clear(PCNT_ENCODER_UNIT);
            xSemaphoreGive(xMutexEncoder);
            home_encontrado = true;
            printf("🎉 HOME ROBUSTO CONCLUÍDO! Posição absoluta: 0\n");
        } else {
            printf("⚠️  Aviso: Não foi possível acessar mutex do encoder\n");
        }
    } else {
        printf("🚫 HOME INCOMPLETO - Posição não zerada\n");
    }

    xTaskHomeHandle = NULL;
    vTaskDelete(NULL);
}

void alterar_velocidade_suave(uint32_t nova_velocidade) {
    if (xSemaphoreTake(xMutexVelocidade, portMAX_DELAY) == pdTRUE) {
        uint32_t velocidade_atual = velocidade_pps;
        xSemaphoreGive(xMutexVelocidade);
        
        if (nova_velocidade > velocidade_atual) {
            acelerar_suavemente_para(nova_velocidade);
        } else {
            desacelerar_suavemente_para(nova_velocidade);
        }
    }
}

// Função para movimento com velocidade controlada (uso geral)
void movimento_controlado(int direcao, uint32_t velocidade) {
    if (!home_finalizado_com_sucesso() && direcao != 0 && direcao != 1) {
        printf("⚠️  Sistema não referenciado. Execute HOME primeiro.\n");
        return;
    }
    
    movimento_continuo_home(direcao, velocidade);
}

void iniciar_home_robusto(void)
{
    // Impede múltiplas instâncias do home
    if (xTaskHomeHandle != NULL) {
        printf("⚠️  Home já em execução\n");
        return;
    }

    xTaskCreate(
        home_robusto,
        "HomeRobusto",
        4096,
        NULL,
        tskIDLE_PRIORITY + 3,
        &xTaskHomeHandle);
}

void parar_home(void)
{
    if (xTaskHomeHandle != NULL) {
        vTaskDelete(xTaskHomeHandle);
        xTaskHomeHandle = NULL;
        parar_movimento();
        printf("🛑 Home interrompido pelo usuário\n");
    }
}

bool home_finalizado_com_sucesso(void)
{
    return home_encontrado;
}