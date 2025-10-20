// No início do arquivo, após os includes
#include "../functions/motion.h"

// Na inicialização do seu sistema
void inicializar_sistema(void) {
    // Suas inicializações existentes...
    TimerInitPulse();
    InitMotionControl(0);  // Inicializa apenas o eixo 0
    
    // Inicializar sistema de home
    home_init();
    
    // Criar tarefa do home
    xTaskCreate(home_task, "Home Task", 4096, NULL, 3, NULL);
    
    printf("✅ Sistema inicializado com função home\n");
}

// Exemplo de uso:
void exemplo_uso_home(void) {
    // Iniciar home
    iniciar_home();
    
    // Aguardar home completar
    while (!sistema_referenciado() && home_em_andamento()) {
        vTaskDelay(pdMS_TO_TICKS(100));
        imprimir_estado_home(); // Opcional: para debug
    }
    
    if (sistema_referenciado()) {
        printf("🎉 Sistema referenciado! Posição atual: %ld\n", home_get_position());
        
        MotorControlAbs(0, 5000, 1000, 1500, 1);
    } else {
        printf("❌ Falha no referenciamento\n");
    }
}

// Comando para parar home se necessário
void comando_emergencia(void) {
    parar_home();
}