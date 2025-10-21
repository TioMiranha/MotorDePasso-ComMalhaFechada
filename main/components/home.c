#include "../functions/home.h"
#include <stdio.h>

gpio_config_t fimDeCurso;

void IRAM_ATTR encoder_overflow_isr(void *arg) {
    uint32_t status;
    pcnt_get_event_status(PCNT_ENCODER_UNIT, &status);
    
    if (xSemaphoreTakeFromISR(xMutexEncoder, NULL) == pdTRUE) {
        if (status & PCNT_EVT_H_LIM) {
            posicao_acumulada += 32768;
            pcnt_counter_clear(PCNT_ENCODER_UNIT);
        } else if (status & PCNT_EVT_L_LIM) {
            posicao_acumulada -= 32768;
            pcnt_counter_clear(PCNT_ENCODER_UNIT);
        }
        xSemaphoreGiveFromISR(xMutexEncoder, NULL);
    }
}

int32_t encoder_get_position(void) {
    int32_t posicao_total = 0;
    int16_t count_atual;
    
    if (xSemaphoreTake(xMutexEncoder, portMAX_DELAY) == pdTRUE) {
        pcnt_get_counter_value(PCNT_ENCODER_UNIT, &count_atual);
        posicao_total = posicao_acumulada + (int32_t)count_atual;
        xSemaphoreGive(xMutexEncoder);
    }
    
    return posicao_total;
}

void encoder_reset_position(void) {
    if (xSemaphoreTake(xMutexEncoder, portMAX_DELAY) == pdTRUE) {
        posicao_acumulada = 0;
        pcnt_counter_clear(PCNT_ENCODER_UNIT);
        xSemaphoreGive(xMutexEncoder);
    }
}

void home_init(void) {
    // Criar mutex
    if (xMutexHome == NULL) {
        xMutexHome = xSemaphoreCreateMutex();
    }
        
    // Configurar GPIO do fim de curso
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FIM_DE_CURSO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Assumindo fim de curso ativo em LOW
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    printf("✅ Sistema de Home inicializado\n");
    printf("   - Fim de curso: GPIO %d\n", FIM_DE_CURSO_PIN);
    printf("   - Encoder: A=GPIO%d, B=GPIO%d\n", ENCODER_A_PIN, ENCODER_B_PIN);
}

// Verifica se o fim de curso está ativado
bool home_switch_activated(void) {
    return gpio_get_level(FIM_DE_CURSO_PIN) == 0; // Ajuste conforme seu hardware
}

// Inicia sequência de home
bool home_start(void) {
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        if (home_control.homing_in_progress) {
            xSemaphoreGive(xMutexHome);
            printf("⚠️  Home já está em andamento!\n");
            return false;
        }
        
        home_control.state = HOME_FAST_SEARCH;
        home_control.start_time = xTaskGetTickCount();
        home_control.homing_in_progress = true;
        home_control.home_found = false;
        
        printf("🏠 INICIANDO SEQUÊNCIA DE HOME\n");
        printf("   Posição inicial do encoder: %d\n", encoder_get_position());
        xSemaphoreGive(xMutexHome);
        return true;
    }
    return false;
}

// Para a sequência de home
void home_stop(void) {
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        home_control.state = HOME_IDLE;
        home_control.homing_in_progress = false;
        parar_movimento(); // Para o motor no eixo 0
        xSemaphoreGive(xMutexHome);
        printf("🛑 Home interrompido\n");
    }
}

// Verifica se home está completo
bool home_is_complete(void) {
    bool complete = false;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        complete = (home_control.state == HOME_COMPLETE);
        xSemaphoreGive(xMutexHome);
    }
    return complete;
}

// Verifica se home está em andamento
bool home_is_homing(void) {
    bool homing = false;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        homing = home_control.homing_in_progress;
        xSemaphoreGive(xMutexHome);
    }
    return homing;
}

// Obtém posição de home
int32_t home_get_position(void) {
    int32_t position = 0;
    if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
        position = home_control.home_position;
        xSemaphoreGive(xMutexHome);
    }
    return position;
}
/*
// Tarefa principal do home
void home_task(void *pvParameters) {
    printf("🔧 Tarefa de Home iniciada\n");
    
    for (;;) {
        if (home_control.homing_in_progress) {
            switch (home_control.state) {
                
                case HOME_FAST_SEARCH: {
                    printf("🔍 FASE 1: Busca rápida do home\n");
                    printf("   Posição atual: %d\n", encoder_get_position());
                    
                    // Mover na direção negativa para buscar home
                    // Ajuste a direção (1 ou 0) conforme seu hardware
                    
                    // Aguardar até encontrar fim de curso ou timeout
                    uint32_t current_time = xTaskGetTickCount();
                    while (home_control.state == HOME_FAST_SEARCH && 
                           (current_time - home_control.start_time) * portTICK_PERIOD_MS < HOMING_TIMEOUT_MS) {
                        
                        if (home_switch_activated()) {
                            printf("🎯 Fim de curso encontrado na busca rápida!\n");
                            printf("   Posição do encoder: %d\n", encoder_get_position());
                            parar_movimento();
                            vTaskDelay(pdMS_TO_TICKS(100));
                            home_control.state = HOME_BACK_OFF;
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            printf("❌ Busca rápida completada sem encontrar home\n");
                            home_control.state = HOME_ERROR;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                        current_time = xTaskGetTickCount();
                    }
                    
                    if (home_control.state == HOME_FAST_SEARCH) {
                        printf("❌ Timeout na busca rápida\n");
                        home_control.state = HOME_ERROR;
                    }
                    break;
                }
                
                case HOME_BACK_OFF: {
                    printf("🔄 FASE 2: Recuando do fim de curso\n");
                    printf("   Posição atual: %d\n", encoder_get_position());
                    
                    // Recuar na direção positiva                    
                    // Aguardar movimento completar ou sair do fim de curso
                    uint32_t backoff_start = xTaskGetTickCount();
                    while (home_control.state == HOME_BACK_OFF && 
                           (xTaskGetTickCount() - backoff_start) * portTICK_PERIOD_MS < 5000) {
                        
                        if (!home_switch_activated()) {
                            printf("✅ Saiu do fim de curso\n");
                            printf("   Posição do encoder: %d\n", encoder_get_position());
                            parar_movimento();
                            vTaskDelay(pdMS_TO_TICKS(100));
                            home_control.state = HOME_SLOW_SEARCH;
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            home_control.state = HOME_SLOW_SEARCH;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    
                    if (home_control.state == HOME_BACK_OFF) {
                        home_control.state = HOME_SLOW_SEARCH;
                    }
                    break;
                }
                
                case HOME_SLOW_SEARCH: {
                    printf("🎯 FASE 3: Busca lenta final\n");
                    printf("   Posição atual: %d\n", encoder_get_position());
                    
                    // Mover lentamente para encontrar home com precisão
                    
                    uint32_t slow_start = xTaskGetTickCount();
                    while (home_control.state == HOME_SLOW_SEARCH && 
                           (xTaskGetTickCount() - slow_start) * portTICK_PERIOD_MS < 5000) {
                        
                        if (home_switch_activated()) {
                            printf("🎯 Home encontrado com precisão!\n");
                            parar_movimento();
                            vTaskDelay(pdMS_TO_TICKS(100));
                            
                            // ZERAR POSIÇÃO ABSOLUTA NO ENCODER
                            encoder_reset_position();
                            
                            if (xSemaphoreTake(xMutexHome, portMAX_DELAY)) {
                                home_control.home_position = 0;
                                home_control.home_found = true;
                                home_control.state = HOME_COMPLETE;
                                xSemaphoreGive(xMutexHome);
                            }
                            
                            printf("🎉 HOME CONCLUÍDO! Posição absoluta: 0\n");
                            break;
                        }
                        
                        if (MotionComplete(0)) {
                            printf("❌ Busca lenta completada sem encontrar home\n");
                            home_control.state = HOME_ERROR;
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    
                    if (home_control.state == HOME_SLOW_SEARCH) {
                        printf("❌ Timeout na busca lenta\n");
                        home_control.state = HOME_ERROR;
                    }
                    break;
                }
                
                case HOME_COMPLETE: {
                    home_control.homing_in_progress = false;
                    printf("✅ Sequência de home finalizada com sucesso\n");
                    printf("   Posição final do encoder: %d\n", encoder_get_position());
                    break;
                }
                
                case HOME_ERROR: {
                    parar_movimento();
                    home_control.homing_in_progress = false;
                    printf("❌ Erro na sequência de home\n");
                    printf("   Posição final do encoder: %d\n", encoder_get_position());
                    break;
                }
                
                default:
                    break;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Executar a 20Hz
    }
}
*/